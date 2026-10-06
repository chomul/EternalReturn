// F19-04 조각 — 판정 뒤 패시브 효과를 N초 강화 (시셀라 R "패시브 효과 +100%" 7/8/9초)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_PassiveBoost.generated.h"

class UERSkillData;

/**
 * 판정 뒤(대상이 없어도) PassiveSkill 의 `잃은 체력 비례 스탯` 을 Duration 초 하나 더 건다 — 둘이 더해져 +100% (Argument 68 P1).
 * 레벨은 패시브 레벨 · 시간은 이 스킬 레벨.
 */
UCLASS(DisplayName = "패시브 강화")
class ETERNALRETURN_API UERSkillFragment_PassiveBoost : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<const UERSkillData> PassiveSkill;

	/** 강화 시간 (이 스킬 레벨별) — 7/8/9 */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> Duration;

	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const override;
	virtual FString GetDebugName() const override { return TEXT("패시브 강화"); }
};
