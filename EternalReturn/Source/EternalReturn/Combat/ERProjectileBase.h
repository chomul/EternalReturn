// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/ERTargetingTypes.h"
#include "ERProjectileBase.generated.h"

class UERGameplayAbility;
class UERSkillData;
class USphereComponent;
class UProjectileMovementComponent;

/** 발사 값 — 스폰 때 **한 번** 복제된다. 클라는 이 값으로 같은 비행을 스스로 그린다 (위치 복제 없음). */
USTRUCT()
struct FERProjectileLaunch
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize10 Start = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantizeNormal Direction = FVector::ForwardVector;

	/** cm/s */
	UPROPERTY()
	float SpeedUU = 0.f;

	/** 이만큼 날면 끝 (cm) */
	UPROPERTY()
	float RangeUU = 0.f;

	/** 판정 · 벽 충돌 반경 (cm) */
	UPROPERTY()
	float RadiusUU = 25.f;

	/** 따라갈 대상 (AERProjectile_Homing) — 비면 직선 */
	UPROPERTY()
	TObjectPtr<AActor> HomingTarget = nullptr;

	/** 따라가는 대상만 맞는다 — 길의 다른 적은 지나친다 (원거리 평타 · Argument 60 H1). 서버 판정만 쓴다 */
	UPROPERTY()
	bool bOnlyHomingTarget = false;
};

/**
 * 날아가는 스킬 투사체 베이스 (F19-01 · Argument 54 P1). 캐릭터가 늘면 **상속**해서 종류를 늘린다 (사용자 2026-10-01).
 *
 * - 서버가 스폰 → 엔진 `ProjectileMovementComponent` 로 직선 비행 · **벽 통과** (원작 · 사용자 2026-10-01)
 * - [서버] 매 틱 지나온 구간을 `ERTargeting::SweepSegment` 로 쓸어 대상 판정 (필터는 다른 판정 모양과 같은 함수)
 *   → 맞으면 시전 어빌리티의 적중 조각을 **도착 때** 돌린다 (`ApplyProjectileHit` — 장판 액터와 같은 길)
 * - 복제: 발사 값만 (`bReplicateMovement = false`). 서버 소멸 → 클라 소멸
 * - 모습(메시 · 이펙트)은 BP 자식이 붙인다 — 로직 없음 (CLAUDE.md §7)
 */
UCLASS()
class ETERNALRETURN_API AERProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	AERProjectileBase();

	/** [서버] SpawnActorDeferred 뒤 · FinishSpawning 전에 한 번. */
	virtual void InitLaunch(UERGameplayAbility* InAbility, const UERSkillData* InSkill, int32 InLevel, const FTargetQuery& InFilter, const FERProjectileLaunch& InLaunch, int32 InShotIndex = -1, bool bInPierce = false);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	/** 비행 시작 — 서버 · 클라 둘 다 (발사 값이 오면). 파생이 이동 방식을 바꿀 수 있다. */
	virtual void StartFlight();
	/** 팀 필터를 통과한 대상에 대해 더 거를 것 (관통 규칙 등). 기본: 이미 맞은 대상 제외. */
	virtual bool CanHit(const AActor* Target) const;
	/** [서버] 맞힘 — 기본: 적중 조각 + 타격음 · 비관통이면 끝. */
	virtual void OnHitTarget(AActor* Target);
	/** 끝 — 서버는 소멸, 클라는 숨기고 멈춤 (복제 액터를 클라가 지우지 않는다). */
	virtual void EndFlight(const TCHAR* Why);

	UFUNCTION()
	void OnRep_Launch();
	UFUNCTION()
	void OnMovementStop(const FHitResult& ImpactResult);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(ReplicatedUsing = OnRep_Launch)
	FERProjectileLaunch Launch;

	// ── 서버만 ──
	TWeakObjectPtr<UERGameplayAbility> Ability;
	UPROPERTY()
	TObjectPtr<const UERSkillData> Skill;
	int32 Level = 1;
	/** 측정 — 비행 틱 수 (서버 · 적중 로그에 비행 시간과 같이 · 2026-10-03 비행 시간이 들쭉날쭉) */
	int32 FlightTicks = 0;
	/** 몇 번째 발 (순차 사격 · 카티야 R) — 피해 조각 ShotValues. 그 외 -1 */
	int32 ShotIndex = -1;
	/** 관통 — 끄면 첫 적중에서 끝 (발사 방식 bPierce) */
	bool bPierce = false;
	/** 팀 · 시전자 · 이미 맞은 대상 (IgnoredActors) — 액터 포인터를 들고 있어 UPROPERTY (GC 가 지워진 액터를 null 로) */
	UPROPERTY()
	FTargetQuery Filter;

	// ── 서버 · 클라 ──
	FVector LastLocation = FVector::ZeroVector;
	/** 지금까지 날아간 거리 (cm) — 거리 비례 피해(카티야 Q) 가 읽는다 */
	float TraveledUU = 0.f;
	bool bFlying = false;
	bool bEnded = false;
};
