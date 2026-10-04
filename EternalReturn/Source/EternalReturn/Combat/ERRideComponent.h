// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ERRideComponent.generated.h"

class UERGameplayAbility;
class UERSkillFragment_Ride;

/**
 * 탑승 이동 (F19-02 매그너스 R 폭주 바이크 · Argument 62 V3) — **걷기 이동 그대로 + 입력만 바이크처럼**.
 *
 * - [소유 클라 · 리슨 호스트] 매 틱 진행 방향을 조종 목표(우클릭) 쪽으로 **초당 TurnRateDeg 까지만** 돌리고 그 방향으로 이동 입력.
 *   입력 세기 = 시작 속도 / 최고 속도 → 1 (RampTime) → 엔진이 1 → 8 m/s 로 가속 (최고 속도는 탑승 GE 의 MoveSpeed Override).
 *   이동 입력이 이동 패킷에 실리니 **예측 그대로** — 서버는 재생 · 최고 속도로 막는다 (Argument 11 · ERPlayerController.cpp:627).
 * - [서버] 앞을 쓸어 **적 실험체 · 벽** → 폭발 (탑승 조각 ExplodeAt) → 내림. 끝내는 건 서버만 (탑승 GE 제거 → 태그 State.Riding 이 빠짐).
 * - 탑승 태그가 빠지면 (충돌 · 7초 · 재사용) 양쪽 모두 멈추고 몽타주 End.
 * 틱은 탑승 중에만 켠다 (CLAUDE.md §2).
 */
UCLASS(ClassGroup = (ER))
class ETERNALRETURN_API UERRideComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UERRideComponent();

	/** [서버] 탑승 시작 — 충돌 감시 (탑승 조각 OnExecute). */
	void BeginServer(const UERSkillFragment_Ride* Config, UERGameplayAbility* Ability);
	/** [소유 클라 · 리슨 호스트] 조종 시작 (탑승 조각 OnLocalExecute). */
	void BeginLocal(const UERSkillFragment_Ride* Config);
	/** [서버] 내림 — 탑승 GE · 리캐스트 창 제거 · 몽타주 End. Why 는 로그. */
	void EndServer(const TCHAR* Why);

	/** 조종 중인가 (우클릭이 경로 대신 조종 목표로) */
	bool IsSteering() const { return bLocal && Config != nullptr; }
	/** [소유 클라] 우클릭 지점 — 경로를 시작하지 않고 목표만 바꾼다 (누누 · 사용자 2026-10-03) */
	void SetSteerTarget(const FVector& Point) { SteerTarget = Point; bHasSteerTarget = true; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool HasRidingTag() const;
	void TickSteer(float DeltaTime);
	void TickCollide();
	/** 양쪽 — 멈춤 · 몽타주 End (로컬) */
	void StopLocal();

	UPROPERTY()
	TObjectPtr<const UERSkillFragment_Ride> Config;
	TWeakObjectPtr<UERGameplayAbility> Ability;

	bool bServer = false;
	bool bLocal = false;
	float StartTime = 0.f;
	float HeadingYaw = 0.f;
	FVector SteerTarget = FVector::ZeroVector;
	bool bHasSteerTarget = false;
};
