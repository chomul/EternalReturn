// F19-03 조각 — 적중 시 대상에게 지속 피해 중첩 (재키 P 출혈 · Argument 65 D1)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_DoT.generated.h"

/**
 * 패시브가 듣는 적중 이벤트마다 맞은 대상에 **출혈**(UERBleedEffect) 을 쌓는다 — 평타 · 스킬 피해 (부가 피해는 이벤트가 없다).
 * 새로 쌓일 때마다 전체 지속 갱신 · 최대 5 (GE 가 한다). FullStacksWhileTag 가 있으면 한 번에 최대 (아드레날린).
 * 최대에 닿으면 자신에게 OnMaxStacksSelfTag 를 N초 (아드레날린 분비 — 이미 있으면 안 건다).
 */
UCLASS(DisplayName = "지속 피해 걸기")
class ETERNALRETURN_API UERSkillFragment_DoT : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** 중첩 하나가 지속 동안 주는 **총** 고정 피해 (레벨별) — 재키 10/20/30. 틱마다 나눠 준다 */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> DamagePerStack;

	/** 중첩 하나의 총 공격력 계수 (레벨별) — 재키 0.2 */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> APRatioPerStack;

	/** 지속 (초) — 틱은 1초마다 (UERBleedEffect) */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "1"))
	float Duration = 6.f;

	/** 적중 한 번에 쌓는 수 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "1"))
	int32 StacksPerHit = 1;

	/** 이 태그가 자신에게 있으면 한 번에 최대 중첩 (재키 State.Adrenaline) */
	UPROPERTY(EditDefaultsOnly)
	FGameplayTag FullStacksWhileTag;

	/** 대상이 최대 중첩에 닿으면 자신에게 이 태그를 OnMaxStacksSelfDuration 초 (재키 State.Adrenaline) */
	UPROPERTY(EditDefaultsOnly)
	FGameplayTag OnMaxStacksSelfTag;

	UPROPERTY(EditDefaultsOnly)
	TArray<float> OnMaxStacksSelfDuration;

	virtual void OnHitDealt(FERSkillContext& Ctx, AActor* Target, const FGameplayTagContainer& HitTags) const override;
	virtual FString GetDebugName() const override { return TEXT("지속 피해 걸기"); }

	/** [서버] 자신에게 태그를 N초 (UERTimedTagEffect) — 처치 조각도 쓴다 */
	static void ApplySelfTimedTag(UAbilitySystemComponent* ASC, FGameplayTag Tag, float Duration);
};
