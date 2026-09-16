// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "ERGameplayAbility.generated.h"

class UERSkillData;
struct FGameplayEventData;
struct FTargetResult;

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
 *   [4] 발동          : CommitAbility(코스트 + 쿨다운) -> ApplySelfMove() [서버, F07-06] -> ExecuteSkill() [서버] + K2_OnSkillExecuted() [양쪽, 연출]
 *                       기본 ExecuteSkill = Shape 로 ERTargeting::Query -> OnTargetsResolved -> ApplySkillDamage
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
	 * 판정 결과를 받는다. 기본 구현: HitActors + InnerHitActors 전부에 ApplySkillDamage.
	 * ⭐ 레니 W 처럼 중앙/외곽이 다른 효과이거나, 다니엘 W 처럼 표식을 거는 스킬은 이걸 오버라이드한다.
	 *   판정 **계산**(F04)은 건드리지 않는다 — 결과의 **해석**만 바꾼다.
	 */
	virtual void OnTargetsResolved(const FTargetResult& Result);

	/**
	 * 대상들에게 이 스킬의 피해 + 적중 효과(OnHitEffect)를 준다. 계수는 SetByCaller, 채널·형상은 동적 애셋 태그 (F03 규약).
	 * FGameplayAbilityTargetData_ActorArray 로 감싸 ApplyGameplayEffectSpecToTarget — 자체 RPC 구조체 없음.
	 * DamageType 이 None 이면 피해는 건너뛰고 적중 효과만.
	 *
	 * @param SkillOverride  다른 스킬의 데이터로 준다 — 평타가 **다음 평타 강화**(재키 W)를 얹을 때. nullptr = 자기 데이터.
	 * @param LevelOverride  SkillOverride 의 레벨. 0 이면 자기 레벨.
	 */
	void ApplySkillDamage(const TArray<AActor*>& Targets, const UERSkillData* SkillOverride = nullptr, int32 LevelOverride = 0);

	/** [4] 적중 후 후처리 — 리캐스트 윈도우 · 다음 평타 강화 부여. OnTargetsResolved 기본 구현이 부른다. 파생이 OnTargetsResolved 를 바꾸면 직접 부른다. */
	void GrantOnHitStates(bool bHitAnything);

	/** 리캐스트 윈도우가 열려 있는가 (자기 슬롯의 Recast.Slot.* 태그). */
	bool IsRecastWindowOpen() const;

	/**
	 * [4] 자기 이동 (F07-06). SkillData.SelfMove 에 따라 ERForcedMove::ApplySelfMove — F06-04 와 같은 경로, 면역 검사만 없음.
	 * 서버에서만. None 이면 아무것도 안 한다. 시작했으면 true.
	 */
	bool ApplySelfMove();

	/** 사거리 상한(m). 기본 = Shape.RangeMax. 평타는 AttackRange 어트리뷰트로 덮는다 (F07-07). ResolveAim 클램프 · 판정 쿼리가 쓴다. */
	virtual float GetRangeMax(const UERSkillData& Skill) const;

	/** [3] 서버가 확정한 조준. 시전자 발밑 기준. ExecuteSkill · 파생이 읽는다. */
	FVector GetAimPoint() const { return AimPoint; }
	FVector GetAimDirection() const { return AimDirection; }
	AActor* GetAimActor() const { return AimActor.Get(); }

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

	void ExecuteAndRecover();

	/** [3] 발동 요청의 TargetData 에서 조준점을 읽고 사거리 [RangeMin, RangeMax] 로 클램프한다. 없으면 시전자 정면. */
	void ResolveAim(const FGameplayEventData* TriggerEventData, const UERSkillData& Skill);

	/** 지금 페이즈 GE. 서버에서만 유효. EndAbility 가 지운다. */
	FActiveGameplayEffectHandle PhaseEffectHandle;

	// [3] 확정된 조준. 매 발동마다 ResolveAim 이 덮어쓴다.
	FVector AimPoint = FVector::ZeroVector;
	FVector AimDirection = FVector::ForwardVector;
	TWeakObjectPtr<AActor> AimActor;

	/** 이 인스턴스의 슬롯 쿨다운 태그 하나 (Cooldown.Slot.Q 등). OnGiveAbility 에서 채운다. */
	FGameplayTagContainer CooldownTags;

	/** 이 인스턴스의 리캐스트 태그 (Recast.Slot.Q 등). 없으면 빈 태그. OnGiveAbility 에서 채운다. */
	FGameplayTag RecastTag;

	/** 이번 발동이 리캐스트(윈도우 소비)였는가 — 쿨다운을 새로 걸지 않는다. ActivateAbility 가 정한다. */
	bool bActivatedByRecast = false;
};
