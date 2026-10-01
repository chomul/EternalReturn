// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ERForcedMoveComponent.h"

#include "Combat/ERForcedMove.h"
#include "EternalReturn.h"
#include "GameFramework/Character.h"

UERForcedMoveComponent::UERForcedMoveComponent()
{
	// ⚠ Tick 은 **강제 이동 중에만** 돈다. 평소에는 꺼져 있어 비용이 0 이다.
	//   켜는 곳은 StartWatch 한 곳뿐이고, 벽에 닿거나 넉백이 끝나면 스스로 끈다.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// ⭐ Multicast 가 가려면 컴포넌트가 복제돼야 한다.
	SetIsReplicatedByDefault(true);
}

ACharacter* UERForcedMoveComponent::GetCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

void UERForcedMoveComponent::Multicast_ForcedMove_Implementation(
	const FVector& StartLocation, const FVector& TargetLocation, float Duration, float HeightUU, bool bPassThroughPawns, bool bThroughWalls)
{
	// ⚠ 서버는 ApplyForcedMove 에서 이미 붙였다. 여기서 또 붙이면 두 번 들어간다.
	//   (Multicast 는 서버에서도 실행된다)
	if (GetOwnerRole() == ROLE_Authority)
	{
		return;
	}

	ERForcedMove::AddForcedMoveSource(GetCharacter(), StartLocation, TargetLocation, Duration, HeightUU, bPassThroughPawns, bThroughWalls);
}

void UERForcedMoveComponent::Multicast_StopForcedMove_Implementation()
{
	// ⚠ 여기는 서버도 포함해서 전부 뗀다 — 중단은 서버가 판정만 하고 제거는 각자 한다.
	ERForcedMove::RemoveForcedMoveSource(GetCharacter());
}

void UERForcedMoveComponent::StartWatch()
{
	// ⚠ 벽 판정은 서버 권위다 (역기획서 §3.5). 클라는 Tick 을 돌 이유가 없다 —
	//   클라는 Multicast 로 받은 소스를 CMC 가 재생하기만 한다.
	if (GetOwnerRole() == ROLE_Authority)
	{
		SetComponentTickEnabled(true);
	}
}

void UERForcedMoveComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ACharacter* Character = GetCharacter();

	// ⚠ 이 Tick 은 강제 이동 감시 전용이다. 다른 용도를 여기에 붙이지 않는다 —
	//   붙이면 "평소엔 꺼져 있다" 는 전제가 깨진다.
	// 넉백이 끝났으면(만료 또는 제거) 감시를 멈춘다.
	if (!Character || GetOwnerRole() != ROLE_Authority || !ERForcedMove::IsForcedMoving(Character))
	{
		SetComponentTickEnabled(false);
		return;
	}

	FHitResult Hit;
	if (!ERForcedMove::CheckWallImpact(Character, Hit))
	{
		return;
	}

	// ── 벽에 부딪혔다 ─────────────────────────────────────
	//
	// ⭐ 서버가 판정하고, 각 머신이 소스를 뗀다. 클라가 따로 판정하지 않는다.
	ERForcedMove::RemoveForcedMoveSource(Character);
	Multicast_StopForcedMove();
	SetComponentTickEnabled(false);

	// Log 로 올림 (E34 — Verbose 라 "캐릭터를 벽으로 잡음" 이 안 보였다)
	UE_LOG(LogEternalReturn, Log, TEXT("[강제이동] %s 가 벽에 부딪혔다 — %s (%s)"),
		*GetNameSafe(Character), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()));

	// 추가 피해·기절은 **구독하는 쪽**이 정한다 (스킬마다 다르다).
	OnForcedMoveWallImpact.Broadcast(Character, Hit);
}
