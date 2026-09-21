// F11.5 조각 — 장판 (F11-05 C 암기 마름쇠)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_Area.generated.h"

/**
 * 판정 대신 조준점에 AERSkillAreaActor 를 스폰 — Duration 초 동안 TickInterval 마다 반경 Radius 안의 적에게
 * 이 데이터의 적중 조각(피해 · 적중 효과)을 준다. 같은 대상은 맞을 때마다 피해가 (1 − DamageDecay)^n (Ctx.DamageScale), 적중 효과는 그대로.
 * ⚠ 반경 · 주기는 자체 결정값 (원문 "일정 범위" · "밟을 때마다").
 */
UCLASS(DisplayName = "장판")
class ETERNALRETURN_API UERSkillFragment_Area : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.1"))
	float Duration = 6.f;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.1"))
	float Radius = 1.5f;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.1"))
	float TickInterval = 1.f;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float DamageDecay = 0.f;

	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual FString GetDebugName() const override { return TEXT("장판"); }
};
