// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "GAS/Fragment/ERSkillContext.h"
#include "Combat/ERTargetingTypes.h"   // FTargetQuery (MakeTargetQuery 반환값)
#include "ERGameplayAbility.generated.h"

class UERSkillFragment;
class UERSkillFragmentState;

class UERSkillData;
struct FGameplayEventData;
struct FTargetResult;
struct FERShapeContext;
struct FERProjectileLaunch;
class AERProjectileBase;

/**
 * 모든 스킬의 베이스.
 *
 * ⭐ **이 클래스는 로직만 갖는다. 수치는 UERSkillData 에 있다.**
 *   같은 어빌리티 클래스를 여러 캐릭터가 데이터만 바꿔 공유한다 —
 *   Lyra 의 ULyraGameplayAbility 가 정책만 갖고 수치는 WeaponInstance 에 두는 것과 같다
 *   (LyraGameplayAbility.h:188-209). 근거: Docs/4_Argument/15_스킬데이터_위치.md
 *
 * ⚠ **계산식을 여기에 넣지 않는다.** 피해는 F03 의 ERDamageExecution 이 한 곳에서 계산한다.
 *   어빌리티는 **계수만** SetByCaller 로 넘긴다 (CLAUDE.md §8).
 *
 * ⚠ **대상 스탯을 읽지 않는다.** 방어력·저항은 Execution / MMC 가 캡처한다 (F03 · F06-05).
 *
 * ⭐ CC 차단은 여기서 하지 않는다 — 애셋의 ActivationBlockedTags 에 State.Block.Skill 을
 *   넣으면 GAS 가 막는다 (F06-01 · Docs/4_Argument/10_CC_차단축_태그설계.md).
 *
 * ⭐ 쿨다운 (F07-02): GE 는 UERCooldownEffect 하나를 전 스킬이 공유하고, "어느 슬롯의 쿨인가" 는
 *   이 클래스가 Cooldown.Slot.* 태그를 스펙에 심어 표현한다. 길이는 ApplyCooldown 이
 *   `기본 × 100 / (100 + SkillHaste)` 로 계산한다 (Docs/4_Argument/16_쿨다운_가속환산_위치.md 방안 A).
 *   ⚠ FTimerManager 로 재지 않는다 (CLAUDE.md §8).
 *
 * ⭐ 코스트 (F07-03): UERSkillData.CostType 이 VP/HP 를 고르고, CheckCost 가 잔량을 보고,
 *   ApplyCost 가 서버에서 깎는다. HP 는 `min(코스트, HP-1)` — 코스트로 죽지 않는다 (역기획서 §3).
 *
 * ⭐ 레벨 0 = 미습득. CanActivateAbility 가 막는다. 포인트로 올린다 (ERSkill::LevelUpSkill).
 *
 * ⭐⭐ 실행 파이프라인 (F07-04, 역기획서 §4.1) — **ActivateAbility 가 템플릿이다. 파생은 ExecuteSkill 만 채운다.**
 *
 *   [1] 발동 가능     : CanActivateAbility (GAS + 레벨>0 + State.Block.Skill / State.Recovering 차단)
 *   [2] 선딜(캐스팅)  : CastTime > 0 이면 UERSkillPhaseEffect{State.Casting(+Block.Movement)} + WaitDelay
 *                       취소: State.Block.Skill 부여(CC) · (bMoveCancelsCast 면) Event.Input.Move
 *                       -> 쿨다운만 커밋하고 CancelAbility. 코스트는 안 든다 (§5.1, 자체 결정값)
 *   [3] 조준 확정     : 발동 요청에 실려 온 조준점(FGameplayAbilityTargetData)을 **서버가 사거리로 클램프** (ResolveAim)
 *   [4] 발동          : CommitAbility(코스트 + 쿨다운) -> 조각.OnExecute [서버] -> ExecuteSkill() [서버] -> 조각.OnTargetsResolved -> K2_OnSkillExecuted() [양쪽, 연출] -> 조각.OnLocalExecute
 *                       기본 ExecuteSkill = Shape 로 ERTargeting::Query -> 조각 훅 -> OnTargetsResolved(파생 후처리)
 *                       ⭐ 기능(피해 · 적중 효과 · 이동 · 버프 · 넉백 · 2차 · 리캐스트 · 장판 · 모드)은 전부 **조각** — GAS/Fragment/ (F11.5 · Argument 28)
 *   [5] 후딜          : RecoveryTime > 0 이면 UERSkillPhaseEffect{State.Recovering} + WaitDelay
 *                       Event.Input.Move 가 오면 즉시 끝낸다 (애니메이션 캔슬)
 *   -> EndAbility
 *
 *   ⚠ **BP 가 ActivateAbility 를 오버라이드하면 이 파이프라인이 통째로 무시된다** (GameplayAbility.cpp:790 — K2_ActivateAbility 가 대신 불린다).
 *     BP 는 OnSkillExecuted 연출 훅만 쓴다.
 *   ⚠ 대기는 전부 UAbilityTask, 상태는 GE. 자체 타이머 상태 머신 없음 (CLAUDE.md §8).
 *   ⚠ 큐잉(§6) · 사망 취소 · 몽타주는 이 단계에 없다 (Docs/4_Argument/17 "이번에 안 하는 것").
 */
UCLASS(Abstract)
class ETERNALRETURN_API UERGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UERGameplayAbility();

	/**
	 * 이 어빌리티의 데이터. 부여될 때 SourceObject 로 심어진 것을 꺼낸다.
	 *
	 * ⚠ nullptr 일 수 있다 — ERSkill::GrantSkills 를 안 거치고 직접 GiveAbility 한 경우.
	 *   그건 잘못된 경로이므로 로그를 남긴다. 호출부는 nullptr 를 검사한다.
	 * ⚠ **인스턴스에서만** 부른다 (ActivateAbility 안 등). CDO 에서 부르면 ensure 가 난다.
	 */
	const UERSkillData* GetSkillData() const;

	// ── F11.5 조각이 부르는 것 (FERSkillContext 경유) ──────────────
	/** 다른 데이터로 파이프라인 재진입 — 2차 판정(판정+적중만) · 리캐스트/벽 충돌. 서버. */
	void ExecuteOther(const UERSkillData* Other, float Scale, bool bWithExecuteHooks);
	/** bKeepActive 로 살려 둔 어빌리티를 조각이 끝낸다 (모드 해제). */
	void EndFromFragment(bool bCancelled);
	/** 조각별 상태 객체 — 없으면 만든다 (Outer = this). */
	UERSkillFragmentState* GetOrCreateFragmentState(const UERSkillFragment* Owner, TSubclassOf<UERSkillFragmentState> Class);
	/**
	 * 판정 없이 정해진 대상에게 Data 의 적중 조각(피해 · 적중 효과 …)만 실행 — 장판 펄스 · 벽 충돌 · 평타 강화.
	 * LevelOverride < 0 이면 이 어빌리티 레벨. ShapeOwner 는 광역 태그 기준 (null = Data). 서버.
	 */
	void ApplyOnTargets(const UERSkillData* Data, const TArray<AActor*>& Targets, float Scale = 1.f, int32 LevelOverride = -1,
		const UERSkillData* ShapeOwner = nullptr, bool bEnhancement = false, float TravelRatio = -1.f, int32 ShotIndex = -1);
	/** 이번 실행의 문맥 (조각 훅 · 상태 객체가 만든다). */
	FERSkillContext MakeContext(const UERSkillData* Skill, float Scale) const;
	FGameplayTag GetRecastTag() const { return RecastTag; }
	/** 조각용 — 쿨다운 태그(모드 반환) · 사거리(블링크 전제). 엔진 오버라이드는 protected 라 공개 접근자를 둔다. */
	const FGameplayTagContainer& GetCooldownTagsForFragment() const { return CooldownTags; }
	/** 마지막 조준 대상 (평타 = 마지막 평타 대상) — 조각용 (재키 W 평타 초기화 · 2026-10-05) */
	AActor* GetAimActorForFragment() const { return AimActor.Get(); }
	/** 이번 시전의 조준점 (서버 클램프 뒤) — 투사체 파생이 시작점을 바꿀 때 (시셀라 E 윌슨 자리 → 커서) */
	FVector GetAimPointForFragment() const { return AimPoint; }
	/** [서버] 지금 이 스킬의 쿨다운을 건다 — 활성 인스턴스에서 (패시브 강화 평타를 쓴 순간 · 시셀라 P · Argument 68) */
	void StartCooldownNow() { ApplyCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo); }
	float GetRangeMaxFor(const UERSkillData& Skill) const { return GetRangeMax(Skill); }
	/** 조각용 공개 래퍼 — 엔진의 ApplyGameplayEffectSpecTo* 는 protected 다. */
	void ApplySpecToTargets(const FGameplayEffectSpecHandle& Spec, const FGameplayAbilityTargetDataHandle& TargetData)
	{
		ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Spec, TargetData);
	}
	void ApplySpecToSelf(const FGameplayEffectSpecHandle& Spec)
	{
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Spec);
	}
	/** [소유 클라] 시전 전 범위 미리보기 — 커서를 ResolveAim 과 같게 당겨 판정과 같은 모양을 한 프레임 그린다 (Argument 56 C2 · D1). */
	void DrawPreview(const UERSkillData& Skill, const FVector& CursorPoint) const;
	/** [서버] 투사체 도착 — 적중 조각 + 타격음 큐 + OnProjectileTargetHit (F19-01 · Argument 54 · 60). 시전음은 발사 때 이미 냈다. */
	void ApplyProjectileHit(const UERSkillData* Data, AActor* Target, int32 Level, float TravelRatio, int32 ShotIndex = -1);
	/** ER.Skill.DebugDraw 1 — 머리 위 글자 1.5초 (모드 조각 등). */
	static void DebugDrawText(const AActor* Avatar, const FString& Text, FColor Color = FColor::Cyan);

	// ── 발사 방식(UERSkillDelivery)이 부르는 것 (Argument 57 S3.1) ──────────
	/** 모양 문맥 — 조준 · 대상 칸 · 시전 시작 때 저장한 자리 · 사거리(평타 = 무기 사거리). */
	FERShapeContext MakeShapeContext(const UERSkillData& Skill, const FVector& InAimPoint, const FVector& InAimDirection, AActor* Designated) const;
	/** [서버] 즉시 판정의 끝 — 판정 로그 · 적중 조각 · 연출 큐 · OnTargetsResolved. */
	/** bAttackCue false = 공격(시전) 큐를 안 보낸다 — 늦춘 판정(착지)은 시전음을 뛰어오를 때 이미 냈다 */
	void ResolveInstantHits(const UERSkillData& Skill, const FTargetQuery& Q, const FTargetResult& Result, bool bAttackCue = true);
	/** [서버] 적중이 나중(투사체 도착)인 발사 — 적중과 무관한 조각만 지금("빗나감" 벌칙 보류) · bAttackCue 면 시전음도 지금. */
	void BeginDeferredHits(const UERSkillData& Skill, bool bAttackCue);
	/** [서버] 투사체 하나 (Deferred → InitLaunch → Finish). */
	AERProjectileBase* SpawnProjectile(const UERSkillData& Skill, TSubclassOf<AERProjectileBase> Class, const FERProjectileLaunch& Launch,
		const FTargetQuery& Filter, int32 Level, int32 ShotIndex, bool bPierce);
	/** ER.Skill.DebugDraw 1 — 모양 테두리(초록) · 적중(빨강) 1.5초 */
	void DebugDrawQuery(const UERSkillData& Skill, const FTargetQuery& Q, const FTargetResult& Result) const;
	/** [서버] 연출 큐 (F12.5-05 · Argument 49 W2) — 판정 순간에 시전자 `GameplayCue.Pres.Attack` · 맞은 대상마다 `GameplayCue.Pres.Hit`. */
	void SendPresCues(const UERSkillData& Skill, AActor* Avatar, const TArray<AActor*>& Targets, bool bWithAttack, int32 ShotNumber = 0, const FVector& FaceDirection = FVector::ZeroVector) const;
	/** [서버] 스킬 통 몽타주 섹션 넘기기 (K8 · Fire · Loop · End) — 다른 클라엔 복제 · 소유 클라는 같이 보내는 큐로. 통 몽타주가 아니면 아무것도 안 한다. */
	void JumpSkillSection(const UERSkillData& Skill, FName Section) const;
	/** [서버] 시전자 연출 사건 하나 (K8) — `GameplayCue.Pres.Aim` (순차 사격 다음 발 조준 · 발 번호) · `.Ready` (강화 걸림). 무엇을 틀지는 받는 쪽 키. */
	void SendEventCue(FGameplayTag CueTag, const UERSkillData& Skill, int32 ShotNumber = 0) const;
	/** 이번 실행이 2차 판정 · 리캐스트 데이터인가 — 시전음은 시전 한 번에 한 번 */
	bool IsExecutingOther() const { return bExecutingOther; }
	/** 서버 인스턴스인가 (발사 방식은 엔진 HasAuthority 에 못 닿는다 — protected) */
	bool IsExecAuthority() const { return HasAuthority(&CurrentActivationInfo); }

	/**
	 * 핸들로 찾는 버전. ⭐ **CDO 에서도 동작한다.**
	 *   클라가 ServerInitiated 어빌리티를 TryActivate 하면 사전 검사(CanActivateAbility)가
	 *   인스턴스가 아니라 **CDO** 에서 돈다 (AbilitySystemComponent_Abilities.cpp:1591 — `Ability->CanActivateAbility`).
	 *   그래서 CheckCost · ApplyCost · ApplyCooldown 처럼 Handle 을 받는 오버라이드는 이쪽을 쓴다 (E12).
	 */
	const UERSkillData* GetSkillData(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const;

	/** 이 어빌리티의 슬롯. 데이터가 없으면 빈 태그. */
	FGameplayTag GetSlotTag() const;

protected:
	// ── 파이프라인 ────────────────────────────────────────────
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/**
	 * [4] 실제 효과. **서버에서만** 불린다.
	 * 기본 구현: SkillData.Shape + 조준으로 F04 판정 → OnTargetsResolved. 형상이 없는 스킬(자기 이동 등)은 파생이 통째로 바꾼다.
	 */
	virtual void ExecuteSkill();

	/**
	 * 디자이너 필드 + 조준 → 판정 질의 (조준 보조 포함). 판정(ExecuteSkill)과 발동 전 대상 확인이 **같은 질의**를 쓴다 —
	 * 다르면 "확인은 통과했는데 판정은 빗나감" 이 생긴다.
	 */
	FTargetQuery MakeTargetQuery(const UERSkillData& Skill) const;
	/** 조준점 · 방향만 받아 판정 질의 (지정 대상 · 조준 보조 없음) — MakeTargetQuery 와 시전 전 미리보기가 같이 쓴다. */
	FTargetQuery BuildQuery(const UERSkillData& Skill, const FVector& InAimPoint, const FVector& InAimDirection) const;

	/**
	 * 판정 결과의 어빌리티 고유 후처리. 기본은 아무것도 안 한다 — 대상 효과는 조각(ExecuteSkill 이 먼저 돌린다).
	 * 평타가 "강화 소비 · N회 버프 소비" 를 여기에 둔다. 판정 **계산**(F04)은 건드리지 않는다.
	 */
	virtual void OnTargetsResolved(const FTargetResult& Result);

	/** [서버] 투사체가 대상에 닿았다 — 적중 조각 · 타격음 뒤. 기본 없음 · 평타가 강화 소비를 도착 때 한다 (Argument 60 T1). */
	virtual void OnProjectileTargetHit(const UERSkillData& Data, AActor* Target) {}

	/** 이번 실행에 쓰는 데이터. 보통 자기 데이터. 리캐스트 조각이 지정한 데이터 · ExecuteOther 중엔 그것. */
	const UERSkillData* GetExecSkill() const;

	/** 리캐스트 윈도우가 열려 있는가 (자기 슬롯의 Recast.Slot.* 태그). */
	bool IsRecastWindowOpen() const;

	/** 사거리 상한(m). 기본 = 모양 GetMaxReach. 평타는 AttackRange 어트리뷰트로 덮는다 (F07-07). ResolveAim 클램프 · 판정 쿼리가 쓴다. */
	virtual float GetRangeMax(const UERSkillData& Skill) const;

	/** [3] 서버가 확정한 조준. 시전자 발밑 기준. ExecuteSkill · 파생이 읽는다. */
	FVector GetAimPoint() const { return AimPoint; }
	FVector GetAimDirection() const { return AimDirection; }
	AActor* GetAimActor() const { return AimActor.Get(); }

public:
	/** 모드(저격) 중인가 — 조각이 어빌리티를 활성으로 붙들고 있다. PC 가 "모드 중 재입력 = 사격" 판단에 쓴다. */
	bool IsModeActive() const { return bFragmentKeepActive; }

	/** 이번 시전이 선딜 중 CC 로 끊겼나 — 한 번 읽으면 지운다 (EndAbility 조각 훅이 두 번 불려도 한 번만). 좌절 조각 (F12.6-03). */
	bool ConsumeCastInterruptedByCC() { const bool b = bCastInterruptedByCC; bCastInterruptedByCC = false; return b; }

protected:
	/** [4] 연출 훅. 서버·클라 양쪽에서 불린다 (ServerInitiated). 로직 금지 — 몽타주·이펙트·Print 만 (CLAUDE.md §7). */
	UFUNCTION(BlueprintImplementableEvent, Category = "ER|Skill", DisplayName = "OnSkillExecuted")
	void K2_OnSkillExecuted();

	/** 레벨 0(미습득)이면 발동 불가. 나머지는 Super (태그·쿨다운·코스트). */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// ── 코스트 ────────────────────────────────────────────────
	// ⚠ GetCostGameplayEffect() 는 오버라이드하지 않는다 — 핸들이 없어서 CDO 에서 데이터를 못 찾는다.
	//   CheckCost 는 Super 를 안 부르고, ApplyCost 는 CostType 으로 GE 클래스를 직접 고른다.

	/** VP: 잔량 >= 코스트. HP: 잔량 > 1 (1 까지만 지불하므로 낼 게 있으면 된다 — 자체 결정값, 원작 (미확인)). */
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const override;

	/** 서버만. 코스트를 SetByCaller 로 넣어 적용. HP 는 하한 1 을 여기서 깎는다. */
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	// ── 쿨다운 ────────────────────────────────────────────────
	/** 리캐스트 윈도우가 열려 있으면 쿨다운을 무시한다 (Argument 19 ③A). 그 외는 Super. */
	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const override;

	// 부여 시점에 CooldownTags 를 채운다. 서버·클라 모두 (클라도 CanActivateAbility 로 먼저 거른다).
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	/** 기본 구현은 GE 클래스의 GrantedTags 를 돌려준다(GameplayAbility.cpp:1076) — 공용 GE 라 비어 있다. 슬롯 태그를 돌려준다. */
	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	/** 길이 계산 + 슬롯 태그를 DynamicGrantedTags 로 심어 적용. 서버에서만 불린다 (CommitAbility). */
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

private:
	// ── 파이프라인 내부 ───────────────────────────────────────
	void BeginCast(const UERSkillData& Skill);
	void BeginRecovery(const UERSkillData& Skill);
	void ApplyPhaseEffect(float Duration, const FGameplayTagContainer& PhaseTags);
	void RemovePhaseEffect();
	/** 선딜 취소 공통 — 쿨다운만 커밋하고 CancelAbility. */
	void CancelCast(const TCHAR* Reason);

	UFUNCTION() void OnCastFinished();
	UFUNCTION() void OnCastInterruptedByCC();
	UFUNCTION() void OnCastInterruptedByMove(FGameplayEventData Payload);
	UFUNCTION() void OnRecoveryFinished();
	UFUNCTION() void OnRecoveryCancelledByMove(FGameplayEventData Payload);
	UFUNCTION() void OnRecoveryInterruptedByCC();

	void ExecuteAndRecover();

	/**
	 * 발동 애니 (F12.5-01 · Argument 39) — 키 = 자기 슬롯 태그, 애니는 시전자의 연출 컴포넌트가 고른다. **판정 흐름과 무관** (Argument 36).
	 * 서버(복제 원천) + 소유 클라 — 엔진은 복제된 몽타주를 소유자에게는 재생하지 않는다 (`AbilitySystemComponent_Abilities.cpp:3145` · 예측 전제).
	 */
	void PlaySkillAnim(const UERSkillData& Skill, bool bExecutePhase = false);

	/** [3] 발동 요청의 TargetData 에서 조준점을 읽고 사거리 [RangeMin, RangeMax] 로 클램프한다. 없으면 시전자 정면. */
	void ResolveAim(const FGameplayEventData* TriggerEventData, const UERSkillData& Skill);

	/** 지금 페이즈 GE. 서버에서만 유효. EndAbility 가 지운다. */
	FActiveGameplayEffectHandle PhaseEffectHandle;

	// [3] 확정된 조준. 매 발동마다 ResolveAim 이 덮어쓴다.
	FVector AimPoint = FVector::ZeroVector;
	/** 모양이 시전 시작 때 저장한 자리 (PlayerCircles — 판정 순간 각 자리에 원). 서버만. */
	TArray<FVector> CircleAimPoints;
	FVector AimDirection = FVector::ForwardVector;
	TWeakObjectPtr<AActor> AimActor;

	/** 이 인스턴스의 슬롯 쿨다운 태그 하나 (Cooldown.Slot.Q 등). OnGiveAbility 에서 채운다. */
	FGameplayTagContainer CooldownTags;

	/** 이 인스턴스의 리캐스트 태그 (Recast.Slot.Q 등). 없으면 빈 태그. OnGiveAbility 에서 채운다. */
	FGameplayTag RecastTag;

	/** 이번 발동이 리캐스트(윈도우 소비)였는가 — 쿨다운을 새로 걸지 않는다. ActivateAbility 가 정한다. */
	bool bActivatedByRecast = false;

	/** ExecuteAndRecover · ExecuteOther 가 세팅 · 해제. 리캐스트 · 2차 판정 · 벽 충돌 데이터. */
	const UERSkillData* ExecOverride = nullptr;
	/** 2차 판정(ExecuteOther) 중 — 공격음을 안 낸다. ⚠ ExecOverride 로 판단하면 리캐스트 데이터(매그너스 R 바이크 발사)까지 소리가 막혔다 (2026-10-04) */
	bool bExecutingOther = false;

	// ── 조각 (F11.5) ──
	/** 이번 실행의 문맥. ExecuteAndRecover/ExecuteOther 가 만들고 ExecuteSkill 이 읽는다 (bSkipTargeting · Targets). */
	FERSkillContext ExecCtx;
	/** 조각이 활성 유지를 요청한 상태 (서버: ExecCtx.bKeepActive · 클라: KeepsAbilityActive 조각 존재). EndAbility 가 내린다. */
	bool bFragmentKeepActive = false;
	/** 선딜 중 CC 취소 표시 — OnCastInterruptedByCC 가 세우고 ActivateAbility 가 지운다. */
	bool bCastInterruptedByCC = false;
	/** 선딜 CC 감시 태스크 — 선딜이 끝나면 멈춘다 (E33). */
	TWeakObjectPtr<class UAbilityTask_WaitGameplayTagAdded> CastCCTask;
	/** 조각 훅 일괄 호출. Skill 이 null 이거나 조각이 없으면 아무 일도 없다 — 옛 필드 경로와 공존 (01 단계). */
	void RunFragmentsExecute(FERSkillContext& Ctx);
	void RunFragmentsTargets(FERSkillContext& Ctx, const TArray<AActor*>& Targets);
	void RunFragmentsLocal(FERSkillContext& Ctx);
	void RunFragmentsEnd(FERSkillContext& Ctx, bool bCancelled);
	const UERSkillFragment* FindFragmentBlocking(const FERSkillContext& Ctx, FString& OutReason) const;

	UPROPERTY()
	TMap<TObjectPtr<const UERSkillFragment>, TObjectPtr<UERSkillFragmentState>> FragmentStates;
};
