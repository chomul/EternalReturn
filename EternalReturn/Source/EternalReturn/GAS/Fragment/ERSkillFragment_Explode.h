// F19-05 조각 — 투사체가 끝나면 그 자리에서 폭발 (레니 Q 당근! 바주카 · Argument 70)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_Explode.generated.h"

class UERSkillData;

/**
 * 데이터만. 날아가는 건 `AERProjectile_Explode` (비관통 — 첫 적 · 아군에서 끝 · 아니면 사거리 끝).
 * 끝난 자리 반경 Radius 안 (판정 팀 그대로 · 보통 All) 에 ExplodeSkill 의 적중 조각 — 적 · 아군 나누기 데이터를 넣는다.
 * bIncludeCaster = 시전자도 반경 안이면 대상 목록에 (레니 Q 회복 — "가까이 있으면 레니도")
 */
UCLASS(DisplayName = "끝에서 폭발")
class ETERNALRETURN_API UERSkillFragment_Explode : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<const UERSkillData> ExplodeSkill;

	/** 폭발 반경 (m) */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float Radius = 2.5f;

	UPROPERTY(EditDefaultsOnly)
	bool bIncludeCaster = false;

	virtual FString GetDebugName() const override { return TEXT("끝에서 폭발"); }
};
