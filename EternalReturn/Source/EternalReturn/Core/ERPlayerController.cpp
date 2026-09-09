// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ERPlayerController.h"
#include "Character/ERCharacterBase.h"
#include "Character/ERInputConfig.h"
#include "EternalReturn.h"

#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "NavigationSystem.h"

AERPlayerController::AERPlayerController()
{
	// 클릭 이동이라 커서가 보여야 한다.
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AERPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// ⚠ 매핑 컨텍스트는 **로컬 컨트롤러에만** 붙인다. 서버에서 붙이면 아무 의미가 없고,
	//   데디케이티드 서버에서는 서브시스템 자체가 없다.
	if (!IsLocalController())
	{
		return;
	}

	if (!InputConfig || !InputConfig->DefaultContext)
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[입력] %s 의 Input Config 또는 Default Context 가 비어 있다. 이동이 동작하지 않는다."),
			*GetNameSafe(this));
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(InputConfig->DefaultContext, InputConfig->DefaultContextPriority);

		UE_LOG(LogEternalReturn, Log, TEXT("[입력] Default 컨텍스트 적용 (우선순위 %d)"),
			InputConfig->DefaultContextPriority);
	}
}

void AERPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		// 프로젝트 설정의 Default Input Component Class 가 EnhancedInputComponent 여야 한다.
		UE_LOG(LogEternalReturn, Error,
			TEXT("[입력] InputComponent 가 UEnhancedInputComponent 가 아니다. ")
			TEXT("Project Settings > Input > Default Input Component Class 를 확인한다."));
		return;
	}

	if (!InputConfig || !InputConfig->MoveToCursor)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[입력] MoveToCursor 액션이 비어 있다."));
		return;
	}

	EnhancedInput->BindAction(InputConfig->MoveToCursor, ETriggerEvent::Started,
		this, &AERPlayerController::OnMoveToCursor);

	if (InputConfig->ToggleCameraLock)
	{
		EnhancedInput->BindAction(InputConfig->ToggleCameraLock, ETriggerEvent::Started,
			this, &AERPlayerController::OnToggleCameraLock);
	}
}

// ═══════════════════════════════════════════════════════════════
//  카메라 (F05-03)
// ═══════════════════════════════════════════════════════════════

void AERPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// ⭐ Tick 을 쓰는 이유: 가장자리 스크롤은 마우스 **위치**에 반응하므로
	//   키 입력 이벤트로는 표현되지 않는다. 매 프레임 평가가 실제로 필요한 경우다
	//   (CLAUDE.md §2 - "정말 매 프레임 필요할 때만").
	//
	// ⚠ 잠금 상태에서는 아무것도 하지 않는다. 기본값이 잠금이라 평소엔 비용이 없다.
	if (!bCameraLocked && bEdgeScrollEnabled)
	{
		UpdateEdgeScroll(DeltaTime);
	}

	ApplyCameraOffset();
}

void AERPlayerController::OnToggleCameraLock()
{
	bCameraLocked = !bCameraLocked;

	// 잠그면 캐릭터로 돌아온다. 밀려나 있던 오프셋을 버린다.
	if (bCameraLocked)
	{
		FreeCameraOffset = FVector::ZeroVector;
	}

	UE_LOG(LogEternalReturn, Log, TEXT("[카메라] %s"),
		bCameraLocked ? TEXT("잠금 - 캐릭터를 따라간다") : TEXT("해제 - 가장자리 스크롤"));
}

void AERPlayerController::UpdateEdgeScroll(float DeltaSeconds)
{
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		// 커서가 창 밖이다. 밀지 않는다.
		return;
	}

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0)
	{
		return;
	}

	// ⭐ 화면 방향을 먼저 구한다 (오른쪽 +X, 위쪽 +Y 인 2D).
	float ScreenRight = 0.f;
	float ScreenUp    = 0.f;

	if (MouseX <= EdgeScrollMargin)                   { ScreenRight = -1.f; }
	else if (MouseX >= ViewportX - EdgeScrollMargin)  { ScreenRight =  1.f; }

	if (MouseY <= EdgeScrollMargin)                   { ScreenUp =  1.f; }
	else if (MouseY >= ViewportY - EdgeScrollMargin)  { ScreenUp = -1.f; }

	if (FMath::IsNearlyZero(ScreenRight) && FMath::IsNearlyZero(ScreenUp))
	{
		return;
	}

	// ⭐⭐ 화면 축을 **카메라 회전에서 계산**한다. 월드 축으로 하드코딩하지 않는다.
	//   카메라 Yaw 가 0 이 아니라서(원작이 비스듬히 본다) +X/+Y 로 박으면
	//   가장자리 스크롤이 엉뚱한 방향으로 밀린다.
	//   ⭐ 이렇게 해 두면 카메라 Yaw 를 바꿔도 여기는 안 고쳐도 된다.
	const AERCharacterBase* ERCharacter = Cast<AERCharacterBase>(GetPawn());
	const float Yaw = ERCharacter ? ERCharacter->GetCameraYaw() : 0.f;
	const FRotator YawOnly(0.f, Yaw, 0.f);

	// 카메라가 내려다보므로 Forward 를 그대로 쓰면 아래로 파고든다. 바닥 평면에 눕힌다.
	const FVector GroundForward = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X);
	const FVector GroundRight   = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);

	const FVector Delta = (GroundForward * ScreenUp) + (GroundRight * ScreenRight);

	FreeCameraOffset += Delta.GetSafeNormal() * EdgeScrollSpeed * DeltaSeconds;

	// ⚠ 상한이 없으면 무한히 밀려나 캐릭터를 화면에서 잃어버린다.
	if (FreeCameraOffset.SizeSquared() > FMath::Square(MaxCameraOffset))
	{
		FreeCameraOffset = FreeCameraOffset.GetSafeNormal() * MaxCameraOffset;
	}
}

void AERPlayerController::ApplyCameraOffset()
{
	if (AERCharacterBase* ERCharacter = Cast<AERCharacterBase>(GetPawn()))
	{
		ERCharacter->SetCameraTargetOffset(FreeCameraOffset);
	}
	// ⚠ 폰이 없는 동안(사망 ~ 부활)에는 반영할 곳이 없다.
	//   관전 카메라는 F14 에서 별도로 다룬다 (Argument 8 파트 2).
}

bool AERPlayerController::ResolveNavigableDestination(const FVector& ClickPoint, FVector& OutDestination) const
{
	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[이동] 네비게이션 시스템이 없다. 레벨에 NavMeshBoundsVolume 이 있는지 확인한다."));
		return false;
	}

	// ── ① 클릭 지점 근처에서 갈 수 있는 곳을 찾는다 ────────
	//
	// ⭐ 상자 위를 클릭해도 **옆 바닥**으로 스냅된다. 이게 MOBA 의 표준 동작이다 -
	//   못 가는 곳을 클릭하면 가장 가까운 갈 수 있는 곳으로 간다.
	FNavLocation Projected;
	if (NavSys->ProjectPointToNavigation(ClickPoint, Projected, NavProjectionExtent))
	{
		OutDestination = Projected.Location;
		return true;
	}

	// ── ② 실패하면 아래로 훑어 지면을 찾는다 ───────────────
	//
	// ⚠ 높은 벽·건물을 클릭하면 ①의 Z 반경(기본 500)으로는 바닥에 닿지 않는다.
	//   그 경우 클릭 지점에서 수직으로 내려 지면을 찾고, 거기서 다시 투영한다.
	//   벽 밑은 못 가는 자리라 투영이 **벽 바깥쪽**으로 밀어 준다.
	FHitResult GroundHit;
	const FVector TraceEnd = ClickPoint - FVector(0.f, 0.f, GroundTraceDownDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ERResolveClick), /*bTraceComplex=*/false);
	if (const APawn* ControlledPawn = GetPawn())
	{
		Params.AddIgnoredActor(ControlledPawn);
	}

	if (GetWorld()->LineTraceSingleByChannel(GroundHit, ClickPoint, TraceEnd, ECC_Visibility, Params))
	{
		if (NavSys->ProjectPointToNavigation(GroundHit.ImpactPoint, Projected, NavProjectionExtent))
		{
			OutDestination = Projected.Location;
			return true;
		}
	}

	// ── ③ 갈 수 있는 곳이 없다 ─────────────────────────────
	// 맵 밖이나 완전히 격리된 곳이다. 크래시가 아니라 무시가 맞다.
	return false;
}

void AERPlayerController::OnMoveToCursor()
{
	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, /*bTraceComplex=*/false, Hit))
	{
		// 하늘을 클릭한 경우 등. 조용히 무시한다 - 에러가 아니다.
		return;
	}

	// ⭐ **클라도 투영한다.** 예전에는 원본 클릭 지점으로 바로 움직여서,
	//   상자를 클릭하면 클라와 서버가 서로 다른 곳으로 갔다.
	FVector Destination;
	if (!ResolveNavigableDestination(Hit.ImpactPoint, Destination))
	{
		return;
	}

	// ⭐ 클라가 **먼저 로컬에서** 움직인다. 서버 응답을 기다리면 클릭할 때마다 지연이 보인다.
	//   서버가 목적지를 거부하면 CMC 의 보정이 위치를 되돌린다.
	StartMoveTo(Destination);

	// ⚠ **원본 클릭 지점**을 보낸다. 서버는 클라의 계산을 믿지 않고 **직접 다시** 푼다.
	//   같은 함수를 쓰므로 결과는 같다.
	ServerSetDestination(Hit.ImpactPoint);
}

bool AERPlayerController::ServerSetDestination_Validate(const FVector& Destination)
{
	// ⭐ 여기서 거부하면 **연결이 끊긴다.** 명백히 조작된 값만 잡는다.
	//   정상 범위를 벗어난 정도는 _Implementation 에서 조용히 무시한다.
	return !Destination.ContainsNaN();
}

void AERPlayerController::ServerSetDestination_Implementation(const FVector& Destination)
{
	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	// ⭐ 검증 ① 거리. 화면 밖을 찍는 텔레포트 시도를 막는다.
	const float DistSq = FVector::DistSquared(ControlledPawn->GetActorLocation(), Destination);
	if (DistSq > FMath::Square(MaxClickDistance))
	{
		UE_LOG(LogEternalReturn, Warning,
			TEXT("[이동] 목적지가 너무 멀다 (%.0fcm > %.0fcm). 무시한다. %s"),
			FMath::Sqrt(DistSq), MaxClickDistance, *GetNameSafe(ControlledPawn));
		return;
	}

	// ⭐ 검증 ② 갈 수 있는 곳인가. **클라와 같은 함수**로 다시 푼다 -
	//   클라가 보낸 좌표를 믿지 않는다.
	FVector Resolved;
	if (!ResolveNavigableDestination(Destination, Resolved))
	{
		// 갈 수 없는 곳이다. 크래시가 아니라 무시가 맞다.
		return;
	}

	// ⚠ **서버는 검증만 하고 직접 이동을 구동하지 않는다.**
	//
	//   역기획서 §3.3 은 "경로는 양쪽 계산" 이라고 적었지만, 플레이어가 조종하는 폰에서
	//   서버가 독립적으로 PathFollowing 을 돌리면 **클라가 보낸 이동과 충돌한다.**
	//   언리얼의 네트워크 캐릭터 이동은 클라가 입력을 만들고 서버가 그것을 재생·검증하는
	//   모델이라(CMC), 서버가 따로 구동하면 서로를 덮어쓴다.
	//
	//   → 서버의 권위는 **목적지 검증**과 CMC 의 ServerMove 검증에 있다.
	//     이게 텔레포트를 막는 실제 방어선이다.
	//
	//   ⚠ 이 판단은 문서와 다르므로 Docs/5_ErrorReport 에 근거를 남긴다.
	UE_LOG(LogEternalReturn, Verbose, TEXT("[이동] 목적지 승인 %s"), *Resolved.ToString());
}

void AERPlayerController::StartMoveTo(const FVector& Destination)
{
	// ⭐ 엔진이 주는 것을 쓴다. 자체 경로 추적을 만들지 않는다 (CLAUDE.md §1).
	//   SimpleMoveToLocation 은 AIController 를 요구하지 않는다 (역기획서 §2.1).
	UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, Destination);
}
