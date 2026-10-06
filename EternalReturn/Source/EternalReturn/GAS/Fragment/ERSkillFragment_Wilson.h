// F19-04 조각 — 시셀라 윌슨 (Q 던지기 · E 몸 뻗기 · W 합침 · Argument 68 W1)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_Wilson.generated.h"

class AERWilson;
class UERSkillData;

/**
 * Q 윌슨! 도와줘 — 데이터 + 던질 때 한 가지.
 * 날아가는 건 투사체 `AERProjectile_Wilson` (관통 · 조준점에서 멈춤 · 경로 피해 = 이 DA 의 피해 조각).
 * 착지하면 투사체가 이 조각을 읽어 **착지 범위 피해(LandSkill)** + 윌슨을 내려놓는다 (AERWilson::Drop).
 * OnExecute: 시셀라가 E 로 끌려가는 중이면 도착 합침을 미룬다 → 착지 자리까지 다시 끈다 (결정 7).
 */
UCLASS(DisplayName = "윌슨 던지기")
class ETERNALRETURN_API UERSkillFragment_WilsonThrow : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** 착지 범위 피해 데이터 (피해 조각만 쓴다 · 레벨은 Q 레벨) */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<const UERSkillData> LandSkill;

	/** 착지 범위 반경 (m) [자체 — 원작 미확인] */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float LandRadius = 2.f;

	/** 윌슨 모습 BP (비우면 C++ 기본 — 안 보임) */
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AERWilson> WilsonClass;

	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual bool GetPreviewOrigin(const AActor* Avatar, FVector& OutOrigin, bool& bOutFullReach) const override;
	virtual FString GetDebugName() const override { return TEXT("윌슨 던지기"); }
};

/**
 * E 나랑 놀자 — 데이터. 날아가는 건 `AERProjectile_WilsonTether` (윌슨이 떨어져 있으면 **윌슨 자리에서** 커서 쪽으로).
 * 적을 맞히면: 피해 · 기절 (이 DA 의 피해 · 적중 효과 조각) + 윌슨 쪽으로 끌어옴.
 * 시셀라를 맞히면 (윌슨이 떨어져 있을 때만): 시셀라가 윌슨에게 끌려감 → 합침 ("E") · 보호막 · E 쿨 −N초.
 */
UCLASS(DisplayName = "윌슨 몸 뻗기")
class ETERNALRETURN_API UERSkillFragment_WilsonTether : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** 끌어오는 속도 (m/s) [자체] — 적 · 시셀라 둘 다 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "1"))
	float PullSpeed = 15.f;

	/** 적을 윌슨 몸에서 이만큼 떨어진 곳까지 (m) [자체 — 겹치지 않게] */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float PullStopShort = 1.f;

	/** 시셀라 적중 — 보호막 (레벨별) 60/105/150/195/240 */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> ShieldBase;

	/** 보호막 스킬 증폭 계수 0.2 */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> ShieldSkillAmpRatio;

	/** 보호막 지속 (초) 2.5 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float ShieldDuration = 2.5f;

	/** 시셀라 적중 — 이 스킬 쿨다운 −N초 (2) */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float SelfHitCooldownCut = 2.f;

	virtual bool GetPreviewOrigin(const AActor* Avatar, FVector& OutOrigin, bool& bOutFullReach) const override;
	virtual FString GetDebugName() const override { return TEXT("윌슨 몸 뻗기"); }
};

/**
 * W 어디있어 윌슨? — 윌슨이 **떨어져 있으면** 합침 ("W" → 패시브 장전). 붙어 있으면 아무것도 (사용자 2026-10-06 결정 4 — 장전 없음).
 * 리캐스트(다시 눌러 터뜨리기)로 다시 돌 때는 안 한다.
 */
UCLASS(DisplayName = "윌슨 합치기")
class ETERNALRETURN_API UERSkillFragment_WilsonJoin : public UERSkillFragment
{
	GENERATED_BODY()

public:
	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual FString GetDebugName() const override { return TEXT("윌슨 합치기"); }
};
