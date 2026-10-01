// F11.5 조각 — 다음 기본 공격 강화 (F07-07 재키 W · 카티야 P)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_NextAttackBuff.generated.h"

/**
 * 사용 후 State.NextAttackBuff 를 건다 — 다음 평타가 이 스킬의 피해 · 적중 조각을 얹고 소비한다 (UERBasicAttackAbility · Argument 19 ②A).
 * 적중 여부와 무관. Duration 0 = 만료 없음.
 */
UCLASS(DisplayName = "다음 평타 강화")
class ETERNALRETURN_API UERSkillFragment_NextAttackBuff : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float Duration = 0.f;

	/**
	 * 강화 내용(피해 · 적중 조각)을 **다른 스킬 DA** 에서 — 레벨은 그 스킬의 지금 레벨 (카티야 Q · E → P 잿빛 사신 · F19-01).
	 * 비우면 이 스킬 자신 (지금까지와 같다 — 재키 W). 그 스킬을 아직 안 배웠으면 강화를 걸지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<const UERSkillData> BuffSkill;

	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const override;
	virtual FString GetDebugName() const override { return TEXT("다음 평타 강화"); }
};
