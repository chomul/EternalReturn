// Copyright Epic Games, Inc. All Rights Reserved.
//
// 스킬 "어떻게" — 발사 방식 (Argument 57 S3.1 · 사용자 2026-10-02 "어떻게가 다양해질 수도").
//   즉시 판정 / 날아가는 투사체. 새 발사 방식 = 이 베이스를 상속한 클래스 하나.
//   ⚠ 실행 상태 금지 — 스킬 DA 안의 설정 객체 (S3.1 ②). 현재 발 · 타이머 · 대상은 어빌리티 · 투사체 액터가 든다.
//   ⚠ 순차 · 동시 · 한 발은 클래스가 아니라 투사체의 FirePattern 칸 (S3.1 ① — 조합마다 클래스가 늘어나는 걸 막는다).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ERSkillDelivery.generated.h"

class AActor;
class AERProjectileBase;
class FDataValidationContext;
class UERGameplayAbility;
class UERSkillData;
struct FERShapeContext;
struct FTargetQuery;

UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class ETERNALRETURN_API UERSkillDelivery : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * [서버] 판정 · 발사. 즉시면 결과를 Ability.ResolveInstantHits 로 · 투사체면 쏘고 적중은 도착 때 (Ability.ApplyProjectileHit).
	 * Q = 모양이 만든 질의 (조준 보조 포함) · Ctx = 같은 문맥 (모양 Query 에 다시 넘긴다).
	 */
	virtual void Deliver(UERGameplayAbility& Ability, const UERSkillData& Skill, const FTargetQuery& Q, const FERShapeContext& Ctx) const PURE_VIRTUAL(UERSkillDelivery::Deliver, );

	/** 한 명만 맞히는 발사인가 — 흡혈 감소 "광역" 판정 (F03-05). 비관통 한 발 투사체. */
	virtual bool IsSingleHit() const { return false; }

	/** 판정 순간 애니(Execute 섹션)를 발사 방식이 직접 넘기나 — 순차 사격은 발마다 조준 → 사격이라 어빌리티가 판정 순간에 넘기지 않는다 (K8). */
	virtual bool DrivesAnimSections() const { return false; }

	/** 로그 한 줄 (이관 · 임포트) */
	virtual FString Describe() const { return GetClass()->GetName().Replace(TEXT("ERDelivery_"), TEXT("")); }

#if WITH_EDITOR
	/** 데이터 검사 (IsDataValid 가 부른다). MaxTargets 는 대상 칸이라 스킬이 같이 넘긴다. */
	virtual void ValidateDelivery(FDataValidationContext& Context, const FString& Owner, int32 MaxTargets) const {}
#endif
};

/** 즉시 판정 — 판정 시점에 모양 안을 바로 맞힌다. */
UCLASS(DisplayName = "즉시")
class ETERNALRETURN_API UERDelivery_Instant : public UERSkillDelivery
{
	GENERATED_BODY()
public:
	/** 같은 모양을 조준 방향 가운데로 부채처럼 여러 번 (위클라인 트리플렛 3줄 · F12.6-06). 합친다 — 같은 대상은 한 번. */
	UPROPERTY(EditDefaultsOnly, Category = "발사", meta = (ClampMin = "1"))
	int32 FanCount = 1;
	/** 줄 사이 각도(도) */
	UPROPERTY(EditDefaultsOnly, Category = "발사", meta = (ClampMin = "0", ClampMax = "180", EditCondition = "FanCount > 1", EditConditionHides))
	float FanAngleDeg = 15.f;

	virtual void Deliver(UERGameplayAbility& Ability, const UERSkillData& Skill, const FTargetQuery& Q, const FERShapeContext& Ctx) const override;
	virtual FString Describe() const override;
};

/** 투사체를 어떻게 쏘나. */
UENUM()
enum class EERFirePattern : uint8
{
	/** 조준 방향으로 한 발 */
	Single       UMETA(DisplayName = "한 발"),
	/** Count 발을 부채처럼 동시에 */
	Simultaneous UMETA(DisplayName = "동시 여러 발"),
	/** 모양 안을 스캔 → 잡힌 대상마다 한 발씩 Interval 간격으로 (카티야 R) */
	Sequential   UMETA(DisplayName = "대상마다 순차"),
};

/** 날아가는 투사체 — 적중 조각은 도착 때 투사체가 돌린다 (Argument 54 P1). */
UCLASS(DisplayName = "투사체")
class ETERNALRETURN_API UERDelivery_Projectile : public UERSkillDelivery
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = "발사")
	EERFirePattern FirePattern = EERFirePattern::Single;

	/** 속도(m/s) */
	UPROPERTY(EditDefaultsOnly, Category = "발사", meta = (ClampMin = "0.1"))
	float Speed = 20.f;

	/** 투사체 반경(m) — 날아가며 스윕 */
	UPROPERTY(EditDefaultsOnly, Category = "발사", meta = (ClampMin = "0"))
	float Radius = 0.25f;

	/** 관통 — 끄면 첫 적중에서 사라진다 */
	UPROPERTY(EditDefaultsOnly, Category = "발사")
	bool bPierce = false;

	/** 대상을 따라간다 (순차 사격 — 인식된 사람은 움직여도 따라감 · 사용자 2026-10-02) */
	UPROPERTY(EditDefaultsOnly, Category = "발사")
	bool bHoming = false;

	/** 따라가는 대상만 맞는다 — 길의 다른 적은 지나친다 (원거리 평타 · Argument 60 H1). 끄면 길에서 처음 닿은 적 (카티야 R) */
	UPROPERTY(EditDefaultsOnly, Category = "발사", meta = (EditCondition = "bHoming"))
	bool bHitOnlyTarget = false;

	/** 모습 · 비행 BP. 비우면 기본 (따라가면 AERProjectile_Homing · 아니면 AERProjectileBase) */
	UPROPERTY(EditDefaultsOnly, Category = "발사")
	TSubclassOf<AERProjectileBase> ProjectileClass;

	/** 동시 발 수 */
	UPROPERTY(EditDefaultsOnly, Category = "발사", meta = (ClampMin = "1", EditCondition = "FirePattern == EERFirePattern::Simultaneous", EditConditionHides))
	int32 Count = 1;
	/** 발 사이 각도(도) */
	UPROPERTY(EditDefaultsOnly, Category = "발사", meta = (ClampMin = "0", ClampMax = "180", EditCondition = "FirePattern == EERFirePattern::Simultaneous", EditConditionHides))
	float SpreadAngleDeg = 15.f;

	/** 발 사이 간격(초) — 사격 애니 길이 (`@Length(…)`) */
	UPROPERTY(EditDefaultsOnly, Category = "발사", meta = (ClampMin = "0", EditCondition = "FirePattern == EERFirePattern::Sequential", EditConditionHides))
	float Interval = 0.15f;
	/**
	 * 발 사이 조준 시간(초) — 앞 발의 사격 애니(Interval)가 끝나면 조준 자세(몽타주 `Loop` 섹션) + 조준음(`Pres.Sfx.SkillAim` 발 번호)을 이만큼 → 다음 발.
	 * 다음 발 = 앞 발 + Interval + AimTime. 0 = 조준 없음 (지금까지와 같다). `@Length(조준 루프 애니)` 로 (사용자 2026-10-02 "Fire → Loop 조준 → Fire")
	 */
	UPROPERTY(EditDefaultsOnly, Category = "발사", meta = (ClampMin = "0", EditCondition = "FirePattern == EERFirePattern::Sequential", EditConditionHides))
	float AimTime = 0.f;
	/** 대상이 이 거리(m)보다 멀어지면 그 발은 안 쏜다. 0 = 끔 */
	UPROPERTY(EditDefaultsOnly, Category = "발사", meta = (ClampMin = "0", EditCondition = "FirePattern == EERFirePattern::Sequential", EditConditionHides))
	float CancelDistance = 0.f;

	virtual void Deliver(UERGameplayAbility& Ability, const UERSkillData& Skill, const FTargetQuery& Q, const FERShapeContext& Ctx) const override;
	virtual bool IsSingleHit() const override { return !bPierce && FirePattern == EERFirePattern::Single; }
	virtual bool DrivesAnimSections() const override { return FirePattern == EERFirePattern::Sequential; }
	virtual FString Describe() const override;
#if WITH_EDITOR
	virtual void ValidateDelivery(FDataValidationContext& Context, const FString& Owner, int32 MaxTargets) const override;
#endif

private:
	/** 비우면 기본 클래스 */
	UClass* ResolveClass() const;
	/** 순차 — 스캔 → 대상마다 한 발씩 Interval 간격 (카티야 R) */
	void FireSequential(UERGameplayAbility& Ability, const UERSkillData& Skill, const FTargetQuery& Q, const FERShapeContext& Ctx) const;
	/** 순차의 한 발 — 대상이 CancelDistance 밖이거나 사라졌으면 건너뛴다 */
	void FireShotAt(UERGameplayAbility& Ability, const UERSkillData& Skill, AActor* Target, int32 ShotIndex, int32 Level, const FTargetQuery& Filter) const;
};
