// F19-04 조각 — 감싸기 → 피해 면역이 끝나면 터짐 (시셀라 W 어디있어 윌슨? · Argument 68 단일안)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_Bubble.generated.h"

class UGameplayEffect;

/**
 * 시전: Duration 초 동안 **피해 면역** (State.DamageImmune — CC 는 받는다 · ERDamageExecution 11) + 이속 GE · 둘 다 State.Sissela.Bubble.
 * 끝: Duration 이 지나면 BurstSkill 판정 (주변 피해). **다시 눌러 터뜨리기는 없다** (사용자 2026-10-06 "피해면역 시간 끝나면 터지는 것").
 * ⚠ 타이머를 어빌리티 **객체에 묶지 않는다** (CreateWeakLambda(A) X) — 엔진이 EndAbility 에서 그 객체의 타이머를 다 지운다
 *   (GameplayAbility.cpp:707 ClearAllTimersForObject · 1차 PIE 에서 안 터졌던 원인 · E40). 약한 포인터만 잡은 람다로.
 */
UCLASS(DisplayName = "감싸기 · 터짐")
class ETERNALRETURN_API UERSkillFragment_Bubble : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** 감싸는 시간 (초) — 1.5 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.1"))
	float Duration = 1.5f;

	/** 이속 GE (자기 버프와 같은 것 — SetByCaller CCDuration · OnHitMagnitude) */
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> MoveSpeedEffect;

	/** 이속 배율 (레벨별) — 1.15 */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> MoveSpeedMagnitude;

	/** 터질 때 판정 (시전자 중심 원 · 피해 조각) */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<const UERSkillData> BurstSkill;

	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual FString GetDebugName() const override { return TEXT("감싸기 · 터짐"); }

};
