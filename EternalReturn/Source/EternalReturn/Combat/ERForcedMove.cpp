// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ERForcedMove.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/ERForcedMoveComponent.h"
// ⚠ 캡슐 크기를 읽으려면 완전 타입이 필요하다. Character.h 는 전방 선언만 준다 —
//   빠뜨리면 error C2027 이 나고, 이어서 SweepSingleByChannel 의 인자 오류로 번진다 (E02 와 같은 부류).
#include "Components/CapsuleComponent.h"
#include "EternalReturn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "GAS/ERGameplayTags.h"
#include "TimerManager.h"

namespace ERForcedMove
{

const FName ForceName(TEXT("ERForcedMove"));

namespace
{
	/**
	 * 넉백과 자기 이동의 공통 몸통 — 검증, 소스 추가, Multicast, 벽 감시.
	 * 면역 검사만 밖에 있다: ApplyForcedMove 는 하고, ApplySelfMove 는 안 한다 (F07-06).
	 */
	bool StartMove(ACharacter* Target, const FVector& Direction, float DistanceUU, float Duration, float HeightUU, bool bPassThroughPawns, const TCHAR* Label);
}

bool ApplyForcedMove(ACharacter* Target, const FVector& Direction, float DistanceUU, float Duration, float HeightUU)
{
	if (!Target)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[강제이동] 대상이 없다."));
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

	return StartMove(Target, Direction, DistanceUU, Duration, HeightUU, false, TEXT("강제이동"));
}

bool ApplySelfMove(ACharacter* Target, const FVector& Direction, float DistanceUU, float Duration, bool bPassThroughPawns)
{
	// 면역 검사 없음 — 자기가 시작하는 이동은 "방해" 가 아니다.
	return StartMove(Target, Direction, DistanceUU, Duration, 0.f, bPassThroughPawns, TEXT("자기이동"));
}

namespace
{

bool StartMove(ACharacter* Target, const FVector& Direction, float DistanceUU, float Duration, float HeightUU, bool bPassThroughPawns, const TCHAR* Label)
{
	if (!Target)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[%s] 대상이 없다."), Label);
		return false;
	}

	// ⚠ 서버 권위다. 방향·거리·시간을 서버가 정하고, 벽 충돌도 서버가 판정한다
	//   (역기획서 §3.5). 클라가 부르면 각 머신이 다른 결과를 낸다.
	if (!Target->HasAuthority())
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[%s] 서버가 아니다. 이동은 서버 권위다. (%s)"), Label, *GetNameSafe(Target));
		return false;
	}

	// 제자리 띄우기(거리 0 · 높이만 — 에어본)는 방향이 필요 없다 → 몸 앞으로 둔다
	FVector Dir = Direction.GetSafeNormal2D();
	if (Dir.IsNearlyZero() && DistanceUU <= 0.f && HeightUU > 0.f)
	{
		Dir = Target->GetActorForwardVector().GetSafeNormal2D();
	}
	if (Dir.IsNearlyZero())
	{
		// ⚠ 시전자와 대상이 정확히 겹치면 방향이 나오지 않는다. 실제로 일어난다.
		UE_LOG(LogEternalReturn, Warning,
			TEXT("[%s] 방향이 0 이다. 이동 방향을 정할 수 없다. (%s)"), Label, *GetNameSafe(Target));
		return false;
	}

	if ((DistanceUU <= 0.f && HeightUU <= 0.f) || Duration <= 0.f)
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[%s] 거리(%.1f) 또는 시간(%.2f)이 0 이하다."), Label, DistanceUU, Duration);
		return false;
	}

	const FVector StartLocation  = Target->GetActorLocation();
	const FVector TargetLocation = StartLocation + Dir * DistanceUU;

	// ⭐ 서버 자신에게 먼저 붙이고, 폰이 클라들에게 Multicast 로 뿌린다.
	//   ⚠ 각 머신이 **같은 StartLocation/TargetLocation** 을 써야 결과가 같다.
	//     방향·거리를 보내고 각자 계산하게 하면 위치가 미세하게 달라 어긋난다.
	AddForcedMoveSource(Target, StartLocation, TargetLocation, Duration, HeightUU, bPassThroughPawns);

	if (UERForcedMoveComponent* Receiver = Target->FindComponentByClass<UERForcedMoveComponent>())
	{
		Receiver->Multicast_ForcedMove(StartLocation, TargetLocation, Duration, HeightUU, bPassThroughPawns);

		// ⭐ 벽 감시를 켠다. 넉백이 끝나거나 벽에 닿으면 스스로 꺼진다.
		//   역기획서 §3.5 — "이동 중 매 틱 지오메트리 트레이스"
		Receiver->StartWatch();
	}
	else
	{
		// ⚠ 컴포넌트가 없으면 클라에 전파되지 않고 벽 감시도 없다 — 실험체 · 야생동물은 생성자에서 붙인다 (Argument 44).
		UE_LOG(LogEternalReturn, Warning,
			TEXT("[%s] %s 에 UERForcedMoveComponent 가 없다 — 클라에 전파되지 않는다."),
			Label, *GetNameSafe(Target));
	}

	UE_LOG(LogEternalReturn, Log, TEXT("[%s] %s — %.0fcm%s, %.2f초, %s -> %s"),
		Label, *GetNameSafe(Target), DistanceUU, HeightUU > 0.f ? *FString::Printf(TEXT(" · 높이 %.0fcm"), HeightUU) : TEXT(""), Duration,
		*StartLocation.ToCompactString(), *TargetLocation.ToCompactString());

	return true;
}

} // namespace

void AddForcedMoveSource(ACharacter* Target, const FVector& StartLocation,
	const FVector& TargetLocation, float Duration, float HeightUU, bool bPassThroughPawns)
{
	UCharacterMovementComponent* CMC = Target ? Target->GetCharacterMovement() : nullptr;
	if (!CMC)
	{
		return;
	}

	// ⚠ 이미 걸려 있으면 먼저 뗀다. 두 개가 겹치면 서로 덮어써서 위치가 튄다.
	RemoveForcedMoveSource(Target);

	// 밀어내며 돌진 (멧돼지 · F12.6-04) — 캐릭터에 막혀 멈추지 않게 캡슐의 Pawn 채널만 이동 시간 동안 Overlap. 벽(WorldStatic)은 그대로 막힌다.
	//   부딪힌 캐릭터를 미는 건 스킬의 넉백 조각.
	//   모든 머신이 같은 시간에 바꿨다 되돌린다 (이 함수는 서버 · 멀티캐스트 양쪽에서 불린다). 원래 값으로 되돌린다.
	if (bPassThroughPawns)
	{
		if (UCapsuleComponent* Capsule = Target->GetCapsuleComponent())
		{
			const ECollisionResponse Original = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
			TWeakObjectPtr<UCapsuleComponent> WeakCapsule(Capsule);
			FTimerHandle Unused;
			Target->GetWorldTimerManager().SetTimer(Unused, FTimerDelegate::CreateLambda([WeakCapsule, Original]()
			{
				if (UCapsuleComponent* C = WeakCapsule.Get())
				{
					C->SetCollisionResponseToChannel(ECC_Pawn, Original);
				}
			}), Duration, false);
		}
	}

	// [진단] F12.6-04 "넉백이 가끔 안 먹는다" (2026-09-30) — 서버 로그엔 매번 걸렸다. 머신마다 끝났을 때 실제로 얼마나 갔나를 남긴다.
	{
		TWeakObjectPtr<ACharacter> WeakTarget(Target);
		const FVector Start2D(StartLocation.X, StartLocation.Y, 0.f);
		const FVector Goal2D(TargetLocation.X, TargetLocation.Y, 0.f);
		FTimerHandle Unused;
		Target->GetWorldTimerManager().SetTimer(Unused, FTimerDelegate::CreateLambda([WeakTarget, Start2D, Goal2D]()
		{
			if (const ACharacter* T = WeakTarget.Get())
			{
				const FVector Now2D(T->GetActorLocation().X, T->GetActorLocation().Y, 0.f);
				UE_LOG(LogEternalReturn, Log, TEXT("[강제이동] %s 끝 — 간 거리 %.0fcm / 목표 %.0fcm · 오차 %.0fcm (%s · %s)"),
					*T->GetName(), FVector::Dist(Start2D, Now2D), FVector::Dist(Start2D, Goal2D), FVector::Dist(Now2D, Goal2D),
					T->HasAuthority() ? TEXT("서버") : TEXT("클라"), *UEnum::GetValueAsString(T->GetLocalRole()));
			}
		}), Duration + 0.05f, false);
	}

	// ⭐ 높이가 있으면 **포물선** (에어본 · 띄우며 밀기 — F12.6-04 멧돼지 돌진). 엔진 JumpForce — 거리 · 높이 · 시간.
	//   설정은 GAS 태스크와 같게 (AbilityTask_ApplyRootMotionJumpForce.cpp:103-116). 뜨는 순간 Walking → Falling 은
	//   UseSensitiveLiftoffCheck 가 맡는다 (MoveToForce 와 같은 플래그). 애니 애셋이 없어 몸만 뜬다 [자체].
	if (HeightUU > 0.f)
	{
		const FVector Delta = FVector(TargetLocation - StartLocation) * FVector(1.f, 1.f, 0.f);
		TSharedPtr<FRootMotionSource_JumpForce> Jump = MakeShared<FRootMotionSource_JumpForce>();
		Jump->InstanceName = ForceName;
		Jump->AccumulateMode = ERootMotionAccumulateMode::Override;
		Jump->Settings.SetFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck);
		Jump->Priority = 1000;
		Jump->Rotation = Delta.IsNearlyZero() ? Target->GetActorRotation() : Delta.Rotation();
		Jump->Distance = Delta.Size();
		Jump->Height = HeightUU;
		Jump->Duration = Duration;
		Jump->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
		Jump->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
		CMC->ApplyRootMotionSource(Jump);
		return;
	}

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
	// ⭐⭐ **오브젝트 종류로** 찾는다 (벽 · 지형 = WorldStatic · WorldDynamic). 채널(ECC_WorldStatic)로 찾으면
	//   캐릭터 캡슐(Pawn 프로필)도 그 채널을 막아서 **앞에 있는 캐릭터를 벽으로 잡는다** — 멧돼지 돌진이 맞은 사람과 서로를
	//   벽으로 보고 둘 다 30~80cm 에서 멈췄다 (E34 · 2026-09-30 진단 로그 "간 거리 42cm / 목표 600cm").
	FCollisionObjectQueryParams WallTypes;
	WallTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	WallTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	const bool bHit = Target->GetWorld()->SweepSingleByObjectType(
		OutHit, Start, End, FQuat::Identity, WallTypes,
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
