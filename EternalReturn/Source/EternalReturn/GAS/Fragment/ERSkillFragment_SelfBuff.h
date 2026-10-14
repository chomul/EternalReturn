// F11.5 조각 — 시전 시 자기 버프 (F11-05 B 권총 이속/공속 · C 단검 망토)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "GAS/ERSkillData.h"
#include "ERSkillFragment_SelfBuff.generated.h"

/** 시전자에게 GE 들 (FERSelfEffect: 지속 · 크기 · N회 소비 · 지연). 기본은 적중과 무관 · bOnlyOnHit 면 맞혔을 때만. */
UCLASS(DisplayName = "자기 버프")
class ETERNALRETURN_API UERSkillFragment_SelfBuff : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TArray<FERSelfEffect> Effects;

	/** 맞혔을 때만 — 날아가는 투사체면 **도착해서 맞은 때** (카티야 Q 공속 · F19-01). 끄면 시전하자마자 (지금까지와 같다). */
	UPROPERTY(EditDefaultsOnly)
	bool bOnlyOnHit = false;

	/**
	 * 맞힌 수 비례 — 첫 대상 뒤 **추가 대상마다** 크기 + 이 값 (진실의 칼날 이속 0.05 / 0.1 · Argument 80).
	 * 쓰면 시전 때가 아니라 **판정 뒤** 에 건다 (0명이어도 기본 크기로). 비우면 지금까지와 같다.
	 */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> MagnitudePerExtraHit;

	/** 위로 늘린 크기의 상한 (1.3 = +30%). 0 = 상한 없음 */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> MagnitudeMax;

	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const override;
	/** 맞혔을 때만이면 적중 조각 — 장판 · 투사체 도착(ApplyOnTargets)에서도 돈다. */
	virtual bool IsHitEffect() const override { return bOnlyOnHit; }
	virtual FString GetDebugName() const override { return TEXT("자기 버프"); }

private:
	/** HitCount < 0 = 맞힌 수 비례 안 씀 */
	void ApplyEffects(FERSkillContext& Ctx, int32 HitCount = -1) const;
	bool ScalesByHits() const { return !MagnitudePerExtraHit.IsEmpty(); }
};
