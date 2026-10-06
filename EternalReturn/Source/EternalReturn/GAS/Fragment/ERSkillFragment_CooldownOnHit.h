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

	/**
	 * true = 패시브가 듣는 **적중 이벤트**마다 (재키 P 안의 E 지속 효과 · Argument 65). false = 이 스킬 판정 때 (매그너스 W).
	 * 적중 이벤트에서는 CooldownTags 를 줄인다 (패시브 자신의 쿨다운은 없다).
	 */
	UPROPERTY(EditDefaultsOnly)
	bool bFromHitEvent = false;

	/** 줄일 쿨다운 태그 — 비면 이 스킬 슬롯 (재키: Cooldown.Slot.E) */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "Cooldown"))
	FGameplayTagContainer CooldownTags;

	/** 평타 적중만 (재키 E "기본 공격 적중 시") */
	UPROPERTY(EditDefaultsOnly)
	bool bBasicAttackOnly = false;

	/** 대상이 출혈 최대 중첩일 때만 (재키 E) — 이번 적중으로 쌓이기 **전** 상태 (DA 에서 출혈 조각보다 앞에 둔다) */
	UPROPERTY(EditDefaultsOnly)
	bool bRequireTargetMaxBleed = false;

	virtual bool IsHitEffect() const override { return true; }
	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const override;
	virtual void OnHitDealt(FERSkillContext& Ctx, AActor* Target, const FGameplayTagContainer& HitTags) const override;
	virtual FString GetDebugName() const override { return TEXT("적중 시 쿨다운 줄이기"); }
};
