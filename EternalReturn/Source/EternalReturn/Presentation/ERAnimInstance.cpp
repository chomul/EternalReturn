// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/ERAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/ERGameplayTags.h"

namespace
{
	/** 움직여서 끊기 직전에만 본다 — 매 틱 조회하지 않는다 */
	bool HasAnimHold(const AActor* Owner)
	{
		const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
		return ASC && ASC->HasMatchingGameplayTag(ERTags::State_AnimHold);
	}

	/** 상체 슬롯만 쓰는 몽타주 — 다리는 걷기라 움직여도 끊지 않는다 (시셀라 Q · Argument 69) */
	bool IsUpperBodyOnly(const UAnimMontage* M)
	{
		static const FName Upper(TEXT("UpperBody"));
		static const FName Full(TEXT("DefaultSlot"));
		return M && M->IsValidSlot(Upper) && !M->IsValidSlot(Full);
	}
}

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
		bModeHasStart = ModeStartAnim != nullptr;
		bModeHasEnd = ModeEndAnim != nullptr;
	}
	// 전이 규칙용 — 규칙 그래프가 변수 하나만 읽게 (AND · NOT 노드가 있으면 Fast Path 를 못 탄다 · 2026-10-05 컴파일 경고)
	bEnterModeStart = bInMode && bModeHasStart;
	bEnterModeLoop = bInMode && !bModeHasStart;
	bExitModeEnd = !bInMode && bModeHasEnd;
	bExitModeNone = !bInMode && !bModeHasEnd;
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
		// · 모션 유지 태그(State.AnimHold — 매그너스 W 장판 동안 · Argument 63 M1) 중에는 안 끊는다. 복제 loose 태그라 모든 머신에 있다
		else if (bWatchedSeenStill && !(Move && Move->HasRootMotionSources()) && !IsUpperBodyOnly(Current) && !HasAnimHold(Owner))
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
	if (bPendingRest)
	{
		bPendingRest = false;
		BewareStartAnim = PendingRestAnims[0]; BewareLoopAnim = PendingRestAnims[1]; BewareEndAnim = PendingRestAnims[2];
		SleepStartAnim = PendingRestAnims[3];  SleepLoopAnim = PendingRestAnims[4];  WakeAnim = PendingRestAnims[5];
		// 반복 애니가 없으면 그 자세에 들어가지 않는다 (들어가면 빈 포즈)
		bBeware = bPendingBeware && BewareLoopAnim;
		bSleep = bPendingSleep && SleepLoopAnim;
		// 전이 조건 조합 (Fast Path — AnimBP 는 하나씩만 읽는다)
		bBewareIntro = bBeware && BewareStartAnim && !bPendingRestSkipIntro;
		bBewareDirect = bBeware && !bBewareIntro;
		bBewareOutro = !bBeware && BewareEndAnim;
		bBewareQuit = !bBeware && !BewareEndAnim;
		bSleepIntro = bSleep && SleepStartAnim && !bPendingRestSkipIntro;
		bSleepDirect = bSleep && !bSleepIntro;
		bSleepOutro = !bSleep && WakeAnim;
		bSleepQuit = !bSleep && !WakeAnim;
	}
}

void UERAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	// 워커 스레드 — 복사본만 쓴다.
	GroundSpeed = GatheredVelocity.Size2D();
	bIsMoving = GroundSpeed > MoveThreshold;
}
