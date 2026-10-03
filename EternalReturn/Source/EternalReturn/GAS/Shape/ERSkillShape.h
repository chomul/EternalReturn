// Copyright Epic Games, Inc. All Rights Reserved.
//
// 스킬 "어디를" — 판정 모양 (Argument 57 S3.1 · 사용자 2026-10-02 "앞으로 모양이 더 다양하게 생길 수 있잖아").
//   새 모양 = 이 베이스를 상속한 클래스 하나 (칸 · 질의 · 그림 · 사거리가 한 클래스). 쓰는 쪽(판정 · AI · 미리보기)은
//   **구체 클래스로 Cast 하지 않는다** — BuildQuery · Query · GetMaxReach · Draw 만 부른다 (S3.1 ③).
//   ⚠ 실행 상태 금지 — 스킬 DA 안의 설정 객체라 여러 캐릭터 · 동시 시전이 같이 쓴다. 모든 함수 const (S3.1 ②).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Combat/ERTargetingTypes.h"
#include "ERSkillShape.generated.h"

struct FERSkillTargets;
class FDataValidationContext;

/** 모양의 원점. */
UENUM(BlueprintType)
enum class EERShapeOrigin : uint8
{
	/** 시전자 발밑 */
	Caster   UMETA(DisplayName = "시전자"),
	/** 조준점(커서) — AimRange 로 당긴다 */
	AimPoint UMETA(DisplayName = "조준점"),
};

/** 모양이 질의를 만들 때 받는 것 — 어빌리티가 채운다. 값만 (소유 안 함). */
struct FERShapeContext
{
	const AActor* Avatar = nullptr;
	FVector AimPoint = FVector::ZeroVector;
	FVector AimDirection = FVector::ForwardVector;
	AActor* DesignatedTarget = nullptr;
	const FERSkillTargets* Targets = nullptr;
	/** 시전 시작 때 모양이 저장한 자리 (PlayerCircles) — 어빌리티가 들고 있다 */
	const TArray<FVector>* CapturedPoints = nullptr;
	/** 질의 RangeMax(m)를 덮는다 — 어빌리티 GetRangeMax (평타 = 무기 사거리 어트리뷰트 · 그 외 = GetMaxReach 와 같다). 0 = 모양 값 */
	float RangeOverride = 0.f;
};

UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class ETERNALRETURN_API UERSkillShapeBase : public UObject
{
	GENERATED_BODY()

public:
	/** 원점 — 시전자 / 조준점(커서). 조준점이면 AimRange 로 당긴다. */
	UPROPERTY(EditDefaultsOnly, Category = "모양")
	EERShapeOrigin ShapeOrigin = EERShapeOrigin::Caster;

	/** 조준점을 당기는 거리(m) — ShapeOrigin 이 조준점일 때. 커서가 더 멀면 이 거리로 (서버 클램프 · 사거리 핵 방지). */
	UPROPERTY(EditDefaultsOnly, Category = "모양", meta = (ClampMin = "0", EditCondition = "ShapeOrigin == EERShapeOrigin::AimPoint", EditConditionHides))
	float AimRange = 0.f;

	/** 최소 거리(m) — 이보다 가까우면 안 된다 (위클라인 통제 2m · 시셀라 Q 1m). 0 = 없음. AI 도 본다. */
	UPROPERTY(EditDefaultsOnly, Category = "모양", meta = (ClampMin = "0"))
	float MinReach = 0.f;

	/** 판정 질의 — 공통(팀 · 원점 · 방향 · 최소 거리)을 채우고 모양 칸은 FillQuery 가. */
	FTargetQuery BuildQuery(const FERShapeContext& Ctx) const;

	/** 판정 — 기본은 아래층 ERTargeting. 자리를 여러 개 쓰는 모양(PlayerCircles)이 바꾼다. */
	virtual FTargetResult Query(const UWorld* World, const FTargetQuery& Q, const FERShapeContext& Ctx) const;

	/** 시전자에서 닿는 최대 거리(m) — 조준점 당기기 · AI 사용 거리 · 미리보기 사거리 원. 원점이 조준점이면 AimRange. */
	float GetMaxReach() const { return ShapeOrigin == EERShapeOrigin::AimPoint ? AimRange : GetReachFromCaster(); }

	/** 한 명만 맞히는 모양인가 — 흡혈 감소 "광역" 판정 (F03-05). */
	virtual bool IsSingleTarget() const { return false; }

	/** 대상이 없으면 발동 자체를 안 하나 (대상 지정 — 쿨다운 · 모션 없음 · 사용자 2026-09-28). */
	virtual bool RequiresTargetToActivate() const { return false; }

	/** 시전 시작 때 자리를 저장하는 모양 (PlayerCircles). 기본 없음. 서버만 부른다. */
	virtual void CaptureAtCastStart(const FERShapeContext& Ctx, TArray<FVector>& OutPoints) const {}

	/** 모양 테두리 — 판정 디버그(시전 뒤)와 미리보기(시전 전)가 같은 그림. Life 0 = 한 프레임. */
	virtual void Draw(const UWorld* World, const FTargetQuery& Q, FColor Color, float Life) const {}

	/** 로그 한 줄 (이관 · 임포트) */
	virtual FString Describe() const;

#if WITH_EDITOR
	/** 데이터 검사 (IsDataValid 가 부른다) — 기본: 최소 거리 > 최대 거리 */
	virtual void ValidateShape(FDataValidationContext& Context, const FString& Owner) const;
#endif

protected:
	/** 모양 칸 → 아래층 질의 (Shape · RangeMax · 반경 …). */
	virtual void FillQuery(FTargetQuery& Q, const FERShapeContext& Ctx) const PURE_VIRTUAL(UERSkillShapeBase::FillQuery, );
	/** 원점이 시전자일 때 닿는 거리(m) */
	virtual float GetReachFromCaster() const PURE_VIRTUAL(UERSkillShapeBase::GetReachFromCaster, return 0.f;);
	/** 원을 조준 방향 앞으로 띄우기(m) — 원 · 이중 원만 */
	virtual float GetForwardOffset() const { return 0.f; }
};
