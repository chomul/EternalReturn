// F19-02 조각 — 적중 시 자기 쿨다운 줄이기 (매그너스 W 17대 1)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_CooldownOnHit.generated.h"

/**
 * 이 스킬이 누군가를 맞힐 때마다 **자기 슬롯 쿨다운**을 Seconds 줄인다 — 판정 한 번(장판은 펄스 한 번)에 한 번 `[자체]`
 * (대상 수만큼이 아니다 — 3명을 맞히면 쿨이 바로 돈다 · Argument 61). 엔진 `ModifyActiveEffectStartTime` (AbilitySystemComponent.h:854).
 */
UCLASS(DisplayName = "적중 시 쿨다운 줄이기")
class ETERNALRETURN_API UERSkillFragment_CooldownOnHit : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** 줄일 초 (레벨별) */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> Seconds;

	virtual bool IsHitEffect() const override { return true; }
	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const override;
	virtual FString GetDebugName() const override { return TEXT("적중 시 쿨다운 줄이기"); }
};
