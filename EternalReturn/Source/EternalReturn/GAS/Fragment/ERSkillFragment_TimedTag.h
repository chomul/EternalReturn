// F14 S4 조각 — 시전하면 자기에게 태그 하나를 N초 (빈사 스킬 W 집념 · Argument 79)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_TimedTag.generated.h"

/**
 * 발동 때 시전자에게 Tag 를 Duration 초 (UERTimedTagEffect — GE 애셋 없이 태그만). 효과는 **그 태그를 보는 쪽**이 낸다.
 * 집념: State.DownedSkill.Obstinacy → PlayerState 가 빈사 이속을 올린다 (Override 라 이속 버프 GE 를 얹으면 안 먹는다 — Argument 79 D4).
 */
UCLASS(DisplayName = "시간 태그")
class ETERNALRETURN_API UERSkillFragment_TimedTag : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "State"))
	FGameplayTag Tag;

	/** 같은 시간 동안 같이 거는 태그 — 전술 아티팩트 경직 (무적 + 이동 · 스킬 · 평타 막기 · Argument 80). 비면 Tag 하나 (지금까지와 같다) */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "State"))
	FGameplayTagContainer ExtraTags;

	/** 레벨별 초 */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> Duration;

	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual FString GetDebugName() const override { return TEXT("시간 태그"); }
};
