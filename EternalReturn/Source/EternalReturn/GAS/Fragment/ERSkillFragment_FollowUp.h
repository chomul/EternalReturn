// F11.5 조각 — 지연 2차 판정 (F11-05 C 투척 연막)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_FollowUp.generated.h"

class UERSkillData;

/**
 * Delay 뒤 같은 조준점에서 Skill 로 판정 + 적중 조각 (ExecuteOther — OnExecute 조각은 안 돈다).
 * 이미 던진 연막이라 시전자가 기절해도 터진다 — 람다 타이머 (E19: EndAbility 가 this 타이머를 지운다).
 */
UCLASS(DisplayName = "2차 판정")
class ETERNALRETURN_API UERSkillFragment_FollowUp : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UERSkillData> Skill;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.01"))
	float Delay = 0.5f;

	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const override;
	virtual FString GetDebugName() const override { return TEXT("2차 판정"); }
};
