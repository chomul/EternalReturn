// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/ERCharacterBase.h"
#include "Core/ERPlayerState.h"
#include "EternalReturn.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Character/ERCharacterData.h"
#include "GAS/ERAttributeInit.h"
#include "GAS/ERAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/ERCharacterMovementComponent.h"
#include "Combat/ERForcedMove.h"

AERCharacterBase::AERCharacterBase(const FObjectInitializer& ObjectInitializer)
	// ⭐ 이동 차단이 들어 있는 CMC 로 교체한다. **생성자에서만 가능하다.**
	//   근거: Docs/4_Argument/10_CC_차단축_태그설계.md (방안 A)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UERCharacterMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	// ⚠ Tick 은 **강제 이동 중에만** 돈다. 평소에는 꺼져 있어 비용이 0 이다.
	//   켜는 곳은 StartForcedMoveWatch 한 곳뿐이고, 벽에 닿거나 넉백이 끝나면 스스로 끈다.
	//   (역기획서 §3.5 가 "이동 중 매 틱 트레이스" 를 요구한다 · CLAUDE.md §2)
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// ── 탑다운 카메라 리그 ──────────────────────────────────
	//
	// ⭐ 값은 **원작 스크린샷에서 역산한 측정값**이다. 추측이 아니다.
	//   측정 방법과 오차: Docs/4_Argument/8_카메라_각도거리_소유주체.md
	//
	//
	// ⚠ **조정할 때는 거리만 만진다.** 거리·FOV·화면 크기가 한 방정식에 묶여 있어
	//   둘을 같이 만지면 수렴하지 않는다. FOV 와 피치를 고정하고 거리로 맞춘다.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 2050.f;

	// ⚠⚠ **Yaw 가 0 이 아니다.** 원작은 월드 축에 대해 비스듬히 본다.
	//   ER_Level 의 같은 위치를 비교한 결과: 우리는 도로를 직각으로 보고 있었고
	//   원작은 대각선으로 본다. 도로의 평행선이 원작에서는 수렴하고 우리는 평행했다.
	//
	// ⚠ 이 값을 바꾸면 가장자리 스크롤의 화면->월드 대응도 같이 움직인다.
	//   그래서 ERPlayerController 가 **카메라 회전에서 계산**하도록 만들어 뒀다.
	//   하드코딩된 축이 없으니 여기만 고치면 된다.
	CameraBoom->SetRelativeRotation(FRotator(-55.f, CameraYaw, 0.f));

	// 캐릭터가 돌아도 시점이 같이 돌면 안 된다. 탑다운은 시점이 고정이다.
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw   = false;
	CameraBoom->bInheritRoll  = false;

	// ⭐ 벽에 닿았다고 카메라를 당기지 않는다.
	//   탑다운에서 시야가 갑자기 좁아지면 조작이 끊긴다.
	CameraBoom->bDoCollisionTest = false;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;

	// ⭐ UE 기본 90° 가 아니다. 원작은 훨씬 좁다.
	//   근거: 스크린샷에서 수직선(전화부스·기둥·벽 모서리)이 거의 수렴하지 않는다.
	//   90° 였다면 화면 가장자리에서 눈에 띄게 기울어야 한다.
	//   ⚠ 이 값을 바꾸면 위의 거리도 같이 틀어진다 (한 방정식에 묶여 있다).
	TopDownCamera->SetFieldOfView(55.f);

	// 컨트롤러 회전을 폰에 그대로 먹이지 않는다. 탑다운이라 컨트롤러는 카메라를 보고
	// 캐릭터는 **가는 방향**을 본다 — 아래 bOrientRotationToMovement 가 그 일을 한다.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw   = false;
	bUseControllerRotationRoll  = false;

	// ── 이동 방향으로 회전 ──────────────────────────────────
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		// ⭐ 캐릭터가 **가는 방향**을 바라본다.
		//   ⚠ bUseControllerRotationYaw 와 **같이 켜면 안 된다.** 둘이 싸운다.
		//     위에서 false 로 둔 것이 짝이다.
		Movement->bOrientRotationToMovement = true;
		Movement->bUseControllerDesiredRotation = false;

		// ⭐ 무엇을 보고 도는가: **Acceleration** 이다
		//   (CharacterMovementComponent.cpp:6264 — ComputeOrientToMovementRotation).
		//   ⚠ 이게 E08 과 맞물린다. 클릭 이동이 RequestedVelocity 로 움직이던 때는
		//     Acceleration 이 0 이라 회전이 어정쩡했다. bUseAccelerationForPaths 로
		//     Acceleration 경로를 타게 되면서 그대로 동작한다.
		//
		// ⭐ 넉백 중에는 자동으로 회전이 멈춘다 — RootMotion Override 라 Acceleration 이
		//   0 이고, 그러면 현재 회전을 유지한다 (:6273). 밀려나며 빙글 도는 일이 없다.
		//
		// ⚠ CC 중에는 UERCharacterMovementComponent::GetDeltaRotation 이 0 을 돌려
		//   회전 자체가 막힌다 (F06-01).

		// ⚠ **자체 결정값이다.** 엔진 기본은 360도/초(한 바퀴 1초)로 탑다운에는 느리다.
		//   1080 이면 반 바퀴가 약 0.17초 — 클릭 방향 전환이 거의 즉시 따라온다.
		//   (720 으로 먼저 넣었다가 **사용자 요청으로 1.5배** 올렸다. 2026-09-10)
		//   원작의 정확한 회전 속도는 **(미확인)**. 감이 안 맞으면 이 값만 조정한다.
		Movement->RotationRate = FRotator(0.f, 1080.f, 0.f);
	}

	// 스킬 판정(SkillTarget 채널) 응답은 여기서 건드리지 않는다.
	//
	// SetCollisionResponseToChannel 을 부르면 FBodyInstance 가
	// InvalidateCollisionProfileName() 을 호출해 프로파일이 Pawn -> Custom 으로 바뀌고
	// (BodyInstance.cpp:544-548), BP 에 저장된 Pawn 프로파일에 다시 덮인다.
	//
	// 대신 Config/DefaultEngine.ini 의 EditProfiles 로 Pawn 프로파일 자체를 고쳤다.
	// 상세: Docs/ErrorReport/E01_스킬판정_채널응답_무시됨.md
}

UAbilitySystemComponent* AERCharacterBase::GetAbilitySystemComponent() const
{
	const AERPlayerState* ERPlayerState = GetPlayerState<AERPlayerState>();
	return ERPlayerState ? ERPlayerState->GetAbilitySystemComponent() : nullptr;
}

void AERCharacterBase::Multicast_ForcedMove_Implementation(
	const FVector& StartLocation, const FVector& TargetLocation, float Duration)
{
	// ⚠ 서버는 ApplyForcedMove 에서 이미 붙였다. 여기서 또 붙이면 두 번 들어간다.
	//   (Multicast 는 서버에서도 실행된다)
	if (HasAuthority())
	{
		return;
	}

	ERForcedMove::AddForcedMoveSource(this, StartLocation, TargetLocation, Duration);
}

void AERCharacterBase::Multicast_StopForcedMove_Implementation()
{
	// ⚠ 여기는 서버도 포함해서 전부 뗀다 — 중단은 서버가 판정만 하고 제거는 각자 한다.
	ERForcedMove::RemoveForcedMoveSource(this);
}

void AERCharacterBase::StartForcedMoveWatch()
{
	// ⚠ 벽 판정은 서버 권위다 (역기획서 §3.5). 클라는 Tick 을 돌 이유가 없다 —
	//   클라는 Multicast 로 받은 소스를 CMC 가 재생하기만 한다.
	if (HasAuthority())
	{
		SetActorTickEnabled(true);
	}
}

void AERCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// ⚠ 이 Tick 은 강제 이동 감시 전용이다. 다른 용도를 여기에 붙이지 않는다 —
	//   붙이면 "평소엔 꺼져 있다" 는 전제가 깨진다.
	if (!HasAuthority())
	{
		SetActorTickEnabled(false);
		return;
	}

	// 넉백이 끝났으면(만료 또는 제거) 감시를 멈춘다.
	if (!ERForcedMove::IsForcedMoving(this))
	{
		SetActorTickEnabled(false);
		return;
	}

	FHitResult Hit;
	if (!ERForcedMove::CheckWallImpact(this, Hit))
	{
		return;
	}

	// ── 벽에 부딪혔다 ─────────────────────────────────────
	//
	// ⭐ 서버가 판정하고, 각 머신이 소스를 뗀다. 클라가 따로 판정하지 않는다.
	ERForcedMove::RemoveForcedMoveSource(this);
	Multicast_StopForcedMove();
	SetActorTickEnabled(false);

	UE_LOG(LogEternalReturn, Verbose, TEXT("[강제이동] %s 가 벽에 부딪혔다 — %s"),
		*GetNameSafe(this), *GetNameSafe(Hit.GetActor()));

	// ⏸ 추가 피해·기절은 **구독하는 쪽**이 정한다 (스킬마다 다르다). F07 에서 붙는다.
	OnForcedMoveWallImpact.Broadcast(this, Hit);
}

void AERCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버 경로
	InitAbilityActorInfo();
}

void AERCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 클라 경로 — 이걸 빠뜨리면 클라에서 어빌리티가 조용히 안 나간다
	InitAbilityActorInfo();
}

void AERCharacterBase::InitAbilityActorInfo()
{
	AERPlayerState* ERPlayerState = GetPlayerState<AERPlayerState>();
	if (!ERPlayerState)
	{
		// PossessedBy 시점에 PlayerState 가 아직 없을 수 있다.
		// 그 경우 OnRep_PlayerState 또는 다음 Possess 에서 다시 불린다.
		return;
	}

	UAbilitySystemComponent* ASC = ERPlayerState->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// Owner = ASC 를 소유한 액터(PlayerState), Avatar = 월드에 서 있는 액터(이 폰)
	ASC->InitAbilityActorInfo(ERPlayerState, this);

	UE_LOG(LogEternalReturn, Log,
		TEXT("[GAS] InitAbilityActorInfo — %s / Owner=%s / Avatar=%s"),
		HasAuthority() ? TEXT("Server") : TEXT("Client"),
		*GetNameSafe(ERPlayerState), *GetNameSafe(this));

	// 어트리뷰트에 값을 넣는 건 여기부터다. ASC 가 준비된 뒤여야 GE 가 먹는다.
	if (HasAuthority())
	{
		InitDefaultStats();
	}

	// ⭐ 서버·클라 양쪽에서 건다. 각자 자기 CMC 를 갱신한다.
	BindMoveSpeed();
}

void AERCharacterBase::SetCameraTargetOffset(const FVector& Offset)
{
	if (CameraBoom)
	{
		// TargetOffset 은 **월드 공간**에서 원점을 밀어낸다.
		// SocketOffset(카메라 로컬)과 다르다 - 잠금 해제 스크롤에는 이쪽이 맞다.
		CameraBoom->TargetOffset = Offset;
	}
}

void AERCharacterBase::BindMoveSpeed()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || MoveSpeedHandle.IsValid())
	{
		return;
	}

	MoveSpeedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		UERAttributeSet::GetMoveSpeedAttribute())
		.AddUObject(this, &AERCharacterBase::OnMoveSpeedChanged);

	// 구독 시점에 이미 값이 들어와 있을 수 있다. 한 번 반영하고 시작한다.
	ApplyMoveSpeed(ASC->GetNumericAttribute(UERAttributeSet::GetMoveSpeedAttribute()));
}

void AERCharacterBase::OnMoveSpeedChanged(const FOnAttributeChangeData& Data)
{
	ApplyMoveSpeed(Data.NewValue);
}

void AERCharacterBase::ApplyMoveSpeed(float MetersPerSecond)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	// ⚠ **단위가 다르다.** 어트리뷰트는 m/s, CMC 는 cm/s 다.
	//   이걸 빠뜨리면 이동 속도가 100배 느려지고, 증상은 "안 움직인다" 로 보인다.
	//   근거: Docs/1_Task/F05_입력_카메라_이동/02_클릭이동_서버권위.md
	constexpr float MetersToUU = 100.f;
	Movement->MaxWalkSpeed = MetersPerSecond * MetersToUU;
}

void AERCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ⭐ 구독 해제. 안 풀면 dangling 델리게이트가 남는다 (F02-06 주의점).
	if (MoveSpeedHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				UERAttributeSet::GetMoveSpeedAttribute()).Remove(MoveSpeedHandle);
		}
		MoveSpeedHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void AERCharacterBase::InitDefaultStats()
{
	// Override 로 다시 박히면 전투 중에 체력이 만피로 돌아간다.
	if (bDefaultStatsApplied)
	{
		return;
	}

	// GAS 는 없는 걸 조용히 건너뛴다. 읽는 쪽이 직접 검사하고 로그를 남긴다.
	if (!CharacterData)
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[초기스탯] %s 의 Character Data 가 비어 있다. 실험체 데이터 애셋을 지정해야 한다."),
			*GetNameSafe(this));
		return;
	}

	if (ERAttributeInit::ApplyStatRow(GetAbilitySystemComponent(), InitStatsEffect, CharacterData->BaseStats))
	{
		bDefaultStatsApplied = true;
	}
}
