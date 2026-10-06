// F11.5 조각 — 적중 효과 (CC · 방어력 감소 등 GE 하나. F07-05 · F11-05 A 망치 ArmorBreak)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "GameplayEffect.h"
#include "ERSkillFragment_OnHitEffect.generated.h"

class UGameplayEffect;

/** 판정 대상 전원에게 GE 하나 (ERCC::ApplyCC). 피해와 독립 — 피해 0 인 유틸리티(카티야 E 둔화)도 이것만으로. */
UCLASS(DisplayName = "적중 효과")
class ETERNALRETURN_API UERSkillFragment_OnHitEffect : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> Effect;

	/** 레벨별 지속 (초). SetByCaller.CCDuration */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> Duration;

	/** 둔화 GE 용 감소율 0~1 (SetByCaller.SlowPercent). 둔화가 아니면 비움. */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> SlowPercent;

	/** GE 가 정하는 크기 (SetByCaller.OnHitMagnitude — 망치 방어력 ×0.9 등). */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> Magnitude;

	/** 시전자 스킬 증폭 1 당 크기 + 이 값 (레니 W 아군 이속 "스킬 증폭의 2.5%" — 배율이면 0.00025) */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> MagnitudePerSkillAmp;

	/** 이만큼 뒤에 건다 (초) — 레니 W 아군: 0.15초 둔화 뒤 이속 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float StartDelay = 0.f;

	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const override;
	virtual FString GetDebugName() const override { return TEXT("적중 효과"); }
	virtual bool IsHitEffect() const override { return true; }
};
