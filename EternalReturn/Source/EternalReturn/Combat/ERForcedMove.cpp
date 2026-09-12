// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ERForcedMove.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/ERCharacterBase.h"
// ⚠ 캡슐 크기를 읽으려면 완전 타입이 필요하다. Character.h 는 전방 선언만 준다 —
//   빠뜨리면 error C2027 이 나고, 이어서 SweepSingleByChannel 의 인자 오류로 번진다 (E02 와 같은 부류).
#include "Components/CapsuleComponent.h"
#include "EternalReturn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "GAS/ERGameplayTags.h"

namespace ERForcedMove
{

const FName ForceName(TEXT("ERForcedMove"));

bool ApplyForcedMove(ACharacter* Target, const FVector& Direction, float DistanceUU, float Duration)
{
	if (!Target)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[강제이동] 대상이 없다."));
		return false;
	}

	// ⚠ 서버 권위다. 방향·거리·시간을 서버가 정하고, 벽 충돌도 서버가 판정한다
	//   (역기획서 §3.5). 클라가 부르면 각 머신이 다른 결과를 낸다.
	if (!Target->HasAuthority())
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[강제이동] 서버가 아니다. 강제 이동은 서버 권위다. (%s)"), *GetNameSafe(Target));
		return false;
	}

	const FVector Dir = Direction.GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		// ⚠ 시전자와 대상이 정확히 겹치면 방향이 나오지 않는다. 실제로 일어난다.
		UE_LOG(LogEternalReturn, Warning,
			TEXT("[강제이동] 방향이 0 이다. 밀어낼 방향을 정할 수 없다. (%s)"), *GetNameSafe(Target));
		return false;
	}

	if (DistanceUU <= 0.f || Duration <= 0.f)
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[강제이동] 거리(%.1f) 또는 시간(%.2f)이 0 이하다."), DistanceUU, Duration);
		return false;
	}

	// ⭐⭐ 이동 방해 면역 (매그너스 R) — **여기서 직접 검사해야 한다.**
	//
	// ⚠ 다른 CC 는 GE 라서 애셋의 ApplicationTagRequirements 가 막아 준다.
	//   강제 이동은 **GE 가 아니라 RootMotionSource** 라 그 경로를 안 탄다.
	//   여기서 안 막으면 면역인데도 밀려난다.
	//   근거: Docs/4_Argument/14_저항_적용방식.md 파트 3
	if (const UAbilitySystemComponent* ASC =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target))
	{
		if (ASC->HasMatchingGameplayTag(ERTags::State_CCImmune))
		{
			UE_LOG(LogEternalReturn, Verbose,
				TEXT("[강제이동] %s 는 이동 방해 면역이다. 무시한다."), *GetNameSafe(Target));
			return false;
		}
	}

	const FVector StartLocation  = Target->GetActorLocation();
	const FVector TargetLocation = StartLocation + Dir * DistanceUU;

	// ⭐ 서버 자신에게 먼저 붙이고, 폰이 클라들에게 Multicast 로 뿌린다.
	//   ⚠ 각 머신이 **같은 StartLocation/TargetLocation** 을 써야 결과가 같다.
	//     방향·거리를 보내고 각자 계산하게 하면 위치가 미세하게 달라 어긋난다.
	AddForcedMoveSource(Target, StartLocation, TargetLocation, Duration);

	if (AERCharacterBase* ERCharacter = Cast<AERCharacterBase>(Target))
	{
		ERCharacter->Multicast_ForcedMove(StartLocation, TargetLocation, Duration);

		// ⭐ 벽 감시를 켠다. 넉백이 끝나거나 벽에 닿으면 스스로 꺼진다.
		//   역기획서 §3.5 — "이동 중 매 틱 지오메트리 트레이스"
		ERCharacter->StartForcedMoveWatch();
	}
	else
	{
		// ⚠ 야생동물 등 AERCharacterBase 가 아닌 캐릭터는 아직 전파 경로가 없다.
		//   서버에서만 움직이므로 클라에서 튄다. F12 에서 같은 RPC 를 붙인다.
		UE_LOG(LogEternalReturn, Warning,
			TEXT("[강제이동] %s 는 AERCharacterBase 가 아니라 클라에 전파되지 않는다."),
			*GetNameSafe(Target));
	}

	UE_LOG(LogEternalReturn, Verbose, TEXT("[강제이동] %s — %.0fcm, %.2f초"),
		*GetNameSafe(Target), DistanceUU, Duration);

	return true;
}

void AddForcedMoveSource(ACharacter* Target, const FVector& StartLocation,
	const FVector& TargetLocation, float Duration)
{
	UCharacterMovementComponent* CMC = Target ? Target->GetCharacterMovement() : nullptr;
	if (!CMC)
	{
		return;
	}

	// ⚠ 이미 걸려 있으면 먼저 뗀다. 두 개가 겹치면 서로 덮어써서 위치가 튄다.
	RemoveForcedMoveSource(Target);

	TSharedPtr<FRootMotionSource_MoveToForce> MoveToForce = MakeShared<FRootMotionSource_MoveToForce>();
	MoveToForce->InstanceName = ForceName;

	// ⭐ Override 다. 넉백 중에는 본인 이동 입력이 무시돼야 한다.
	//   ⚠ 입력 차단(State.Block.Movement)과 **이중으로** 건다 — 태그는 입력 레이어를,
	//     이 모드는 이동 계산 자체를 막는다.
	MoveToForce->AccumulateMode = ERootMotionAccumulateMode::Override;

	// GAS 태스크가 쓰는 값과 맞춘다 (AbilityTask_ApplyRootMotionMoveToForce.cpp:66-68).
	MoveToForce->Settings.SetFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck);
	MoveToForce->Priority = 1000;

	MoveToForce->StartLocation  = StartLocation;
	MoveToForce->TargetLocation = TargetLocation;
	MoveToForce->Duration       = Duration;

	// ⚠ false 다. true 면 예상 속도로 제한해서 **정확한 거리 도달을 방해**한다.
	MoveToForce->bRestrictSpeedToExpected = false;

	// ⭐ 끝나면 속도를 0 으로. 안 그러면 넉백이 끝난 뒤 미끄러진다.
	//
	// ⚠ "ClearVelocity" 같은 모드는 없다. 모드는 셋뿐이고
	//   (RootMotionSource.h:140-148: MaintainLastRootMotionVelocity / SetVelocity / ClampVelocity)
	//   멈추려면 **SetVelocity 에 0 을 넣는** 것이 엔진이 의도한 방법이다
	//   (:144 주석 — "for example, 0,0,0 to stop the character").
	MoveToForce->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	MoveToForce->FinishVelocityParams.SetVelocity = FVector::ZeroVector;

	CMC->ApplyRootMotionSource(MoveToForce);
}

void RemoveForcedMoveSource(ACharacter* Target)
{
	if (UCharacterMovementComponent* CMC = Target ? Target->GetCharacterMovement() : nullptr)
	{
		CMC->RemoveRootMotionSource(ForceName);
	}
}

bool IsForcedMoving(const ACharacter* Target)
{
	const UCharacterMovementComponent* CMC = Target ? Target->GetCharacterMovement() : nullptr;
	if (!CMC)
	{
		return false;
	}

	// ⚠ const 를 벗긴다. GetRootMotionSource 는 const 가 아니지만 상태를 바꾸지 않는다.
	UCharacterMovementComponent* MutableCMC = const_cast<UCharacterMovementComponent*>(CMC);
	return MutableCMC->GetRootMotionSource(ForceName).IsValid();
}

bool CheckWallImpact(ACharacter* Target, FHitResult& OutHit)
{
	if (!Target || !Target->HasAuthority())
	{
		return false;
	}

	const UCapsuleComponent* Capsule = Target->GetCapsuleComponent();
	if (!Capsule)
	{
		return false;
	}

	const UCharacterMovementComponent* CMC = Target->GetCharacterMovement();
	if (!CMC)
	{
		return false;
	}

	// ⭐ **진행 방향으로 짧게 스윕한다.**
	//   ⚠ 위치 비교("덜 움직였으면 벽") 로 판정하지 않는다 — 경사·계단·다른 캐릭터와
	//     부딪혀도 덜 움직이므로 오탐이 난다. 무엇에 막혔는지 알아야 한다.
	const FVector Velocity = CMC->Velocity;
	const FVector Direction = Velocity.GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		// 이미 멈췄다. 벽에 닿아서인지 끝나서인지는 여기서 알 수 없다.
		return false;
	}

	// ⚠ 한 틱에 이동하는 거리보다 조금 앞을 본다. 너무 길면 아직 안 닿은 벽을 잡고,
	//   너무 짧으면 빠른 넉백에서 벽을 통과한 뒤에야 감지한다.
	constexpr float ProbeDistance = 30.f;

	const FVector Start = Target->GetActorLocation();
	const FVector End   = Start + Direction * ProbeDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ERForcedMoveWall), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(Target);

	// ⚠ 캡슐로 스윕한다. 라인 트레이스는 캐릭터 옆구리가 걸리는 경우를 놓친다.
	const bool bHit = Target->GetWorld()->SweepSingleByChannel(
		OutHit, Start, End, FQuat::Identity, ECC_WorldStatic,
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),
			Capsule->GetScaledCapsuleHalfHeight()),
		Params);

	if (!bHit)
	{
		return false;
	}

	// ⭐ **벽만 센다.** 바닥·경사는 제외한다 —
	//   넉백 중에도 바닥은 늘 아래에 있어서, 그냥 두면 매번 "벽 충돌" 이 된다.
	//
	// ⭐ 엔진이 이미 판정해 준다 (CharacterMovementComponent.h:1868).
	//   ⚠ 우리가 ImpactNormal.Z 임계값을 따로 정하지 않는다 — CMC 의
	//     WalkableFloorZ 설정과 어긋나면 "걸어 올라갈 수 있는 경사인데 벽으로 친다"
	//     같은 불일치가 생긴다 (CLAUDE.md §1).
	if (CMC->IsWalkable(OutHit))
	{
		return false;
	}

	return true;
}

} // namespace ERForcedMove
