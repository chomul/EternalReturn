// Copyright Epic Games, Inc. All Rights Reserved.
//
// 기본 모양 7종 (Argument 57 S3.1 — 옛 ESkillTargeting 의 칸들을 모양마다 뜻 있는 이름으로). 계산은 아래층 ERTargeting 그대로 → 판정 결과 동일.
// 새 모양은 여기 말고 **새 파일**로 (UERSkillShapeBase 상속) — 쓰는 쪽 코드는 안 바뀐다.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Shape/ERSkillShape.h"
#include "ERSkillShapes.generated.h"

/** 지정한 대상 하나 — 평타 · 대상 지정 스킬. 대상이 없으면 발동 안 함. */
UCLASS(DisplayName = "대상 하나")
class ETERNALRETURN_API UERShape_Single : public UERSkillShapeBase
{
	GENERATED_BODY()
public:
	/** 사거리(m) — 시전자 몸 끝 → 대상 표면 (Argument 32). 평타는 무기 사거리 어트리뷰트가 덮는다. */
	UPROPERTY(EditDefaultsOnly, Category = "모양", meta = (ClampMin = "0"))
	float Range = 1.f;

	virtual bool IsSingleTarget() const override { return true; }
	virtual bool RequiresTargetToActivate() const override { return true; }
	virtual void Draw(const UWorld* World, const FTargetQuery& Q, FColor Color, float Life) const override;
	virtual FString Describe() const override;
protected:
	virtual void FillQuery(FTargetQuery& Q, const FERShapeContext& Ctx) const override;
	virtual float GetReachFromCaster() const override { return Range; }
};

/** 원 — 시전자 중심(앞으로 띄우기 가능) 또는 조준점 중심. */
UCLASS(DisplayName = "원")
class ETERNALRETURN_API UERShape_Circle : public UERSkillShapeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = "모양", meta = (ClampMin = "0"))
	float Radius = 3.f;

	/** 중심을 조준 방향 앞으로(m) — 원점이 시전자일 때. 곰 강타. */
	UPROPERTY(EditDefaultsOnly, Category = "모양", meta = (ClampMin = "0", EditCondition = "ShapeOrigin == EERShapeOrigin::Caster", EditConditionHides))
	float ForwardOffset = 0.f;

	virtual void Draw(const UWorld* World, const FTargetQuery& Q, FColor Color, float Life) const override;
	virtual FString Describe() const override;
protected:
	virtual void FillQuery(FTargetQuery& Q, const FERShapeContext& Ctx) const override;
	virtual float GetReachFromCaster() const override { return Radius; }
	virtual float GetForwardOffset() const override { return ForwardOffset; }
};

/** 이중 원 — 안쪽 · 바깥이 다른 결과 (레니 W). 안쪽 대상은 결과의 InnerHitActors. */
UCLASS(DisplayName = "이중 원")
class ETERNALRETURN_API UERShape_DualCircle : public UERSkillShapeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = "모양", meta = (ClampMin = "0"))
	float InnerRadius = 1.f;
	UPROPERTY(EditDefaultsOnly, Category = "모양", meta = (ClampMin = "0"))
	float OuterRadius = 2.f;
	UPROPERTY(EditDefaultsOnly, Category = "모양", meta = (ClampMin = "0", EditCondition = "ShapeOrigin == EERShapeOrigin::Caster", EditConditionHides))
	float ForwardOffset = 0.f;

	virtual void Draw(const UWorld* World, const FTargetQuery& Q, FColor Color, float Life) const override;
	virtual FString Describe() const override;
#if WITH_EDITOR
	virtual void ValidateShape(FDataValidationContext& Context, const FString& Owner) const override;
#endif
protected:
	virtual void FillQuery(FTargetQuery& Q, const FERShapeContext& Ctx) const override;
	virtual float GetReachFromCaster() const override { return OuterRadius; }
	virtual float GetForwardOffset() const override { return ForwardOffset; }
};

/** 부채꼴 — 시전자에서 조준 방향으로. */
UCLASS(DisplayName = "부채꼴")
class ETERNALRETURN_API UERShape_Cone : public UERSkillShapeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = "모양", meta = (ClampMin = "0"))
	float Length = 3.f;
	/** 전체 각도(도) — 반각 아님. 다니엘 Q 65 */
	UPROPERTY(EditDefaultsOnly, Category = "모양", meta = (ClampMin = "0", ClampMax = "360"))
	float AngleDeg = 60.f;

	virtual void Draw(const UWorld* World, const FTargetQuery& Q, FColor Color, float Life) const override;
	virtual FString Describe() const override;
protected:
	virtual void FillQuery(FTargetQuery& Q, const FERShapeContext& Ctx) const override;
	virtual float GetReachFromCaster() const override { return Length; }
};

/** 직선 — 시전자에서 조준 방향으로 길이 × 폭. 첫 하나만 맞히려면 대상 MaxTargets 1. 날아가는 투사체는 발사 방식(Delivery)이 정한다. */
UCLASS(DisplayName = "직선")
class ETERNALRETURN_API UERShape_Line : public UERSkillShapeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = "모양", meta = (ClampMin = "0"))
	float Length = 5.f;
	/** 전체 폭(m) */
	UPROPERTY(EditDefaultsOnly, Category = "모양", meta = (ClampMin = "0"))
	float Width = 0.5f;

	virtual void Draw(const UWorld* World, const FTargetQuery& Q, FColor Color, float Life) const override;
	virtual FString Describe() const override;
protected:
	virtual void FillQuery(FTargetQuery& Q, const FERShapeContext& Ctx) const override;
	virtual float GetReachFromCaster() const override { return Length; }
};

/** 역사다리꼴 — 원점(보통 조준점)이 가운데 · 시전자 쪽 변이 좁다 (카티야 R 스캔 · 사용자 2026-10-01). */
UCLASS(DisplayName = "사다리꼴")
class ETERNALRETURN_API UERShape_Trapezoid : public UERSkillShapeBase
{
	GENERATED_BODY()
public:
	UERShape_Trapezoid() { ShapeOrigin = EERShapeOrigin::AimPoint; }

	UPROPERTY(EditDefaultsOnly, Category = "모양", meta = (ClampMin = "0"))
	float Length = 8.f;
	/** 시전자 쪽 변의 전체 폭(m) */
	UPROPERTY(EditDefaultsOnly, Category = "모양", meta = (ClampMin = "0"))
	float NearWidth = 4.f;
	/** 먼 쪽 변의 전체 폭(m) */
	UPROPERTY(EditDefaultsOnly, Category = "모양", meta = (ClampMin = "0"))
	float FarWidth = 8.f;

	virtual void Draw(const UWorld* World, const FTargetQuery& Q, FColor Color, float Life) const override;
	virtual FString Describe() const override;
protected:
	virtual void FillQuery(FTargetQuery& Q, const FERShapeContext& Ctx) const override;
	virtual float GetReachFromCaster() const override { return Length; }
};

/** 플레이어마다 원 — 시전 시작 때 CaptureRange 안 살아 있는 실험체 자리를 팀 상관없이 저장 → 판정 때 그 자리마다 원 (오메가 VF 방출 · F12.6-05). */
UCLASS(DisplayName = "플레이어마다 원")
class ETERNALRETURN_API UERShape_PlayerCircles : public UERSkillShapeBase
{
	GENERATED_BODY()
public:
	/** 자리를 저장할 범위(m) — 시전자 중심 */
	UPROPERTY(EditDefaultsOnly, Category = "모양", meta = (ClampMin = "0"))
	float CaptureRange = 8.f;
	/** 저장한 자리마다 원 반경(m) */
	UPROPERTY(EditDefaultsOnly, Category = "모양", meta = (ClampMin = "0"))
	float Radius = 2.5f;

	virtual void CaptureAtCastStart(const FERShapeContext& Ctx, TArray<FVector>& OutPoints) const override;
	virtual FTargetResult Query(const UWorld* World, const FTargetQuery& Q, const FERShapeContext& Ctx) const override;
	virtual void Draw(const UWorld* World, const FTargetQuery& Q, FColor Color, float Life) const override;
	virtual FString Describe() const override;
protected:
	virtual void FillQuery(FTargetQuery& Q, const FERShapeContext& Ctx) const override;
	virtual float GetReachFromCaster() const override { return CaptureRange; }
};
