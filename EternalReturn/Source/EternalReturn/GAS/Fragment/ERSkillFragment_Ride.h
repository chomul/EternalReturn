// F19-02 조각 — 탑승 (매그너스 R 폭주 바이크 · Argument 62 V3)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_Ride.generated.h"

class UGameplayEffect;
class UERSkillData;
class UERGameplayAbility;

/**
 * 탑승 — 시전 때 탑승 GE (지속 · 최고 속도 Override · 태그 State.Riding · 면역 · 차단) + 캐릭터의 UERRideComponent 가 조종 · 충돌.
 * bDismount = 재사용 데이터 (바이크만 발사) 용 — 탑승 GE 를 지우고 내린다 (발사는 그 데이터의 발사 방식 · AERProjectile_Bike).
 * 폭발 = ExplodeAt — 중심 반경 ExplodeRadius 안 ExplodeSkill 대상에게 ExplodeSkill 의 적중 조각 (피해 · 현재 체력 30%).
 * 회전 속도 · 폭발 반경 · 충돌 감지 반경은 `[자체]` (원작 미확인).
 */
UCLASS(DisplayName = "탑승")
class ETERNALRETURN_API UERSkillFragment_Ride : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** 탑승 GE (GE_Magnus_Ride) — 지속 SetByCaller.CCDuration · MoveSpeed Override SetByCaller.OnHitMagnitude · 태그 State.Riding 등 */
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> RideEffect;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.1"))
	float Duration = 7.f;

	/** m/s */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.1"))
	float SpeedStart = 1.f;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.1"))
	float SpeedMax = 8.f;

	/** 최고 속도까지 (초) */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.01"))
	float RampTime = 2.f;

	/** 초당 최대 회전 (도) — 누누 눈덩이처럼 천천히 꺾인다 `[자체]` */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "1"))
	float TurnRateDeg = 120.f;

	/** 몸 앞으로 이만큼(m) 안에 적 실험체 · 벽이면 충돌 `[자체]` */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float HitReach = 0.5f;

	/** 폭발 반경 (m) `[자체]` */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.1"))
	float ExplodeRadius = 2.f;

	/** 폭발 데이터 — 적중 조각(피해) · 대상 칸(팀) 을 쓴다 */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UERSkillData> ExplodeSkill;

	/** 재사용 데이터용 — 시작하지 않고 내린다 */
	UPROPERTY(EditDefaultsOnly)
	bool bDismount = false;

	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual void OnLocalExecute(FERSkillContext& Ctx) const override;
	virtual FString GetDebugName() const override { return TEXT("탑승"); }

	/** [서버] Center 반경 ExplodeRadius 안 대상에게 폭발 피해 — 탑승 충돌 · 발사한 바이크가 같이 쓴다. 맞은 수. */
	int32 ExplodeAt(UERGameplayAbility* Ability, const FVector& Center, const TCHAR* Why) const;
};
