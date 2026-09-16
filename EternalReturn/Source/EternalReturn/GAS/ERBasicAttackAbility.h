// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/ERGameplayAbility.h"
#include "ERBasicAttackAbility.generated.h"

/**
 * 기본 공격 (F07-07). **스킬 파이프라인의 파생** — 조준 · 판정 · 피해 · 서버 권위가 전부 베이스 것이다.
 * 근거: Docs/4_Argument/19_평타_다음평타강화_리캐스트_구조.md ①A
 *
 * 베이스와 다른 것 셋:
 *   - 쿨다운 길이 = 1 / AttackSpeed         (SkillData.Cooldowns 무시 · 스킬 가속 무관)
 *   - 사거리     = AttackRange 어트리뷰트  (Shape.RangeMax 무시 · 무기가 정한다, F11)
 *   - 적중 시 State.NextAttackBuff 가 있으면 **그 스킬의 피해 · 적중 효과를 얹고 소비**한다 (재키 W 등)
 *
 * 애셋: DA_Attack_<무기> — SlotTag=Ability.Slot.Attack, Shape=SingleTarget, DamageType=BasicAttack, InitialLevel=1, bUsesSkillPoints=false.
 * 차단: 생성자가 State.Block.BasicAttack 을 ActivationBlockedTags 에 넣는다 (F06 무장 해제 축) — 강화 소비가 안에 있으니 강화도 같이 막힌다.
 *
 * ⚠ 이번 범위 밖: 오토 어택(우클릭 추적 · 자동 반복) · 무기별 모션.
 */
UCLASS()
class ETERNALRETURN_API UERBasicAttackAbility : public UERGameplayAbility
{
	GENERATED_BODY()

public:
	UERBasicAttackAbility();

protected:
	virtual float GetRangeMax(const UERSkillData& Skill) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void OnTargetsResolved(const FTargetResult& Result) override;
};
