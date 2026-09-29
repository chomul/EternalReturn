// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/ERAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UERAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 게임 스레드 — 읽기만. 시뮬레이티드 프록시도 복제된 이동으로 속도가 들어온다 (ACharacter::PostNetReceiveVelocity).
	const AActor* Owner = GetOwningActor();
	GatheredVelocity = Owner ? Owner->GetVelocity() : FVector::ZeroVector;
	bInMode = bPendingInMode;
	if (bPendingModeAnims)
	{
		bPendingModeAnims = false;
		ModeStartAnim = PendingModeAnims[0];
		ModeIdleAnim = PendingModeAnims[1];
		ModeRunAnim = PendingModeAnims[2];
		ModeEndAnim = PendingModeAnims[3];
	}
	// ⭐ 동작 몽타주(평타 · 스킬 · 채집 …)는 **움직이면 끊는다** — 모든 머신이 자기 화면 속도로 (Argument 48 ③ M1).
	//   · 몽타주가 시작된 뒤 한 번 멈춘 적이 있어야 끊는다 — 도착하며 감속 중에 시작한 평타 · 복제 지연된 시뮬레이티드 프록시가 바로 끊기지 않게
	//   · 루트모션 소스(자기 이동 돌진 · 블링크 · 넉백) 중에는 안 끊는다 — 그 이동은 스킬의 일부다. 모든 머신에 소스가 있다 (ERForcedMove Multicast)
	UAnimMontage* Current = GetCurrentActiveMontage();
	if (Current != WatchedMontage)
	{
		WatchedMontage = Current;
		bWatchedSeenStill = false;
	}
	if (Current)
	{
		const ACharacter* Character = Cast<ACharacter>(Owner);
		const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
		if (GatheredVelocity.Size2D() <= MoveThreshold)
		{
			bWatchedSeenStill = true;
		}
		else if (bWatchedSeenStill && !(Move && Move->HasRootMotionSources()))
		{
			Montage_Stop(MoveStopBlendOut, Current);
			WatchedMontage = nullptr;
		}
	}
	if (bPendingDead && !bDead && PendingDeathAnim)
	{
		DeathAnim = PendingDeathAnim;
		DeathStartPosition = bPendingDeathSkipToEnd ? DeathAnim->GetPlayLength() : 0.f;
		bDead = true;
	}
}

void UERAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	// 워커 스레드 — 복사본만 쓴다.
	GroundSpeed = GatheredVelocity.Size2D();
	bIsMoving = GroundSpeed > MoveThreshold;
}
