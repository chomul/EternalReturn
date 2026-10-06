// F19-03 조각 — 태그가 있는 동안 적중마다 추가 피해 · 회복 (재키 P 아드레날린 · Argument 65)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_HitProc.generated.h"

/**
 * 자신에게 WhileTag 가 있는 동안 패시브가 듣는 적중마다 (사용자 2026-10-05 "적중마다"):
 *   ① 맞은 대상에 추가 스킬 피해 (부가 피해 — 적중 이벤트 없음) ② 자신 회복 = 잃은 체력 비례 Min ~ Max · 체력 FullHealBelowHPRatio 이하에서 최대 (사이는 직선 `[자체]`)
 */
UCLASS(DisplayName = "적중 시 추가 효과")
class ETERNALRETURN_API UERSkillFragment_HitProc : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	FGameplayTag WhileTag;

	/** 추가 피해 (레벨별) — 재키 10/25/40 · 공격력 0.14/0.16/0.18 */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> BonusDamage;

	UPROPERTY(EditDefaultsOnly)
	TArray<float> BonusAPRatio;

	/** 회복 최소 (체력 가득) · 최대 (FullHealBelowHPRatio 이하) — 고정 + 공격력 계수 (레벨별) */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> HealMin;

	UPROPERTY(EditDefaultsOnly)
	TArray<float> HealMinAPRatio;

	UPROPERTY(EditDefaultsOnly)
	TArray<float> HealMax;

	UPROPERTY(EditDefaultsOnly)
	TArray<float> HealMaxAPRatio;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float FullHealBelowHPRatio = 0.4f;

	virtual void OnHitDealt(FERSkillContext& Ctx, AActor* Target, const FGameplayTagContainer& HitTags) const override;
	virtual FString GetDebugName() const override { return TEXT("적중 시 추가 효과"); }
};
