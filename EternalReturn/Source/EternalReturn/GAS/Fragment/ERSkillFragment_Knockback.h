// F11.5 조각 — 넉백 + 벽 충돌 (F11-05 B 방망이)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_Knockback.generated.h"

class UERSkillData;

/**
 * 판정 대상을 **시전자가 바라보는 방향**으로 민다 (원작 12시즌: 맞는 각도 무관 — 나무위키 방망이). ERForcedMove 경로 · 면역 검사.
 * WallImpactSkill 이 있으면 벽에 부딪힌 대상에게 그 데이터의 적중 조각(피해 · 기절)을 한 번 더 (ApplyOnTargets).
 */
UCLASS(DisplayName = "넉백")
class ETERNALRETURN_API UERSkillFragment_Knockback : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float Distance = 3.f;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.01"))
	float Duration = 0.3f;

	/** 벽 충돌 시 대상에게 실행할 데이터 (피해 · 적중 효과 조각만 쓴다). */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UERSkillData> WallImpactSkill;

	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const override;
	virtual FString GetDebugName() const override { return TEXT("넉백"); }
};
