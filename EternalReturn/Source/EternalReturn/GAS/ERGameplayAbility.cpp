// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "EternalReturn.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERCooldownEffect.h"
#include "GAS/ERCostEffect.h"
#include "GAS/ERSkillPhaseEffect.h"
#include "GAS/ERSkillDamageEffect.h"
#include "GAS/ERSkillStateEffects.h"
#include "GAS/ERCCLibrary.h"
#include "Combat/ERTargeting.h"
#include "Combat/ERForcedMove.h"
#include "GameFramework/Character.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"

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
}

// ─────────────────────────────────────────────────────────────
// 파이프라인
// ─────────────────────────────────────────────────────────────

void UERGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// ⚠ Super 를 부르지 않는다 — Super 는 BP 의 K2_ActivateAbility 로 넘기는 분기뿐이다 (GameplayAbility.cpp:786-800).
	//   BP 가 ActivateAbility 를 오버라이드했다면 여기 자체가 안 불린다.

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

	// 리캐스트 — 윈도우가 열려 있었으면 이번 발동이 그것을 **소비**한다. 쿨다운은 새로 안 건다 (Argument 19 ③A, 자체 결정값).
	//   ⚠ 여기서 지우면 안 된다 — CommitAbility → CheckCooldown 이 태그를 못 봐 "쿨다운 중" 으로 실패한다
	//     (2026-09-14 로그 "리캐스트 (윈도우 소비)" 직후 "커밋 실패"). 커밋이 성공한 뒤 ExecuteAndRecover 에서 지운다.
	bActivatedByRecast = IsRecastWindowOpen();

	if (Skill->CastTime > 0.f)
	{
		BeginCast(*Skill);       // [2] -> OnCastFinished -> ExecuteAndRecover
	}
	else
	{
		ExecuteAndRecover();     // 즉발: [4] -> [5]
	}
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
	RemovePhaseEffect();
	ExecuteAndRecover();
}

void UERGameplayAbility::OnCastInterruptedByCC()
{
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

		ApplySelfMove();   // 돌진·도약이 있으면 판정보다 먼저 출발한다
		ExecuteSkill();
	}

	// 연출 훅은 양쪽.
	K2_OnSkillExecuted();

	const UERSkillData* Skill = GetSkillData(CurrentSpecHandle, CurrentActorInfo);
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
	}

	// ⭐ 서버 클램프 — 클라가 보낸 좌표를 믿지 않는다 (§4.2 "클램프 없으면 사거리 핵").
	//   Z 는 무시한다. 탑다운이라 판정도 bIgnoreZ 다 (F04).
	FVector ToAim = AimPoint - Origin;
	ToAim.Z = 0.f;
	const float Dist = ToAim.Size();
	const float MinUU = Skill.Shape.RangeMin * 100.f;
	const float MaxUU = GetRangeMax(Skill) * 100.f;

	if (Dist > KINDA_SMALL_NUMBER)
	{
		AimDirection = ToAim / Dist;
	}
	const float ClampedDist = FMath::Clamp(Dist, MinUU, MaxUU);
	if (!FMath::IsNearlyEqual(ClampedDist, Dist))
	{
		UE_LOG(LogEternalReturn, Verbose, TEXT("[스킬] %s 조준 거리 %.0f -> %.0f (사거리 %.0f~%.0f)"),
			*GetNameSafe(&Skill), Dist, ClampedDist, MinUU, MaxUU);
	}
	AimPoint = Origin + AimDirection * ClampedDist;
	AimPoint.Z = Origin.Z;
}

bool UERGameplayAbility::ApplySelfMove()
{
	const UERSkillData* Skill = GetSkillData(CurrentSpecHandle, CurrentActorInfo);
	ACharacter* Avatar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Skill || !Avatar || Skill->SelfMove == ESkillSelfMove::None)
	{
		return false;
	}

	FVector Direction = AimDirection;
	float DistanceUU = Skill->SelfMoveDistance * 100.f;

	switch (Skill->SelfMove)
	{
	case ESkillSelfMove::TowardAim:
		break;

	case ESkillSelfMove::AwayFromAim:
		// ⚠ 카티야 E — 조준의 **반대**. 여기 한 줄이 이 스킬의 전부다.
		Direction = -AimDirection;
		break;

	case ESkillSelfMove::ToAimPoint:
		// 조준점은 ResolveAim 이 이미 사거리로 클램프했다 → 거리 = 발밑에서 조준점까지.
		DistanceUU = FVector::Dist2D(ERTargeting::GetTargetingLocation(Avatar), AimPoint);
		break;

	default:
		return false;
	}

	const bool bStarted = ERForcedMove::ApplySelfMove(Avatar, Direction, DistanceUU, Skill->SelfMoveDuration);

	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 자기 이동 %s %.0fcm / %.2f초 -> %s"),
		*GetNameSafe(GetOwningActorFromActorInfo()), *GetNameSafe(Skill),
		*UEnum::GetValueAsString(Skill->SelfMove), DistanceUU, Skill->SelfMoveDuration,
		bStarted ? TEXT("시작") : TEXT("실패"));
	return bStarted;
}

void UERGameplayAbility::ExecuteSkill()
{
	const UERSkillData* Skill = GetSkillData(CurrentSpecHandle, CurrentActorInfo);
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Skill || !Avatar)
	{
		return;
	}

	// ⭐ F04 는 그대로 쓴다. 여기서는 디자이너 필드 + 조준을 FTargetQuery 로 옮길 뿐이다.
	FTargetQuery Q;
	Q.Shape = Skill->Shape.Shape;
	Q.TeamFilter = Skill->Shape.TeamFilter;
	Q.RangeMax = GetRangeMax(*Skill);
	Q.RangeMin = Skill->Shape.RangeMin;
	Q.RadiusInner = Skill->Shape.RadiusInner;
	Q.RadiusOuter = Skill->Shape.RadiusOuter;
	Q.AngleDeg = Skill->Shape.AngleDeg;
	Q.ProjectileRadius = Skill->Shape.ProjectileRadius;
	Q.bPenetrate = Skill->Shape.bPenetrate;
	Q.Instigator = Avatar;
	Q.Direction = AimDirection;
	// GroundCircle 만 조준점이 중심이다. 나머지는 시전자가 원점.
	Q.Origin = (Q.Shape == ESkillTargeting::GroundCircle) ? AimPoint : ERTargeting::GetTargetingLocation(Avatar);
	Q.DesignatedTarget = AimActor.Get();

	// ⭐ 조준 보조 — SingleTarget 인데 커서 아래 액터가 유효한 대상이 아니면(바닥 · 자기 자신 · 아군),
	//   조준점 반경 AimAssistRadius 안에서 가장 가까운 대상을 대신 잡는다. 판정은 그대로 SingleTarget 이 한다.
	if (Q.Shape == ESkillTargeting::SingleTarget && Skill->Shape.AimAssistRadius > 0.f)
	{
		FTargetQuery Probe = Q;
		Probe.Shape = ESkillTargeting::SingleTarget;
		const bool bDirectValid = Q.DesignatedTarget && !ERTargeting::Query(Avatar->GetWorld(), Probe).IsEmpty();
		if (!bDirectValid)
		{
			FTargetQuery Assist = Q;
			Assist.Shape = ESkillTargeting::SelfRadius;   // 조준점 중심 원 — 가까운 순으로 정렬돼 온다 (F04)
			Assist.Origin = AimPoint;
			Assist.RangeMin = 0.f;
			Assist.RangeMax = Skill->Shape.AimAssistRadius;
			Assist.DesignatedTarget = nullptr;
			const FTargetResult Near = ERTargeting::Query(Avatar->GetWorld(), Assist);
			if (!Near.HitActors.IsEmpty())
			{
				Q.DesignatedTarget = Near.HitActors[0];
				UE_LOG(LogEternalReturn, Verbose, TEXT("[스킬] %s 조준 보조: %s -> %s"),
					*GetNameSafe(Skill), *GetNameSafe(AimActor.Get()), *GetNameSafe(Q.DesignatedTarget));
			}
		}
	}

	const FTargetResult Result = ERTargeting::Query(Avatar->GetWorld(), Q);

	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 판정 %s: 적중 %d (중앙 %d)"),
		*GetNameSafe(GetOwningActorFromActorInfo()), *GetNameSafe(Skill),
		*UEnum::GetValueAsString(Q.Shape), Result.HitActors.Num(), Result.InnerHitActors.Num());

	OnTargetsResolved(Result);
}

void UERGameplayAbility::OnTargetsResolved(const FTargetResult& Result)
{
	TArray<AActor*> Targets;
	Targets.Reserve(Result.HitActors.Num() + Result.InnerHitActors.Num());
	for (AActor* A : Result.HitActors)      { if (A) { Targets.Add(A); } }
	for (AActor* A : Result.InnerHitActors) { if (A) { Targets.Add(A); } }

	ApplySkillDamage(Targets);
	GrantOnHitStates(!Targets.IsEmpty());
}

void UERGameplayAbility::GrantOnHitStates(bool bHitAnything)
{
	const UERSkillData* Skill = GetSkillData(CurrentSpecHandle, CurrentActorInfo);
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo_Ensured();
	if (!Skill || !ASC || !HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	// ── 리캐스트 윈도우 (재키 Q "적중 시 3초 내 재사용") ─────
	//   ⚠ 이번 발동이 리캐스트였으면 다시 열지 않는다 — 무한 재사용이 된다.
	if (Skill->bRecastOnHit && bHitAnything && !bActivatedByRecast && RecastTag.IsValid())
	{
		const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
			CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, UERRecastWindowEffect::StaticClass(), GetAbilityLevel());
		if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
		{
			Spec->DynamicGrantedTags.AddTag(RecastTag);
			Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_StateDuration, Skill->RecastWindow);
			ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 리캐스트 윈도우 %.1f초 (%s)"),
				*GetNameSafe(GetOwningActorFromActorInfo()), *GetNameSafe(Skill), Skill->RecastWindow, *RecastTag.ToString());
		}
	}

	// ── 다음 기본 공격 강화 (재키 W · 카티야 P …) ─────────────
	//   적중 여부와 무관 — "사용 후" 다음 평타다. 이미 있으면 새로 건다(같은 스킬이면 갱신, 다른 스킬이면 나중 것).
	if (Skill->bGrantsNextAttackBuff)
	{
		FGameplayTagContainer Q; Q.AddTag(ERTags::State_NextAttackBuff);
		ASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(Q));

		const bool bInfinite = Skill->NextAttackBuffDuration <= 0.f;
		const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
			CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
			bInfinite ? UERNextAttackBuffInfiniteEffect::StaticClass() : UERNextAttackBuffEffect::StaticClass(), GetAbilityLevel());
		if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
		{
			Spec->DynamicGrantedTags.AddTag(ERTags::State_NextAttackBuff);
			if (!bInfinite)
			{
				Spec->SetSetByCallerMagnitude(ERTags::SetByCaller_StateDuration, Skill->NextAttackBuffDuration);
			}
			// ⭐ 평타가 이걸 읽어 "무슨 스킬의 피해·효과를 얹을지" 를 안다 (Argument 19 ②A). 레벨은 Spec Level 로 간다.
			Spec->GetContext().AddSourceObject(Skill);
			ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 다음 평타 강화 대기 (%s)"),
				*GetNameSafe(GetOwningActorFromActorInfo()), *GetNameSafe(Skill),
				bInfinite ? TEXT("만료 없음") : *FString::Printf(TEXT("%.1f초"), Skill->NextAttackBuffDuration));
		}
	}
}

void UERGameplayAbility::ApplySkillDamage(const TArray<AActor*>& Targets, const UERSkillData* SkillOverride, int32 LevelOverride)
{
	const UERSkillData* Skill = SkillOverride ? SkillOverride : GetSkillData(CurrentSpecHandle, CurrentActorInfo);
	if (!Skill || Targets.IsEmpty())
	{
		return;
	}
	const int32 Level = LevelOverride > 0 ? LevelOverride : GetAbilityLevel();

	// ── 적중 효과 (CC) — 피해와 독립. 피해 0 인 유틸리티(카티야 E 둔화)도 여기로 ─────
	if (Skill->OnHitEffect)
	{
		UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo_Ensured();
		const float Duration = UERSkillData::LevelValue(Skill->OnHitDuration, Level);
		const float Slow = UERSkillData::LevelValue(Skill->OnHitSlowPercent, Level);
		for (AActor* Target : Targets)
		{
			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
			if (TargetASC)
			{
				ERCC::ApplyCC(SourceASC, TargetASC, Skill->OnHitEffect, Duration, Slow);
			}
		}
	}

	if (Skill->DamageType == ESkillDamageType::None)
	{
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, UERSkillDamageEffect::StaticClass(), Level);
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (!Spec)
	{
		return;
	}

	// ⭐ 계수만 넘긴다. 곱하는 건 ERDamageExecution 이다 (Docs/4_Argument/5).
	Spec->SetSetByCallerMagnitude(ERTags::Data_Damage_Base,          UERSkillData::LevelValue(Skill->BaseDamage, Level));
	Spec->SetSetByCallerMagnitude(ERTags::Data_Damage_APRatio,       UERSkillData::LevelValue(Skill->APRatio, Level));
	Spec->SetSetByCallerMagnitude(ERTags::Data_Damage_BonusAPRatio,  UERSkillData::LevelValue(Skill->BonusAPRatio, Level));
	Spec->SetSetByCallerMagnitude(ERTags::Data_Damage_SkillAmpRatio, UERSkillData::LevelValue(Skill->SkillAmpRatio, Level));

	// 채널 태그 — 애셋 태그와 같은 통로 (GameplayEffect.h:1119-1120). 없으면 Execution 이 경고한다.
	switch (Skill->DamageType)
	{
	case ESkillDamageType::BasicAttack: Spec->AddDynamicAssetTag(ERTags::Damage_Type_BasicAttack); break;
	case ESkillDamageType::Fixed:       Spec->AddDynamicAssetTag(ERTags::Damage_Type_True);        break;
	default:                            Spec->AddDynamicAssetTag(ERTags::Damage_Type_Skill);       break;
	}

	// ⭐ 형상 태그는 **어빌리티가** 붙인다 — Execution 이 형상을 추측하지 않는다 (F03-05 흡혈 치유 감소).
	//   강화(SkillOverride)는 **이 어빌리티**(평타)의 판정에 실리는 것이라 형상도 이 어빌리티 것을 따른다.
	const UERSkillData* ShapeOwner = GetSkillData(CurrentSpecHandle, CurrentActorInfo);
	if (ShapeOwner && ShapeOwner->Shape.IsAoE())
	{
		Spec->AddDynamicAssetTag(ERTags::Damage_Shape_AoE);
	}

	// ⭐ 판정 결과를 GAS 그릇에 담아 넘긴다. 스펙은 우리 ASC 가 만들었으므로 Source = 시전자, Target = 대상 (F03-01).
	const FGameplayAbilityTargetDataHandle TargetData =
		UAbilitySystemBlueprintLibrary::AbilityTargetDataFromActorArray(Targets, /*OneTargetPerHandle=*/false);
	ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, TargetData);

	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s <- %s 피해 %d명 (기본 %.0f · %s%s%s)"),
		*GetNameSafe(GetOwningActorFromActorInfo()), *GetNameSafe(Skill), Targets.Num(),
		UERSkillData::LevelValue(Skill->BaseDamage, Level),
		Skill->DamageType == ESkillDamageType::Fixed ? TEXT("고정") : Skill->DamageType == ESkillDamageType::BasicAttack ? TEXT("평타") : TEXT("스킬"),
		(ShapeOwner && ShapeOwner->Shape.IsAoE()) ? TEXT(" · 광역") : TEXT(""),
		SkillOverride ? TEXT(" · 강화") : TEXT(""));
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
		const FGameplayTag CooldownTag = ERSkill::CooldownTagForSlot(SlotTag);
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
	return Skill.Shape.RangeMax;
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
