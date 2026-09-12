// Copyright Epic Games, Inc. All Rights Reserved.

#include "ERCharacterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GAS/ERGameplayTags.h"

UERCharacterMovementComponent::UERCharacterMovementComponent()
{
	// ⭐⭐ **클릭 이동이 멀티에서 동작하게 하는 한 줄이다.** (E08)
	//
	//   기본값 false 이면 경로 추적이 RequestDirectMove 를 쓴다. 그러면
	//   RequestedVelocity 만 세워지고(CharacterMovementComponent.cpp:3804)
	//   **Acceleration 은 0 으로 남는다.**
	//   그런데 FSavedMove_Character 가 저장하는 이동 입력은 Acceleration 뿐이라
	//   (CharacterMovementComponent.cpp:12157) 클라의 이동이 ServerMove 에 실리지 않는다.
	//   → 서버는 가만히 있고, 클라를 계속 원위치로 되돌린다.
	//   ⚠ Standalone 에서는 서버=클라라 이 검증이 없어 **문제가 드러나지 않는다.**
	//
	//   true 로 두면 경로 추적이 Acceleration 경로를 탄다:
	//     PathFollowingComponent.cpp:1109  RequestPathMove(CurrentMoveInput)
	//     PawnMovementComponent.cpp:91     Internal_AddMovementInput(MoveInput)
	//     CharacterMovementComponent.cpp:1518  ConsumeInputVector() -> Acceleration
	//   → SavedMove 에 담기고 ServerMove 로 가서 서버가 **같은 입력을 재생**한다.
	//
	// ⭐ 이 방식이 E07 의 "서버는 검증만 한다" 와도 맞물린다 —
	//   서버가 따로 경로를 돌릴 필요가 없다. 클라 입력을 재생하면 되기 때문이다.
	bUseAccelerationForPaths = true;
}

bool UERCharacterMovementComponent::IsMovementBlocked() const
{
	// ⭐ 인터페이스로 찾는다. ASC 가 PlayerState(플레이어)에 있는지
	//   Pawn(야생동물)에 있는지 여기서 몰라도 된다.
	//   AERCharacterBase::GetAbilitySystemComponent() 가 PlayerState 것을 돌려준다.
	//   (AbilitySystemGlobals.cpp:216-220 이 IAbilitySystemInterface 를 먼저 본다)
	const UAbilitySystemComponent* ASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());

	// ⚠ ASC 가 없는 순간이 정상적으로 존재한다 — PlayerState 복제 전, 사망 직후.
	//   그때는 막지 않는다. 없는 것을 못 움직이는 것으로 취급하면 스폰 직후 얼어붙는다.
	if (!ASC)
	{
		return false;
	}

	return ASC->HasMatchingGameplayTag(ERTags::State_Block_Movement);
}

float UERCharacterMovementComponent::GetMaxSpeed() const
{
	if (IsMovementBlocked())
	{
		return 0.f;
	}

	return Super::GetMaxSpeed();
}

FRotator UERCharacterMovementComponent::GetDeltaRotation(float DeltaTime) const
{
	if (IsMovementBlocked())
	{
		return FRotator::ZeroRotator;
	}

	return Super::GetDeltaRotation(DeltaTime);
}
