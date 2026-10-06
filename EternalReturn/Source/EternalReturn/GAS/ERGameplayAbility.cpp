// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "EternalReturn.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERCooldownEffect.h"
#include "GAS/ERCostEffect.h"
#include "GAS/ERSkillPhaseEffect.h"
#include "Combat/ERProjectileBase.h"
#include "Combat/ERTargeting.h"
#include "Core/ERTeamStatics.h"
#include "Core/ERPlayerState.h"
#include "TimerManager.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "DrawDebugHelpers.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "GAS/Fragment/ERSkillFragment_TeamSplit.h"
#include "GAS/Delivery/ERSkillDelivery.h"
#include "GAS/Shape/ERSkillShape.h"
#include "Presentation/ERPresentationComponent.h"
#include "Presentation/ERPresentationData.h"

/** 눈으로 보는 디버그 — 판정 형상 · 적중 · 모드 상태를 서버 월드에 그린다 (리슨 서버 창). 0 = 끔. */
static TAutoConsoleVariable<int32> CVarSkillDebugDraw(TEXT("ER.Skill.DebugDraw"), 0,
	TEXT("스킬 판정 형상(초록) · 적중(빨강) · 모드 상태 글자를 1.5초 동안 그린다. 1 = 켬"));

namespace
{
	constexpr float DebugDrawSeconds = 1.5f;

	void DrawModeText(const AActor* Avatar, const FString& Text, FColor Color = FColor::Cyan)
	{
		if (!Avatar || CVarSkillDebugDraw.GetValueOnGameThread() == 0)
		{
			return;
		}
		DrawDebugString(Avatar->GetWorld(), Avatar->GetActorLocation() + FVector(0, 0, 160.f), Text, nullptr, Color, DebugDrawSeconds, true);
	}
}
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"
#include "Wildlife/ERWildlifeAIController.h"

UERGameplayAbility::UERGameplayAbility()
{
	// ⭐ 액터당 인스턴스 하나. 채널링·리캐스트 같은 **상태**를 들 수 있다.
	//   엔진 기본은 InstancedPerExecution(GameplayAbility.cpp:84)인데 그건 실행마다
	//   새로 만들어서 상태를 못 든다. Lyra 도 PerActor 다 (LyraGameplayAbility.cpp:40).
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// ⭐⭐ **서버가 시작하고, 클라는 따라 실행한다.**
	//   "This ability is initiated by the server, but will also run on the local client if one exists"
	//   (GameplayAbilityTypes.h:68-69)
	//
	//   흐름: 클라 TryActivate -> 서버로 요청 -> 서버가 실행 -> 클라도 실행(연출용)
	//
	// ⚠ Lyra 는 LocalPredicted 다 (LyraGameplayAbility.cpp:41) — 슈터라 반응성이 우선이다.
	//   우리는 **예측을 켜지 않는다** (CLAUDE.md §8 — "먼저 서버 권위로 정확히 동작시킨다").
	//   예측이 필요해지면 그때 이 값만 바꾸면 된다. 구조는 같다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// 어빌리티 객체 자체는 복제하지 않는다. ASC 가 스펙을 복제한다. Lyra 와 같다.
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;

	// ⭐ 전 스킬 공용 쿨다운 GE. 애셋에서 바꿀 일이 없다 — 수치는 UERSkillData.Cooldowns 다.
	CooldownGameplayEffectClass = UERCooldownEffect::StaticClass();

	// ⭐ 차단 태그를 베이스에 둔다 — 애셋마다 넣다가 빠뜨리면 조용히 안 막힌다 (F06-01 은 애셋에 넣었었다).
	//   State.Block.Skill : CC (기절 · 침묵)          State.Recovering : 다른 스킬의 후딜
	//   애셋이 더 붙이는 건 자유다 (BP Class Defaults 는 여기에 **추가**된다).
	ActivationBlockedTags.AddTag(ERTags::State_Block_Skill);
	ActivationBlockedTags.AddTag(ERTags::State_Recovering);
	ActivationBlockedTags.AddTag(ERTags::State_Unarmed);   // 무기 없음 — 평타 포함 전부 (F11-02, 원작 확인)
}

// ─────────────────────────────────────────────────────────────
// 파이프라인
// ─────────────────────────────────────────────────────────────

void UERGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// ⚠ Super 를 부르지 않는다 — Super 는 BP 의 K2_ActivateAbility 로 넘기는 분기뿐이다 (GameplayAbility.cpp:786-800).
	//   BP 가 ActivateAbility 를 오버라이드했다면 여기 자체가 안 불린다.
	bCastInterruptedByCC = false;

	const UERSkillData* Skill = GetSkillData(Handle, ActorInfo);
	if (!Skill)
	{
		// GetSkillData 가 이미 Error 로그를 남겼다.
		constexpr bool bReplicateEndAbility = true;
		constexpr bool bWasCancelled = true;
		EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		return;
	}

	// [3] 조준 확정 — 발동 요청에 실려 온 좌표를 서버가 클램프. 클라 인스턴스도 같은 데이터를 받아 같은 값을 갖는다(연출용).
	ResolveAim(TriggerEventData, *Skill);

	// 시전 시작 때 자리를 저장하는 모양 (PlayerCircles — F12.6-05 VF 방출). 기본 모양은 아무것도 안 한다
	CircleAimPoints.Reset();
	if (Skill->Area && HasAuthority(&ActivationInfo))
	{
		Skill->Area->CaptureAtCastStart(MakeShapeContext(*Skill, AimPoint, AimDirection, AimActor.Get()), CircleAimPoints);
	}

	// 리캐스트 — 윈도우가 열려 있었으면 이번 발동이 그것을 **소비**한다. 쿨다운은 새로 안 건다 (Argument 19 ③A, 자체 결정값).
	//   ⚠ 여기서 지우면 안 된다 — CommitAbility → CheckCooldown 이 태그를 못 봐 "쿨다운 중" 으로 실패한다
	//     (2026-09-14 로그 "리캐스트 (윈도우 소비)" 직후 "커밋 실패"). 커밋이 성공한 뒤 ExecuteAndRecover 에서 지운다.
	bActivatedByRecast = IsRecastWindowOpen();

	// 조각 CanExecute — 커밋 전. 실행될 데이터(리캐스트면 리캐스트 조각의 데이터)의 조각이 거부하면 **없던 일** —
	//   쿨다운 · 리캐스트 창 소비 없음 (단검 블링크 "대상 없음 · 사거리 밖", 사용자 확인 2026-09-20).
	{
		const UERSkillData* WillExec = Skill;
		if (bActivatedByRecast)
		{
			for (const TObjectPtr<UERSkillFragment>& F : Skill->Fragments)
			{
				if (F && F->GetRecastExecData()) { WillExec = F->GetRecastExecData(); break; }
			}
		}
		FString Reason;
		if (const UERSkillFragment* Blocking = FindFragmentBlocking(MakeContext(WillExec, 1.f), Reason))
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 보류 — [%s] %s (쿨다운 · 창 소비 없음)"),
				*GetNameSafe(GetOwningActorFromActorInfo()), *GetNameSafe(WillExec), *Blocking->GetDebugName(), *Reason);
			EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
			return;
		}
	}

	// ⭐ 대상 지정(SingleTarget) — 사거리 안에 유효한 대상이 없으면 **없던 일** (쿨다운 · 모션 없음).
	//   빈 땅을 찍어도 평타가 나가던 문제 (사용자 2026-09-28). 판정과 같은 질의(조준 보조 포함)로 미리 본다.
	//   ⏸ 사거리 밖 대상에게 다가가서 치기(원작 우클릭 · A 공격 명령)는 F17.
	if (Skill->Area && Skill->Area->RequiresTargetToActivate())
	{
		const AActor* Avatar = GetAvatarActorFromActorInfo();
		const FTargetQuery Q = MakeTargetQuery(*Skill);
		if (!Avatar || ERTargeting::Query(Avatar->GetWorld(), Q).IsEmpty())
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 보류 — 대상 없음 · 사거리 밖 (대상 %s · 거리 %.1fcm · 사거리 %.0fcm · 몸 Yaw %.0f — 조준 회전 뒤) · 쿨다운 · 모션 없음"),
				*GetNameSafe(GetOwningActorFromActorInfo()), *GetNameSafe(Skill), *GetNameSafe(Q.DesignatedTarget),
				Q.DesignatedTarget && Avatar ? ERTargeting::SingleTargetDistance(Avatar, Q.Origin, Q.DesignatedTarget, true) : -1.f, Q.RangeMax * 100.f,
				Avatar ? Avatar->GetActorRotation().Yaw : 0.f);
			EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
			return;
		}
	}

	// 연출 — 선딜 시작(또는 즉발) 순간. 판정 타이머와 따로 돈다 (Argument 36 "판정은 애니에 걸지 않는다").
	PlaySkillAnim(*Skill);

	if (Skill->CastTime > 0.f)
	{
		// 조각 OnCastStart — 충전 중 자기 효과 (F12.6-06 「격리」). 서버만
		if (HasAuthority(&ActivationInfo))
		{
			FERSkillContext CastCtx = MakeContext(Skill, 1.f);
			for (const TObjectPtr<UERSkillFragment>& F : Skill->Fragments)
			{
				if (F) { F->OnCastStart(CastCtx); }
			}
		}
		BeginCast(*Skill);       // [2] -> OnCastFinished -> ExecuteAndRecover
	}
	else
	{
		ExecuteAndRecover();     // 즉발: [4] -> [5]
	}
}

void UERGameplayAbility::PlaySkillAnim(const UERSkillData& Skill, bool bExecutePhase)
{
	// 서버 = 복제 원천 · 소유 클라 = 엔진이 복제 재생을 안 해 주니 직접 (ServerInitiated 라 RTT/2 늦다 — 예측 안 함, CLAUDE.md §8).
	if (!HasAuthority(&CurrentActivationInfo) && !IsLocallyControlled())
	{
		return;
	}
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UERPresentationComponent* Pres = Avatar ? Avatar->FindComponentByClass<UERPresentationComponent>() : nullptr;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Pres || !ASC)
	{
		return;
	}
	const FGameplayTag Slot = GetSlotTag();
	// 평타만 공속으로 빨라진다 (쿨다운 = 1/공속 · ERBasicAttackAbility.h).
	const float AttackSpeed = Slot == ERTags::Ability_Slot_Attack ? ASC->GetNumericAttribute(UERAttributeSet::GetAttackSpeedAttribute()) : 0.f;
	// 리캐스트(재입력) 발동이면 `<슬롯>.Recast` 를 먼저 — 동작표에 없으면 슬롯 키 (사용자 2026-09-29 단검 D)
	FGameplayTag Key = Slot;
	// 강화를 소비하는 평타 — 전용 애니가 있으면 그것 (재키 W skill02_attack · 모드 중이면 Saw_ · Argument 66 E1). 소리의 강화 판단과 같은 태그
	if (Slot == ERTags::Ability_Slot_Attack && ASC->HasMatchingGameplayTag(ERTags::State_NextAttackBuff) && Pres->HasKey(ERTags::Pres_Anim_AttackEnhanced))
	{
		Key = ERTags::Pres_Anim_AttackEnhanced;
	}
	if (bActivatedByRecast)
	{
		static const TMap<FGameplayTag, FGameplayTag> RecastKeys = {
			{ ERTags::Ability_Slot_Q, ERTags::Ability_Slot_Q_Recast }, { ERTags::Ability_Slot_W, ERTags::Ability_Slot_W_Recast },
			{ ERTags::Ability_Slot_E, ERTags::Ability_Slot_E_Recast }, { ERTags::Ability_Slot_R, ERTags::Ability_Slot_R_Recast },
			{ ERTags::Ability_Slot_D, ERTags::Ability_Slot_D_Recast },
		};
		if (const FGameplayTag* RecastKey = RecastKeys.Find(Slot); RecastKey && Pres->HasKey(*RecastKey))
		{
			Key = *RecastKey;
		}
	}
	// 판정 순간의 두 번째 단계 (F12.6-04 멧돼지 차징 → 돌진) — 몽타주 Execute 섹션 · 없으면 `<슬롯>.Execute` 줄 · 둘 다 없으면 시작 애니가 끝까지
	if (bExecutePhase)
	{
		// 발사 방식이 섹션을 넘기는 스킬 (순차 사격 — 발마다 조준 → 사격) 은 판정 순간에 아무것도 안 튼다 (K8)
		if (Skill.Delivery && Skill.Delivery->DrivesAnimSections())
		{
			return;
		}
		static const TMap<FGameplayTag, FGameplayTag> ExecuteKeys = {
			{ ERTags::Ability_Slot_Q, ERTags::Ability_Slot_Q_Execute }, { ERTags::Ability_Slot_W, ERTags::Ability_Slot_W_Execute },
			{ ERTags::Ability_Slot_E, ERTags::Ability_Slot_E_Execute }, { ERTags::Ability_Slot_R, ERTags::Ability_Slot_R_Execute },
			{ ERTags::Ability_Slot_D, ERTags::Ability_Slot_D_Execute },
		};
		// 스킬 몽타주에 Execute 섹션이 있으면 판정 순간 그 섹션으로 — 모든 스킬 (순차 사격은 1발째 · 2발째부터는 발사 방식이) · 서버 = 복제 · 소유 클라 = 로컬 (K8)
		if (Pres->FindSkillMontage(Slot, ERPresSection::Execute))
		{
			Pres->JumpSkillSection(Slot, ERPresSection::Execute);
			return;
		}
		const FGameplayTag* ExecKey = ExecuteKeys.Find(Slot);
		if (!ExecKey || !Pres->HasKey(*ExecKey))
		{
			return;
		}
		Pres->PlayAbilityAnim(this, CurrentActivationInfo, *ExecKey, 0.f, Skill.RecoveryTime);
		return;
	}
	// 선딜 있는 스킬은 애니의 "ER 타격 지점" 마커가 CastTime 에 오게 재생 속도를 맞춘다 (Argument 52 T2 · 마커 없으면 1배속)
	Pres->PlayAbilityAnim(this, CurrentActivationInfo, Key, AttackSpeed, Skill.CastTime + Skill.RecoveryTime, Skill.CastTime);
}

void UERGameplayAbility::BeginCast(const UERSkillData& Skill)
{
	// 선딜 태그. 이동 취소형이 아니면 이동을 막는다 — F06 의 State.Block.Movement 배선을 그대로 탄다.
	FGameplayTagContainer PhaseTags;
	PhaseTags.AddTag(ERTags::State_Casting);
	if (!Skill.bMoveCancelsCast)
	{
		PhaseTags.AddTag(ERTags::State_Block_Movement);
	}
	ApplyPhaseEffect(Skill.CastTime, PhaseTags);

	// 시간 — UAbilityTask. 자체 타이머 아님.
	UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(this, Skill.CastTime);
	Wait->OnFinish.AddDynamic(this, &UERGameplayAbility::OnCastFinished);
	Wait->ReadyForActivation();

	// ⭐ CC 취소 — "태그가 어디서 오든" 끊긴다 (Docs/4_Argument/17 ②A).
	//   F06-03 이 전제한 "GE 의 CancelAbilitiesWithTag" 는 GE 에 없다 — GE 는 새 발동을 막을 뿐이다 (GameplayEffect.cpp:4279).
	UAbilityTask_WaitGameplayTagAdded* WaitCC = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(
		this, ERTags::State_Block_Skill, /*OptionalExternalTarget=*/nullptr, /*OnlyTriggerOnce=*/true);
	WaitCC->Added.AddDynamic(this, &UERGameplayAbility::OnCastInterruptedByCC);
	WaitCC->ReadyForActivation();
	CastCCTask = WaitCC;   // 선딜이 끝나면 멈춘다 (OnCastFinished) — 안 멈추면 판정 · 후딜 중 CC 도 "선딜 취소" 로 잡힌다 (E33)

	// 이동 취소형 — 서버가 이동 명령을 수락하면 PC 가 Event.Input.Move 를 보낸다 (ERPlayerController::ServerSetDestination).
	if (Skill.bMoveCancelsCast)
	{
		UAbilityTask_WaitGameplayEvent* WaitMove = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, ERTags::Event_Input_Move, nullptr, /*OnlyTriggerOnce=*/true);
		WaitMove->EventReceived.AddDynamic(this, &UERGameplayAbility::OnCastInterruptedByMove);
		WaitMove->ReadyForActivation();
	}

	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 선딜 %.2f초 시작 (이동 %s)"),
		*GetNameSafe(GetOwningActorFromActorInfo()), *GetNameSafe(&Skill), Skill.CastTime,
		Skill.bMoveCancelsCast ? TEXT("취소형") : TEXT("차단"));
}

void UERGameplayAbility::OnCastFinished()
{
	// ⭐ 선딜 CC 감시는 여기까지 (E33). 판정 중 자기에게 거는 기절(좌절)이 "선딜 취소" 로 잡혀 쿨다운을 다시 걸고 벌칙이 두 번 걸렸다.
	//   판정 뒤 CC 는 후딜 감시(BeginRecovery)가 모션만 끊는다.
	if (UAbilityTask_WaitGameplayTagAdded* Task = CastCCTask.Get())
	{
		Task->EndTask();
	}
	CastCCTask.Reset();
	RemovePhaseEffect();
	ExecuteAndRecover();
}

void UERGameplayAbility::OnCastInterruptedByCC()
{
	bCastInterruptedByCC = true;   // 좌절 조각이 EndAbility 에서 읽는다 (F12.6-03)
	CancelCast(TEXT("CC"));
}

void UERGameplayAbility::OnCastInterruptedByMove(FGameplayEventData Payload)
{
	CancelCast(TEXT("이동 입력"));
}

void UERGameplayAbility::CancelCast(const TCHAR* Reason)
{
	if (!IsActive())
	{
		return;
	}

	const bool bAuthority = HasAuthority(&CurrentActivationInfo);

	// ⭐ §5.1 — 선딜 중 취소: **쿨다운은 정상 진행, 코스트는 안 든다** (발동 전이라 환불이 아니라 미차감).
	//   자체 결정값 — 원작 (미확인). 원작이 다르면 역기획서 §5.1 과 함께 고친다.
	//   ForceCooldown=true: CheckCooldown 을 건너뛴다 (이미 도는 쿨은 없지만 명시적으로).
	if (bAuthority)
	{
		CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*ForceCooldown=*/true);
	}

	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 선딜 취소 (%s, %s)"),
		*GetNameSafe(GetOwningActorFromActorInfo()), *GetNameSafe(GetSkillData(CurrentSpecHandle, CurrentActorInfo)),
		Reason, bAuthority ? TEXT("서버 · 쿨다운 커밋") : TEXT("클라"));

	// 서버가 취소하면 클라로 복제된다 (GameplayAbility.cpp:614-640). 클라는 복제된 태그로 먼저 끊길 수 있는데,
	// 그때는 자기 인스턴스만 정리한다 — 서버로 되보내지 않는다.
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateCancelAbility=*/bAuthority);
}

void UERGameplayAbility::ExecuteAndRecover()
{
	// [4] 코스트 + 쿨다운 + 로직 — **서버만.** 여기가 "발동 직전" 이다 (역기획서 §3 "선딜 시작 시점이 아님").
	//
	// ⚠ 클라는 커밋하지 않는다. 클라 인스턴스도 같은 파이프라인을 따라오지만(ServerInitiated) 클라의 GE 적용은
	//   엔진이 버리고(E12), 서버가 먼저 건 쿨다운 태그가 복제되어 오면 클라의 CheckCooldown 이 **실패**한다 —
	//   그러면 연출 훅이 안 불린다 (2026-09-13 로그 "선딜 취소 (커밋 실패, 클라)"). 서버가 진실, 클라는 따라간다.
	if (HasAuthority(&CurrentActivationInfo))
	{
		if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
		{
			// 선딜 사이에 VP 가 빠졌거나 CC 가 들어온 경우. 취소와 같이 다룬다 (쿨다운은 CommitAbility 가 못 걸었으니 여기서).
			CancelCast(TEXT("커밋 실패"));
			return;
		}

		// 리캐스트 윈도우 소비 — 커밋(CheckCooldown 우회)이 끝난 **뒤**.
		if (bActivatedByRecast && RecastTag.IsValid())
		{
			if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo_Ensured())
			{
				FGameplayTagContainer RecastQuery; RecastQuery.AddTag(RecastTag);
				ASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(RecastQuery));
			}
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 리캐스트 (윈도우 소비)"),
				*GetNameSafe(GetOwningActorFromActorInfo()), *GetNameSafe(GetSkillData(CurrentSpecHandle, CurrentActorInfo)));
		}

		// 리캐스트 발동에 리캐스트 조각이 다른 데이터를 지정했으면 이번 실행은 그 데이터 (단검 망토 → 단검, F11-05 C).
		const UERSkillData* Own = GetSkillData(CurrentSpecHandle, CurrentActorInfo);
		ExecOverride = nullptr;
		if (bActivatedByRecast && Own)
		{
			for (const TObjectPtr<UERSkillFragment>& F : Own->Fragments)
			{
				if (F && F->GetRecastExecData()) { ExecOverride = F->GetRecastExecData(); break; }
			}
		}

		// [4] 조각 파이프라인 — OnExecute(자기 이동 · 자기 버프 · 장판 · 모드) → 판정 → OnTargetsResolved(피해 · 적중 효과 · 넉백 · 2차 · 리캐스트 · 강화)
		ExecCtx = MakeContext(GetExecSkill(), 1.f);
		RunFragmentsExecute(ExecCtx);
		ExecuteSkill();
		ExecOverride = nullptr;
	}

	// 연출 훅은 양쪽.
	K2_OnSkillExecuted();
	// 판정 순간 (서버 + 소유 클라 — 시작 애니와 같은 규칙):
	//   ① 선딜을 맞추느라 바꾼 재생 속도를 1배속으로 — 후딜은 원래 속도 (Argument 52 T2 개선)
	//   ② `<슬롯>.Execute` 줄이 있으면 두 번째 애니
	if (const UERSkillData* ExecAnimSkill = GetSkillData(CurrentSpecHandle, CurrentActorInfo); ExecAnimSkill && ExecAnimSkill->CastTime > 0.f
		&& (HasAuthority(&CurrentActivationInfo) || IsLocallyControlled()))
	{
		if (AActor* Avatar = GetAvatarActorFromActorInfo())
		{
			if (UERPresentationComponent* Pres = Avatar->FindComponentByClass<UERPresentationComponent>())
			{
				Pres->RestoreSkillAnimRate(this);
			}
		}
		PlaySkillAnim(*ExecAnimSkill, /*bExecutePhase=*/true);
	}

	const UERSkillData* Skill = GetSkillData(CurrentSpecHandle, CurrentActorInfo);

	// 로컬 조각 (모드 카메라 · 이동 정지). 소유 클라 · 리슨 호스트.
	if (IsLocallyControlled())
	{
		FERSkillContext LocalCtx = MakeContext(Skill, 1.f);
		RunFragmentsLocal(LocalCtx);
	}
	// 조각이 활성 유지를 요청했으면(모드) 여기서 멈춘다. 서버는 그 조각이 EndFromFragment 로, 클라는 서버의 EndAbility 복제로 끝난다.
	if (HasAuthority(&CurrentActivationInfo))
	{
		bFragmentKeepActive = ExecCtx.bKeepActive;
	}
	else if (Skill)
	{
		for (const TObjectPtr<UERSkillFragment>& F : Skill->Fragments)
		{
			if (F && F->KeepsAbilityActive()) { bFragmentKeepActive = true; break; }
		}
	}
	if (bFragmentKeepActive)
	{
		return;
	}

	if (Skill && Skill->RecoveryTime > 0.f)
	{
		BeginRecovery(*Skill);   // [5] -> OnRecoveryFinished -> EndAbility
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
	}
}

void UERGameplayAbility::BeginRecovery(const UERSkillData& Skill)
{
	FGameplayTagContainer PhaseTags;
	PhaseTags.AddTag(ERTags::State_Recovering);
	ApplyPhaseEffect(Skill.RecoveryTime, PhaseTags);

	UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(this, Skill.RecoveryTime);
	Wait->OnFinish.AddDynamic(this, &UERGameplayAbility::OnRecoveryFinished);
	Wait->ReadyForActivation();

	// ⭐ 후딜은 이동 입력이 끝낸다 (§5.1 "후딜 중 이동 = 애니메이션 캔슬"). 자체 결정값.
	UAbilityTask_WaitGameplayEvent* WaitMove = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, ERTags::Event_Input_Move, nullptr, /*OnlyTriggerOnce=*/true);
	WaitMove->EventReceived.AddDynamic(this, &UERGameplayAbility::OnRecoveryCancelledByMove);
	WaitMove->ReadyForActivation();

	// 후딜 중 CC — 모션만 끊는다 (스킬은 이미 발동 · 쿨다운 다시 안 건다). 선딜 감시가 여기까지 이어지던 동작을 나눈 것 (E33).
	UAbilityTask_WaitGameplayTagAdded* WaitCC = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(
		this, ERTags::State_Block_Skill, /*OptionalExternalTarget=*/nullptr, /*OnlyTriggerOnce=*/true);
	WaitCC->Added.AddDynamic(this, &UERGameplayAbility::OnRecoveryInterruptedByCC);
	WaitCC->ReadyForActivation();
}

void UERGameplayAbility::OnRecoveryInterruptedByCC()
{
	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 후딜 중 CC — 모션만 끊음"),
		*GetNameSafe(GetOwningActorFromActorInfo()), *GetNameSafe(GetSkillData(CurrentSpecHandle, CurrentActorInfo)));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
}

void UERGameplayAbility::OnRecoveryFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UERGameplayAbility::OnRecoveryCancelledByMove(FGameplayEventData Payload)
{
	UE_LOG(LogEternalReturn, Verbose, TEXT("[스킬] %s 후딜을 이동으로 끊음"), *GetNameSafe(GetOwningActorFromActorInfo()));
	// 취소가 아니다 — 스킬은 이미 발동됐다. 후딜만 일찍 끝난다.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

// ─────────────────────────────────────────────────────────────
// [3] 조준 · [4] 판정 · 피해
// ─────────────────────────────────────────────────────────────

void UERGameplayAbility::ResolveAim(const FGameplayEventData* TriggerEventData, const UERSkillData& Skill)
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const FVector Origin = Avatar ? ERTargeting::GetTargetingLocation(Avatar) : FVector::ZeroVector;
	const FVector Forward = Avatar ? Avatar->GetActorForwardVector() : FVector::ForwardVector;

	AimActor = nullptr;
	AimPoint = Origin + Forward * GetRangeMax(Skill) * 100.f;
	AimDirection = Forward;

	// 조준 데이터가 실려 왔으면 그걸 쓴다. Docs/4_Argument/18 — PC 가 FGameplayAbilityTargetData_SingleTargetHit 로 보낸다.
	if (TriggerEventData && TriggerEventData->TargetData.Num() > 0)
	{
		const FGameplayAbilityTargetData* Data = TriggerEventData->TargetData.Get(0);
		if (Data && Data->HasEndPoint())
		{
			AimPoint = Data->GetEndPoint();
			if (const FHitResult* Hit = Data->GetHitResult())
			{
				AimActor = Hit->GetActor();
			}
		}
		// 두 번째 점 (누른 자리 고정 · 방향 — 레니 R)
		bHasSecondAimPoint = false;
		if (TriggerEventData->TargetData.Num() > 1)
		{
			const FGameplayAbilityTargetData* Second = TriggerEventData->TargetData.Get(1);
			if (Second && Second->HasEndPoint())
			{
				SecondAimPoint = Second->GetEndPoint();
				bHasSecondAimPoint = true;
			}
		}
	}

	// ⭐ 서버 클램프 — 클라가 보낸 좌표를 믿지 않는다 (§4.2 "클램프 없으면 사거리 핵").
	//   Z 는 무시한다. 탑다운이라 판정도 bIgnoreZ 다 (F04).
	FVector ToAim = AimPoint - Origin;
	ToAim.Z = 0.f;
	const float Dist = ToAim.Size();
	const float MinUU = Skill.GetMinReach() * 100.f;
	const float MaxUU = GetRangeMax(Skill) * 100.f;

	if (Dist > KINDA_SMALL_NUMBER)
	{
		AimDirection = ToAim / Dist;
	}

	// F11-05 D: 모드 평타(저지사격 · 데드아이)는 진입 시 방향 ± AimHalfAngleDeg 안으로만 조준된다 (사용자 확인 2026-09-21 "좌우 30°").
	//   서버가 자른다 — 클라 커서는 자유지만 실제 발사 방향은 원뿔 경계로 붙는다. 조준점도 그 방향으로 다시 놓는다.
	if (const AERPlayerState* PS = Cast<AERPlayerState>(GetOwningActorFromActorInfo());
		PS && PS->GetModeAimHalfAngleDeg() > 0.f && PS->GetModeAttackHandle() == CurrentSpecHandle)
	{
		const FVector Center = PS->GetModeAimCenter();
		const float Half = PS->GetModeAimHalfAngleDeg();
		const float SignedDeg = FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(Center, AimDirection).Z, FVector::DotProduct(Center, AimDirection)));
		if (FMath::Abs(SignedDeg) > Half)
		{
			const FVector Clamped = Center.RotateAngleAxis(FMath::Sign(SignedDeg) * Half, FVector::UpVector);
			UE_LOG(LogEternalReturn, Verbose, TEXT("[스킬] %s 조준 각 %.0f° -> ±%.0f° 로 자름"), *GetNameSafe(&Skill), SignedDeg, Half);
			AimDirection = Clamped;
		}
	}
	const float ClampedDist = FMath::Clamp(Dist, MinUU, MaxUU);
	if (!FMath::IsNearlyEqual(ClampedDist, Dist))
	{
		UE_LOG(LogEternalReturn, Verbose, TEXT("[스킬] %s 조준 거리 %.0f -> %.0f (사거리 %.0f~%.0f)"),
			*GetNameSafe(&Skill), Dist, ClampedDist, MinUU, MaxUU);
	}
	AimPoint = Origin + AimDirection * ClampedDist;
	AimPoint.Z = Origin.Z;

	// ⭐ 몸을 조준 방향으로 돌린다 (사용자 요청 2026-09-20). 서버 · 소유 클라 양쪽에서 돌고, 다른 클라는 이동 복제로 받는다.
	//   Yaw 만 — 탑다운이라 Pitch/Roll 은 건드리지 않는다. 이동 중이면 CharacterMovement 가 다시 돌릴 수 있지만
	//   시전 페이즈에선 이동이 막히거나(Block.Movement) 취소되므로 문제 없다.
	if (AActor* MutableAvatar = GetAvatarActorFromActorInfo())
	{
		FVector Flat = AimDirection; Flat.Z = 0.f;
		if (Flat.SizeSquared() > KINDA_SMALL_NUMBER)
		{
			MutableAvatar->SetActorRotation(FRotator(0.f, Flat.Rotation().Yaw, 0.f));
		}
	}
}



const UERSkillData* UERGameplayAbility::GetExecSkill() const
{
	return ExecOverride ? ExecOverride : GetSkillData(CurrentSpecHandle, CurrentActorInfo);
}


FERShapeContext UERGameplayAbility::MakeShapeContext(const UERSkillData& SkillRef, const FVector& InAimPoint, const FVector& InAimDirection, AActor* Designated) const
{
	FERShapeContext Ctx;
	Ctx.Avatar = GetAvatarActorFromActorInfo();
	Ctx.AimPoint = InAimPoint;
	Ctx.AimDirection = InAimDirection;
	Ctx.DesignatedTarget = Designated;
	Ctx.Targets = &SkillRef.Targets;
	Ctx.CapturedPoints = &CircleAimPoints;
	Ctx.RangeOverride = GetRangeMax(SkillRef);   // 평타 = 무기 사거리 (F07-07) · 그 외 = 모양 GetMaxReach 그대로
	return Ctx;
}

FTargetQuery UERGameplayAbility::BuildQuery(const UERSkillData& SkillRef, const FVector& InAimPoint, const FVector& InAimDirection) const
{
	// 질의는 모양이 만든다 (S3.1) — 지정 대상 없음 (시전 전 미리보기)
	return SkillRef.Area ? SkillRef.Area->BuildQuery(MakeShapeContext(SkillRef, InAimPoint, InAimDirection, nullptr)) : FTargetQuery();
}

FTargetQuery UERGameplayAbility::MakeTargetQuery(const UERSkillData& SkillRef) const
{
	// 조준 보조(대상 하나)도 모양이 한다 — 판정과 발동 전 대상 확인이 같은 질의
	return SkillRef.Area ? SkillRef.Area->BuildQuery(MakeShapeContext(SkillRef, AimPoint, AimDirection, AimActor.Get())) : FTargetQuery();
}

void UERGameplayAbility::ExecuteSkill()
{
	const UERSkillData* Skill = GetExecSkill();
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Skill || !Avatar)
	{
		return;
	}

	// 조각이 판정을 대신했다 (장판).
	if (ExecCtx.bSkipTargeting)
	{
		return;
	}

	if (!Skill->Area || !Skill->Delivery)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[스킬] %s — 모양(Area) · 발사(Delivery) 가 없다. ER.Skill.Resave 를 했나 (S3.1)"), *GetNameSafe(Skill));
		return;
	}
	// ⭐ 발사 방식이 판정 · 발사를 정한다 (Argument 57 S3.1) — 즉시면 ResolveInstantHits 로 돌아온다 · 투사체면 적중은 도착 때
	const FTargetQuery Q = MakeTargetQuery(*Skill);
	Skill->Delivery->Deliver(*this, *Skill, Q, MakeShapeContext(*Skill, AimPoint, AimDirection, AimActor.Get()));
}

void UERGameplayAbility::ResolveInstantHits(const UERSkillData& SkillRef, const FTargetQuery& Q, const FTargetResult& Result, bool bAttackCue)
{
	const UERSkillData* Skill = &SkillRef;
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 판정 %s: 적중 %d (중앙 %d)"),
		*GetNameSafe(GetOwningActorFromActorInfo()), *GetNameSafe(Skill),
		*UEnum::GetValueAsString(Q.Shape), Result.HitActors.Num(), Result.InnerHitActors.Num());

	// SingleTarget 빗나감 — 왜인지 한 줄 (대상 · 거리 · 사거리 · 적대). 2026-09-20 단검 블링크 뒤 적중 0 진단용.
	if (Q.Shape == ESkillTargeting::SingleTarget && Result.HitActors.IsEmpty())
	{
		const AActor* T = Q.DesignatedTarget;
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬]   SingleTarget 빗나감 — 대상 %s · 거리 %.0fcm · 사거리 %.0f~%.0fcm · 적대 %s"),
			*GetNameSafe(T), T ? ERTargeting::SingleTargetDistance(Q.Instigator, Q.Origin, T, true) : -1.f,
			Q.RangeMin * 100.f, Q.RangeMax * 100.f, T && ERTeamStatics::IsHostile(Avatar, T) ? TEXT("O") : TEXT("X"));
	}

	// 적중 조각 (피해 · 적중 효과 · 넉백 · 2차 · 리캐스트 · 강화) — 가상 OnTargetsResolved **앞**: 평타는 "자기 피해 → 강화 소비" 순서.
	{
		TArray<AActor*> Targets;
		for (AActor* A : Result.HitActors)      { if (A) { Targets.Add(A); } }
		for (AActor* A : Result.InnerHitActors) { if (A) { Targets.Add(A); } }
		ExecCtx.Targets = Targets;
		ExecCtx.bHitAnything = !Targets.IsEmpty();
		RunFragmentsTargets(ExecCtx, Targets);

		// ⭐ 연출 큐 (F12.5-05 · Argument 49 W2) — **판정 시점 = 애니 타격 프레임** (선딜을 애니에 맞췄다 · 45). 소리 시점의 원천은 선딜 하나.
		//   서버만 보낸다 → GAS 가 모든 클라에 (데디 서버는 재생 안 함). 2차 판정(ExecuteOther)에는 공격음 없음 — 시전 한 번에 한 번.
		//   ⏸ 소유자 공격음 예측은 GAS 예측을 켤 때 (CLAUDE.md §8). 타격음은 예측하지 않는다 — 서버 확정만.
		if (HasAuthority(&CurrentActivationInfo))
		{
			SendPresCues(*Skill, Avatar, Targets, /*bWithAttack=*/bAttackCue && !bExecutingOther);
			// 야생동물이 누굴 맞혔다 — 돌아다니는 종(위클라인)은 그 자리가 새 기준 (F12.6-06 · 사용자 2026-10-01)
			if (!Targets.IsEmpty())
			{
				if (const APawn* Pawn = Cast<APawn>(Avatar))
				{
					if (AERWildlifeAIController* AI = Cast<AERWildlifeAIController>(Pawn->GetController()))
					{
						AI->NotifyDealtHit();
					}
				}
			}
		}
	}

	OnTargetsResolved(Result);
}

void UERGameplayAbility::SendPresCues(const UERSkillData& Skill, AActor* Avatar, const TArray<AActor*>& InTargets, bool bWithAttack, int32 ShotNumber, const FVector& FaceDirection) const
{
	// 적 · 아군 나누기 스킬 (레니) — 타격음은 적에게만 · 조각이 허락하면 아군도 (E 보호막 · 사용자 2026-10-07)
	TArray<AActor*> Targets = InTargets;
	if (const UERSkillFragment_TeamSplit* Split = Skill.FindFragment<UERSkillFragment_TeamSplit>(); Split && !Split->bAllyHitCue && Avatar)
	{
		Targets.RemoveAll([Avatar](const AActor* T) { return T == Avatar || !ERTeamStatics::IsHostile(Avatar, T); });
	}
	bWithAttack = bWithAttack && !Skill.bNoCastCue;
	FGameplayCueParameters Base;
	Base.Instigator = Avatar;                       // 소리는 시전자의 무기 · 스킨에서 찾는다
	Base.EffectCauser = Avatar;
	Base.SourceObject = &Skill;
	Base.AggregatedSourceTags.AddTag(Skill.SlotTag);   // 평타 / 스킬 구분 (키 Pres.Sfx.Attack · SkillCast …)
	// 슬롯 없는 하위 DA (매그너스 R_Explode · R_Launch · E_Wall) — 이 어빌리티 스킬의 슬롯 소리로 (F19-02 · 없으면 무기 공용 소리가 났다)
	if (!Skill.SlotTag.IsValid())
	{
		if (const UERSkillData* Own = GetSkillData())
		{
			Base.AggregatedSourceTags.AddTag(Own->SlotTag);
		}
	}
	// 재사용으로 발동 — 받는 쪽이 재사용 소리 키가 있으면 그것으로 (매그너스 R 바이크 발사 · F19-02)
	if (bActivatedByRecast && bWithAttack)
	{
		static const TMap<FGameplayTag, FGameplayTag> RecastOf = {
			{ ERTags::Ability_Slot_Q, ERTags::Ability_Slot_Q_Recast }, { ERTags::Ability_Slot_W, ERTags::Ability_Slot_W_Recast },
			{ ERTags::Ability_Slot_E, ERTags::Ability_Slot_E_Recast }, { ERTags::Ability_Slot_R, ERTags::Ability_Slot_R_Recast },
			{ ERTags::Ability_Slot_D, ERTags::Ability_Slot_D_Recast },
		};
		for (const TPair<FGameplayTag, FGameplayTag>& R : RecastOf)
		{
			if (Base.AggregatedSourceTags.HasTagExact(R.Key)) { Base.AggregatedSourceTags.AddTag(R.Value); break; }
		}
	}
	Base.RawMagnitude = static_cast<float>(ShotNumber);   // 순차 사격의 몇 번째 발 (카티야 R) — 2 이상이면 받는 쪽이 판정 순간 애니를 다시 튼다. 그 외 0
	// 이번 평타가 강화를 소비한다 (강화 대기 중 — 소비는 이 뒤 OnTargetsResolved · 투사체 평타는 도착 때) → 받는 쪽이 강화 소리 키로 (카티야 P Reinforce · K8)
	//   투사체 평타는 쏠 때 대상 목록이 비어 있다 — 그래서 대상 조건 없이 (즉시 평타는 대상이 없으면 발동 자체가 안 된다)
	if (Skill.SlotTag == ERTags::Ability_Slot_Attack)
	{
		const UAbilitySystemComponent* MyASC = GetAbilitySystemComponentFromActorInfo();
		if (MyASC && MyASC->HasMatchingGameplayTag(ERTags::State_NextAttackBuff))
		{
			Base.AggregatedSourceTags.AddTag(ERTags::State_NextAttackBuff);
		}
	}

	if (bWithAttack)
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			FGameplayCueParameters P = Base;
			P.Location = Avatar->GetActorLocation();
			P.Normal = FaceDirection;   // 쏘는 방향 (순차 사격 — 소유 클라가 몸을 돌린다 · 0 이면 안 돌림)
			ASC->ExecuteGameplayCue(ERTags::GameplayCue_Pres_Attack, P);
		}
	}
	for (AActor* T : Targets)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(T);
		if (!TargetASC)
		{
			continue;
		}
		// 타격 지점 — 대상 몸 표면의 시전자 쪽 (판정이 액터만 돌려준다 · HitResult 가 생기면 그 ImpactPoint 로)
		FGameplayCueParameters P = Base;
		const FVector ToCaster = (Avatar->GetActorLocation() - T->GetActorLocation()).GetSafeNormal2D();
		P.Location = T->GetActorLocation() + ToCaster * T->GetSimpleCollisionRadius();
		P.Normal = ToCaster;
		TargetASC->ExecuteGameplayCue(ERTags::GameplayCue_Pres_Hit, P);
	}
}

void UERGameplayAbility::JumpSkillSection(const UERSkillData& Skill, FName Section) const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UERPresentationComponent* Pres = Avatar ? Avatar->FindComponentByClass<UERPresentationComponent>() : nullptr;
	if (Pres && HasAuthority(&CurrentActivationInfo))
	{
		Pres->JumpSkillSection(Skill.SlotTag, Section);
	}
}

void UERGameplayAbility::SendEventCue(FGameplayTag CueTag, const UERSkillData& Skill, int32 ShotNumber) const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Avatar || !ASC || !HasAuthority(&CurrentActivationInfo))
	{
		return;
	}
	FGameplayCueParameters P;
	P.Instigator = Avatar;
	P.EffectCauser = Avatar;
	P.SourceObject = &Skill;
	P.AggregatedSourceTags.AddTag(Skill.SlotTag);
	P.RawMagnitude = static_cast<float>(ShotNumber);
	P.Location = Avatar->GetActorLocation();
	ASC->ExecuteGameplayCue(CueTag, P);
}

void UERGameplayAbility::ApplyOnTargets(const UERSkillData* Data, const TArray<AActor*>& Targets, float Scale, int32 LevelOverride, const UERSkillData* ShapeOwner, bool bEnhancement, float TravelRatio, int32 ShotIndex)
{
	if (!Data || Targets.IsEmpty() || !HasAuthority(&CurrentActivationInfo))
	{
		return;
	}
	FERSkillContext Ctx = MakeContext(Data, Scale);
	if (LevelOverride > 0) { Ctx.Level = LevelOverride; }
	Ctx.ShapeOwner = ShapeOwner;
	Ctx.bEnhancement = bEnhancement;
	Ctx.TravelRatio = TravelRatio;
	Ctx.ShotIndex = ShotIndex;
	Ctx.Targets = Targets;
	Ctx.bHitAnything = true;
	// ⚠ 대상 효과 조각(피해 · 적중 효과)만 — 리캐스트 · 강화 · 2차 판정은 "시전 결과" 라 여기서 돌면 안 된다 (평타 강화 무한 루프).
	for (const TObjectPtr<UERSkillFragment>& F : Data->Fragments)
	{
		if (F && F->IsHitEffect()) { F->OnTargetsResolved(Ctx, Targets); }
	}
}

void UERGameplayAbility::DrawPreview(const UERSkillData& SkillRef, const FVector& CursorPoint) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	// ResolveAim 과 같은 클램프 — 미리보기 위치 = 실제 판정 위치
	const FVector Origin = ERTargeting::GetTargetingLocation(Avatar);
	FVector ToAim = CursorPoint - Origin;
	ToAim.Z = 0.f;
	const float Dist = ToAim.Size();
	const FVector Dir = Dist > KINDA_SMALL_NUMBER ? ToAim / Dist : Avatar->GetActorForwardVector().GetSafeNormal2D();
	const float MaxUU = GetRangeMax(SkillRef) * 100.f;
	FVector Aim = Origin + Dir * FMath::Clamp(Dist, SkillRef.GetMinReach() * 100.f, MaxUU);
	Aim.Z = Origin.Z;
	// 출발점이 시전자가 아닌 스킬 (시셀라 Q — 떨어진 윌슨에서 조준점으로) — 그 점 → 조준점 화살표 + 시전자 사거리 원
	for (const TObjectPtr<UERSkillFragment>& F : SkillRef.Fragments)
	{
		FVector From;
		bool bFullReach = false;
		if (F && F->GetPreviewOrigin(Avatar, From, bFullReach))
		{
			From.Z = Origin.Z;
			FVector To = Aim;
			if (bFullReach)
			{
				// 윌슨에서 커서 쪽으로 사거리만큼 (E) — 시전자 사거리 원은 의미가 없다
				FVector D = CursorPoint - From;
				D.Z = 0.f;
				To = From + (D.IsNearlyZero() ? Dir : D.GetSafeNormal()) * MaxUU;
			}
			else
			{
				DrawDebugCircle(World, Origin + FVector(0.f, 0.f, 20.f), MaxUU, 48, FColor(255, 255, 255, 120), false, 0.f, 0, 1.f, FVector::RightVector, FVector::ForwardVector, false);
			}
			DrawDebugDirectionalArrow(World, From + FVector(0.f, 0.f, 20.f), To + FVector(0.f, 0.f, 20.f), 60.f, FColor::Cyan, false, 0.f, 0, 4.f);
			return;
		}
	}
	const FTargetQuery Q = BuildQuery(SkillRef, Aim, Dir);
	if (!SkillRef.Area)
	{
		return;
	}
	SkillRef.Area->Draw(World, Q, FColor::Cyan, 0.f);
	// 조준점을 당기는 모양(지면 원 · 사다리꼴)은 사거리 원도 — 커서가 밖이면 이 원 끝으로 붙는다
	if (SkillRef.Area->ShapeOrigin == EERShapeOrigin::AimPoint)
	{
		DrawDebugCircle(World, Origin + FVector(0.f, 0.f, 20.f), MaxUU, 48, FColor(255, 255, 255, 120), false, 0.f, 0, 1.f, FVector::RightVector, FVector::ForwardVector, false);
	}
}

AERProjectileBase* UERGameplayAbility::SpawnProjectile(const UERSkillData& SkillRef, TSubclassOf<AERProjectileBase> Class, const FERProjectileLaunch& L,
	const FTargetQuery& Filter, int32 Lv, int32 ShotIndex, bool bPierce)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World || !Class || !HasAuthority(&CurrentActivationInfo))
	{
		return nullptr;
	}
	const FTransform T(FVector(L.Direction).Rotation(), FVector(L.Start));
	AERProjectileBase* P = World->SpawnActorDeferred<AERProjectileBase>(Class, T, Avatar, Cast<APawn>(Avatar), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!P)
	{
		return nullptr;
	}
	P->InitLaunch(this, &SkillRef, Lv, Filter, L, ShotIndex, bPierce);
	P->FinishSpawning(T);
	return P;
}

void UERGameplayAbility::BeginDeferredHits(const UERSkillData& SkillRef, bool bAttackCue)
{
	// 적중과 무관한 조각(다음 평타 강화 등)은 지금 — 적중 조각은 빈 대상이라 아무것도 안 한다. "빗나감" 벌칙은 보류 (bHitDeferred)
	ExecCtx.Targets.Reset();
	ExecCtx.bHitAnything = false;
	ExecCtx.bHitDeferred = true;
	RunFragmentsTargets(ExecCtx, {});
	if (bAttackCue && HasAuthority(&CurrentActivationInfo))
	{
		SendPresCues(SkillRef, GetAvatarActorFromActorInfo(), {}, /*bWithAttack=*/true);   // 시전음은 발사 때 · 타격음은 도착 때
	}
}

void UERGameplayAbility::DebugDrawQuery(const UERSkillData& SkillRef, const FTargetQuery& Q, const FTargetResult& Result) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World || CVarSkillDebugDraw.GetValueOnGameThread() == 0)
	{
		return;
	}
	if (SkillRef.Area)
	{
		SkillRef.Area->Draw(World, Q, FColor::Green, DebugDrawSeconds);
	}
	const FVector O = Q.Origin + FVector(0.f, 0.f, 20.f);
	for (const AActor* Hit : Result.HitActors)
	{
		if (Hit) { DrawDebugSphere(World, Hit->GetActorLocation(), 45.f, 12, FColor::Red, false, DebugDrawSeconds, 0, 2.f); }
	}
	DrawDebugString(World, O + FVector(0, 0, 120.f), FString::Printf(TEXT("%s: 적중 %d"), *GetNameSafe(&SkillRef), Result.HitActors.Num()), nullptr,
		Result.HitActors.IsEmpty() ? FColor::Yellow : FColor::Red, DebugDrawSeconds, true);
}

void UERGameplayAbility::ApplyProjectileHit(const UERSkillData* Data, AActor* Target, int32 InLevel, float TravelRatio, int32 ShotIndex)
{
	if (!Data || !Target)
	{
		return;
	}
	ApplyOnTargets(Data, { Target }, 1.f, InLevel, nullptr, false, TravelRatio, ShotIndex);
	if (HasAuthority(&CurrentActivationInfo))
	{
		// 순차 사격이면 발 번호 — 타격음도 발마다 (Skill04_Hit → _02 → _03 · K8)
		SendPresCues(*Data, GetAvatarActorFromActorInfo(), { Target }, /*bWithAttack=*/false, /*ShotNumber=*/ShotIndex >= 0 ? ShotIndex + 1 : 0);
		OnProjectileTargetHit(*Data, Target);   // 타격음(강화 표시) 뒤에 — 평타 강화 소비 (Argument 60 T1)
	}
}

void UERGameplayAbility::DebugDrawText(const AActor* Avatar, const FString& Text, FColor Color)
{
	DrawModeText(Avatar, Text, Color);
}

// ─────────────────────────────────────────────────────────────
// F11.5 조각 — 문맥 · 훅 호출 · 조각이 부르는 것
// ─────────────────────────────────────────────────────────────

FERSkillContext UERGameplayAbility::MakeContext(const UERSkillData* Skill, float Scale) const
{
	FERSkillContext Ctx;
	Ctx.Ability = const_cast<UERGameplayAbility*>(this);
	Ctx.ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	Ctx.Avatar = GetAvatarActorFromActorInfo();
	Ctx.PlayerState = Cast<AERPlayerState>(GetOwningActorFromActorInfo());
	Ctx.Skill = Skill;
	Ctx.Level = GetAbilityLevel();
	Ctx.AimPoint = AimPoint;
	Ctx.AimDirection = AimDirection;
	Ctx.AimActor = AimActor.Get();
	Ctx.bActivatedByRecast = bActivatedByRecast;
	Ctx.bAuthority = HasAuthority(&CurrentActivationInfo);
	Ctx.DamageScale = Scale;
	return Ctx;
}

const UERSkillFragment* UERGameplayAbility::FindFragmentBlocking(const FERSkillContext& Ctx, FString& OutReason) const
{
	if (!Ctx.Skill) { return nullptr; }
	for (const TObjectPtr<UERSkillFragment>& F : Ctx.Skill->Fragments)
	{
		if (F && !F->CanExecute(Ctx, OutReason)) { return F.Get(); }
	}
	return nullptr;
}

void UERGameplayAbility::RunFragmentsExecute(FERSkillContext& Ctx)
{
	if (!Ctx.Skill) { return; }
	for (const TObjectPtr<UERSkillFragment>& F : Ctx.Skill->Fragments) { if (F) { F->OnExecute(Ctx); } }
}

void UERGameplayAbility::RunFragmentsTargets(FERSkillContext& Ctx, const TArray<AActor*>& Targets)
{
	if (!Ctx.Skill) { return; }
	for (const TObjectPtr<UERSkillFragment>& F : Ctx.Skill->Fragments) { if (F) { F->OnTargetsResolved(Ctx, Targets); } }
}

void UERGameplayAbility::RunFragmentsLocal(FERSkillContext& Ctx)
{
	if (!Ctx.Skill) { return; }
	for (const TObjectPtr<UERSkillFragment>& F : Ctx.Skill->Fragments) { if (F) { F->OnLocalExecute(Ctx); } }
}

void UERGameplayAbility::RunFragmentsEnd(FERSkillContext& Ctx, bool bCancelled)
{
	if (!Ctx.Skill) { return; }
	for (const TObjectPtr<UERSkillFragment>& F : Ctx.Skill->Fragments) { if (F) { F->OnEnd(Ctx, bCancelled); } }
}

void UERGameplayAbility::ExecuteOther(const UERSkillData* Other, float Scale, bool bWithExecuteHooks)
{
	if (!Other || !HasAuthority(&CurrentActivationInfo))
	{
		return;
	}
	// 바깥 실행의 문맥 · 오버라이드를 보존하고 잠시 바꾼다 (2차 판정이 끝나면 원래대로).
	const UERSkillData* SavedOverride = ExecOverride;
	const FERSkillContext SavedCtx = ExecCtx;
	const bool bSavedOther = bExecutingOther;

	ExecOverride = Other;
	bExecutingOther = true;
	ExecCtx = MakeContext(Other, Scale);
	if (bWithExecuteHooks) { RunFragmentsExecute(ExecCtx); }
	ExecuteSkill();

	ExecOverride = SavedOverride;
	ExecCtx = SavedCtx;
	bExecutingOther = bSavedOther;
}

void UERGameplayAbility::EndFromFragment(bool bCancelled)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, bCancelled);
}

UERSkillFragmentState* UERGameplayAbility::GetOrCreateFragmentState(const UERSkillFragment* Owner, TSubclassOf<UERSkillFragmentState> Class)
{
	if (!Owner || !Class)
	{
		return nullptr;
	}
	if (TObjectPtr<UERSkillFragmentState>* Found = FragmentStates.Find(Owner))
	{
		if (*Found && (*Found)->IsA(Class)) { return Found->Get(); }
	}
	UERSkillFragmentState* State = NewObject<UERSkillFragmentState>(this, Class);
	FragmentStates.Add(Owner, State);
	return State;
}

void FERSkillContext::ExecuteOther(const UERSkillData* Other, float Scale, bool bWithExecuteHooks) const
{
	if (Ability) { Ability->ExecuteOther(Other, Scale, bWithExecuteHooks); }
}

UERSkillFragmentState* FERSkillContext::GetStateInternal(const UERSkillFragment* Owner, TSubclassOf<UERSkillFragmentState> Class) const
{
	return Ability ? Ability->GetOrCreateFragmentState(Owner, Class) : nullptr;
}

void UERGameplayAbility::OnTargetsResolved(const FTargetResult& Result)
{
	// 기본은 아무것도 안 한다 — 대상 효과는 전부 조각 (ExecuteSkill 이 조각 훅을 먼저 돌린 뒤 여기로 온다).
	//   파생(평타)이 "강화 소비" 같은 어빌리티 고유 후처리를 여기에 둔다.
}





void UERGameplayAbility::ApplyPhaseEffect(float Duration, const FGameplayTagContainer& PhaseTags)
{
	// ⚠ 서버만. 클라의 GE 적용은 엔진이 버린다 (E12 · AbilitySystemComponent.cpp:380). 태그는 복제로 온다.
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	RemovePhaseEffect();

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, UERSkillPhaseEffect::StaticClass(), GetAbilityLevel());

	if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
	{
		Spec->DynamicGrantedTags.AppendTags(PhaseTags);
		Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_PhaseDuration, Duration);
		PhaseEffectHandle = ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
	}
}

void UERGameplayAbility::RemovePhaseEffect()
{
	if (!PhaseEffectHandle.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo_Ensured())
	{
		ASC->RemoveActiveGameplayEffect(PhaseEffectHandle);
	}
	PhaseEffectHandle.Invalidate();
}






void UERGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 어떤 경로로 끝나든(정상 · 취소 · 외부 CancelAbility) 페이즈 GE 를 남기지 않는다.
	RemovePhaseEffect();
	// 취소(CC · 이동 취소형 선딜)면 모션도 끊는다. 정상 종료는 후딜 모션이 끝까지 간다. 이 어빌리티가 재생 중일 때만 멈춘다 (UGameplayAbility::MontageStop).
	if (bWasCancelled)
	{
		MontageStop();
	}
	// 조각 OnEnd (모드 정리 · 쿨 반환 · 로컬 카메라 복구). 양쪽 — 조각이 Ctx.bAuthority 로 가른다. 두 번 불려도 조각이 알아서 무시한다.
	{
		FERSkillContext EndCtx = MakeContext(GetSkillData(Handle, ActorInfo), 1.f);
		RunFragmentsEnd(EndCtx, bWasCancelled);
		bFragmentKeepActive = false;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UERGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	// Super 가 CurrentActorInfo·Handle 을 세팅한다 (GameplayAbility.cpp — SetCurrentActorInfo).
	// 그래야 GetSlotTag -> GetCurrentSourceObject 가 스펙을 찾는다.
	Super::OnGiveAbility(ActorInfo, Spec);

	CooldownTags.Reset();
	RecastTag = FGameplayTag();

	const FGameplayTag SlotTag = GetSlotTag();
	if (SlotTag.IsValid())
	{
		// F11-04: D 는 무기 계열 태그로 (데이터의 CooldownTagOverride). 그 외는 슬롯 태그.
		const UERSkillData* Skill = GetSkillData();
		const FGameplayTag CooldownTag = (Skill && Skill->CooldownTagOverride.IsValid()) ? Skill->CooldownTagOverride : ERSkill::CooldownTagForSlot(SlotTag);
		if (CooldownTag.IsValid())
		{
			CooldownTags.AddTag(CooldownTag);
		}
		RecastTag = ERSkill::RecastTagForSlot(SlotTag);
	}
	// SlotTag 가 없으면 GetSkillData 가 이미 Error 로그를 남겼다. 쿨다운 없이 동작한다.
}

float UERGameplayAbility::GetRangeMax(const UERSkillData& Skill) const
{
	return Skill.GetMaxReach();
}

bool UERGameplayAbility::IsRecastWindowOpen() const
{
	const UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	return RecastTag.IsValid() && ASC && ASC->HasMatchingGameplayTag(RecastTag);
}

bool UERGameplayAbility::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	// ⭐ 리캐스트 윈도우 — "쿨다운 중인데 발동" 이 맞는 유일한 경우 (재키 Q). 태그는 복제되어 클라 선판정도 같은 답.
	//   ⚠ CDO 에서 불리면 RecastTag 가 비어 있어 그냥 Super 로 간다 (E12) — PC 는 인스턴스로 선판정하므로 문제없다.
	if (RecastTag.IsValid() && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		&& ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(RecastTag))
	{
		return true;
	}
	return Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
}

const FGameplayTagContainer* UERGameplayAbility::GetCooldownTags() const
{
	// ⚠ Super 는 &CooldownGE->GetGrantedTags() 다 (GameplayAbility.cpp:1076-1080).
	//   UERCooldownEffect 는 태그를 안 주므로 그걸 쓰면 CheckCooldown 이 항상 통과한다.
	return &CooldownTags;
}

void UERGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// ⚠ 서버만. ServerInitiated 라 클라도 Commit 을 타고 여기까지 오는데, 클라의 GE 적용은
	//   예측 키가 없어서 엔진이 버린다 (AbilitySystemComponent.cpp:380-383
	//   HasNetworkAuthorityToApplyGameplayEffect). 계산·로그만 헛돌므로 먼저 끊는다.
	//   예측을 켜면(LocalPredicted) 이 가드를 빼야 한다.
	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	const UERSkillData* SkillData = GetSkillData(Handle, ActorInfo);
	if (!SkillData || CooldownTags.IsEmpty())
	{
		return;
	}

	// 리캐스트로 발동했으면 쿨다운을 다시 걸지 않는다 — 첫 시전의 쿨이 그대로 돈다 (자체 결정값).
	if (bActivatedByRecast)
	{
		return;
	}

	// ── ① 기본 쿨다운 (레벨별) ───────────────────────────────
	const float BaseCooldown = SkillData->GetCooldown(GetAbilityLevel(Handle, ActorInfo));
	if (BaseCooldown <= 0.f)
	{
		return;   // 쿨다운 없는 스킬 (카티야 P 등). GE 를 안 건다.
	}

	// ── ② 스킬 가속 환산 ─────────────────────────────────────
	//   최종 = 기본 × 100 / (100 + 스킬 가속)   — 원본 §6.4 [S32], 스탯공식 §4
	//   ⭐ 상한 없음. F02-03 이 확인한 대로 SkillHaste 를 여기서 자르지 않는다.
	//   ⚠ 음수 가속은 0 으로 본다 — 쿨이 늘어나는 원작 규칙이 없다 (미확인).
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	float FinalCooldown = BaseCooldown;
	if (!SkillData->bIgnoreCooldownReduction)
	{
		const float Haste = ASC ? FMath::Max(ASC->GetNumericAttribute(UERAttributeSet::GetSkillHasteAttribute()), 0.f) : 0.f;
		FinalCooldown = BaseCooldown * 100.f / (100.f + Haste);
	}

	// ── ③ 공용 GE 에 길이 + 슬롯 태그를 심어 적용 ───────────────
	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, CooldownGameplayEffectClass, GetAbilityLevel(Handle, ActorInfo));

	if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
	{
		// ⭐ GE 애셋이 아니라 스펙에 심는 태그. 적용되면 ASC 소유 태그가 되어 CheckCooldown 이 본다.
		Spec->DynamicGrantedTags.AppendTags(CooldownTags);
		Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_Cooldown, FinalCooldown);

		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);

		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 쿨다운 %.2f초 (기본 %.2f · 가속 %s) 서버시각=%.2f"),
			*GetNameSafe(ActorInfo->OwnerActor.Get()), *GetNameSafe(SkillData), FinalCooldown, BaseCooldown,
			SkillData->bIgnoreCooldownReduction ? TEXT("무시") : TEXT("적용"),
			ASC ? ASC->GetWorld()->GetTimeSeconds() : 0.f);
	}
}

bool UERGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	// ⭐ 미습득. GAS 는 레벨을 검사하지 않는다 — 스펙 레벨은 "몇 레벨 값을 쓰나" 일 뿐이다.
	//   역기획서 §4.1 [1] "스킬 레벨>0" 이 여기다.
	if (GetAbilityLevel(Handle, ActorInfo) <= 0)
	{
		return false;
	}

	// 탑승 중 (매그너스 R 바이크) — 평타 · 다른 스킬은 막고 **자기 리캐스트 창(재사용 발사)만** 연다.
	//   차단 태그(State.Block.Skill)로 막으면 R 재사용까지 막힌다 — 리캐스트는 쿨다운만 건너뛰기 때문 (Argument 62).
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (ASC && ASC->HasMatchingGameplayTag(ERTags::State_Riding))
	{
		const UERSkillData* Skill = GetSkillData(Handle, ActorInfo);
		const FGameplayTag Recast = Skill ? ERSkill::RecastTagForSlot(Skill->SlotTag) : FGameplayTag();
		if (!Recast.IsValid() || !ASC->HasMatchingGameplayTag(Recast))
		{
			return false;
		}
	}

	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

// ─────────────────────────────────────────────────────────────
// 코스트
// ─────────────────────────────────────────────────────────────

namespace
{
	TSubclassOf<UGameplayEffect> CostEffectClassFor(ESkillCostType CostType)
	{
		switch (CostType)
		{
		case ESkillCostType::VP: return UERVPCostEffect::StaticClass();
		case ESkillCostType::HP: return UERHPCostEffect::StaticClass();
		default:                 return nullptr;
		}
	}
}

bool UERGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	// ⚠ Super 를 부르지 않는다. Super 는 CostGE 의 모디파이어를 SetByCaller 없이 평가해서
	//   (GameplayAbility.cpp:975-981 CanApplyAttributeModifiers) 0 으로 통과시키고 에러 로그를 남긴다.
	//   쿨다운과 같은 이유로 직접 본다.
	// ⭐ 핸들 버전 — 클라 사전 검사는 CDO 에서 돈다 (E12).
	const UERSkillData* SkillData = GetSkillData(Handle, ActorInfo);
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!SkillData || !ASC || SkillData->CostType == ESkillCostType::None)
	{
		return true;
	}

	const float Cost = SkillData->GetCost(GetAbilityLevel(Handle, ActorInfo));
	if (Cost <= 0.f)
	{
		return true;
	}

	bool bAffordable = true;
	switch (SkillData->CostType)
	{
	case ESkillCostType::VP:
		bAffordable = ASC->GetNumericAttribute(UERAttributeSet::GetVPAttribute()) >= Cost;
		break;

	case ESkillCostType::HP:
		// ⭐ 부족해도 시전은 된다 — 1 까지만 지불한다 (Task 문서 "현재HP-1 로 깎아서 넘긴다").
		//   낼 게 아예 없는(HP<=1) 경우만 막는다. 자체 결정값, 원작 (미확인).
		bAffordable = ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute()) > 1.f;
		break;

	default:
		break;
	}

	if (!bAffordable)
	{
		const FGameplayTag& CostTag = UAbilitySystemGlobals::Get().ActivateFailCostTag;
		if (OptionalRelevantTags && CostTag.IsValid())
		{
			OptionalRelevantTags->AddTag(CostTag);
		}
	}
	return bAffordable;
}

void UERGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// ⚠ 서버만 — ApplyCooldown 과 같은 이유.
	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	// 리캐스트는 코스트를 다시 안 낸다 — 쿨다운과 같은 규칙 [자체] (시셀라 W 다시 눌러 터뜨리기에 체력 50 을 또 내지 않게 · 2026-10-06)
	if (bActivatedByRecast)
	{
		return;
	}

	const UERSkillData* SkillData = GetSkillData(Handle, ActorInfo);
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const TSubclassOf<UGameplayEffect> CostClass = SkillData ? CostEffectClassFor(SkillData->CostType) : nullptr;
	if (!CostClass || !ASC)
	{
		return;
	}

	float Cost = SkillData->GetCost(GetAbilityLevel(Handle, ActorInfo));
	if (Cost <= 0.f)
	{
		return;
	}

	// ⭐ HP 하한 1 — 어트리뷰트셋이 아니라 여기서. F02-04: "하한이 데미지 경로에 섞이면 죽어야 할 때 안 죽는다".
	if (SkillData->CostType == ESkillCostType::HP)
	{
		const float HP = ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute());
		Cost = FMath::Min(Cost, FMath::Max(HP - 1.f, 0.f));
		if (Cost <= 0.f)
		{
			return;
		}
	}

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, CostClass, GetAbilityLevel(Handle, ActorInfo));

	if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
	{
		// GE 는 Additive 라 음수로 넣는다.
		Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_Cost, -Cost);
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);

		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 코스트 %s %.0f 지불"),
			*GetNameSafe(ActorInfo->OwnerActor.Get()), *GetNameSafe(SkillData),
			SkillData->CostType == ESkillCostType::HP ? TEXT("HP") : TEXT("VP"), Cost);
	}
}

// ─────────────────────────────────────────────────────────────
// 데이터
// ─────────────────────────────────────────────────────────────

const UERSkillData* UERGameplayAbility::GetSkillData(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	const UERSkillData* SkillData = Spec ? Cast<UERSkillData>(Spec->SourceObject.Get()) : nullptr;

	if (!SkillData)
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[스킬] %s 에 SkillData 가 없다 (핸들 조회). ERSkill::GrantSkills 로 부여해야 한다."),
			*GetNameSafe(this));
	}
	return SkillData;
}

const UERSkillData* UERGameplayAbility::GetSkillData() const
{
	// ⭐ 부여할 때 ERSkill::GrantSkills 가 SourceObject 에 심어 둔 것을 꺼낸다.
	const UERSkillData* SkillData = Cast<UERSkillData>(GetCurrentSourceObject());

	if (!SkillData)
	{
		// ⚠ GrantSkills 를 안 거치고 직접 GiveAbility 한 것이다. 잘못된 경로다.
		//   조용히 nullptr 를 돌려주면 "수치가 전부 0" 으로 나타나 원인을 못 찾는다.
		UE_LOG(LogEternalReturn, Error,
			TEXT("[스킬] %s 에 SkillData 가 없다. ERSkill::GrantSkills 로 부여해야 한다."),
			*GetNameSafe(this));
	}

	return SkillData;
}

FGameplayTag UERGameplayAbility::GetSlotTag() const
{
	const UERSkillData* SkillData = GetSkillData();
	return SkillData ? SkillData->SlotTag : FGameplayTag();
}
