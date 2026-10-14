// F20 조각 — 적 쪽으로 움직이는 동안 이속 (전술 스트라이더 · Argument 80)

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Engine/TimerHandle.h"
#include "GameplayTagContainer.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_ChaseSpeed.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UERSkillFragment_ChaseSpeed;

/** 검사 중 상태 — 어빌리티 인스턴스에 붙는다. 어빌리티는 바로 끝나도 WhileTag 가 있는 동안 주기 검사. */
UCLASS()
class UERChaseSpeedState : public UERSkillFragmentState
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<const UERSkillFragment_ChaseSpeed> Frag;
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	TWeakObjectPtr<AActor> Avatar;
	TWeakObjectPtr<const UObject> Skill;
	int32 Level = 1;
	FTimerHandle Timer;
	/** 지금 걸려 있는 이속 GE — 적 쪽이 아니면 뗀다 */
	FActiveGameplayEffectHandle Buff;

	void Start();
	void Stop();
	void Check();
};

/**
 * WhileTag 가 있는 동안 CheckInterval 마다: 시전자가 **Radius 안 적 실험체 쪽으로 움직이고 있으면** (이동 방향과 그 적 방향의 각 ≤ MaxAngleDeg) Effect 를 걸고, 아니면 뗀다.
 * 스트라이더 "5초 동안 10m 안 적 실험체를 향해 이동하면 이속 30 / 40% (원거리 20 / 30%)" — 근/원거리는 조각 두 개 + 조건 AttackRangeCond.
 * 서버 권위 (MoveSpeed 어트리뷰트가 복제된다).
 */
UCLASS(DisplayName = "적 쪽으로 이동 시 이속")
class ETERNALRETURN_API UERSkillFragment_ChaseSpeed : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** 이 태그가 있는 동안만 (같은 스킬의 시간 태그 조각이 건다) */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "State"))
	FGameplayTag WhileTag;

	/** 적 실험체를 찾는 반경 (m) */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float Radius = 10.f;

	/** "적 쪽" 으로 보는 최대 각 (도) `[자체]` — 원작 기준을 못 찾음 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", ClampMax = "180"))
	float MaxAngleDeg = 60.f;

	/** 검사 주기 (초) */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.05"))
	float CheckInterval = 0.2f;

	/** 이속 GE (SetByCaller.CCDuration · OnHitMagnitude) — GE_Wild_MoveSpeedUp */
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> Effect;

	/** 레벨별 크기 (1.3 = +30%) */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> Magnitude;

	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual FString GetDebugName() const override { return TEXT("적 쪽으로 이동 시 이속"); }
};
