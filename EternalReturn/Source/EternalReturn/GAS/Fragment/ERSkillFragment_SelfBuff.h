// F11.5 조각 — 시전 시 자기 버프 (F11-05 B 권총 이속/공속 · C 단검 망토)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "GAS/ERSkillData.h"
#include "ERSkillFragment_SelfBuff.generated.h"

/** 적중과 무관하게 시전자에게 GE 들 (FERSelfEffect: 지속 · 크기 · N회 소비 · 지연). */
UCLASS(DisplayName = "자기 버프")
class ETERNALRETURN_API UERSkillFragment_SelfBuff : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TArray<FERSelfEffect> Effects;

	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual FString GetDebugName() const override { return TEXT("자기 버프"); }
};
