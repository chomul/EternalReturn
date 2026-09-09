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

AERCharacterBase::AERCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	// ── 탑다운 카메라 리그 ──────────────────────────────────
	//
	// ⭐ 값은 **원작 스크린샷에서 역산한 측정값**이다. 추측이 아니다.
	//   측정 방법과 오차: Docs/4_Argument/8_카메라_각도거리_소유주체.md
	//
	//   피치 -44°  : 바닥 원의 타원 비율(단축/장축 = sin 피치)로 구했다. ±4°
	//                ⭐ 이 방법은 FOV 를 몰라도 성립한다
	//   거리 1600  : 캐릭터 키를 기준자로 역산. ⚠ ±25% 로 가장 불확실하다
	//   FOV  63°   : 도로 소실점과 피치를 교차해서 구했다 (아래 TopDownCamera)
	//
	// ⚠ **조정할 때는 거리만 만진다.** 거리·FOV·화면 크기가 한 방정식에 묶여 있어
	//   둘을 같이 만지면 수렴하지 않는다. FOV 와 피치를 고정하고 거리로 맞춘다.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 1600.f;

	// ⚠⚠ **Yaw 가 0 이 아니다.** 원작은 월드 축에 대해 비스듬히 본다.
	//   ER_Level 의 같은 위치를 비교한 결과: 우리는 도로를 직각으로 보고 있었고
	//   원작은 대각선으로 본다. 도로의 평행선이 원작에서는 수렴하고 우리는 평행했다.
	//
	// ⚠ 이 값을 바꾸면 가장자리 스크롤의 화면->월드 대응도 같이 움직인다.
	//   그래서 ERPlayerController 가 **카메라 회전에서 계산**하도록 만들어 뒀다.
	//   하드코딩된 축이 없으니 여기만 고치면 된다.
	CameraBoom->SetRelativeRotation(FRotator(-44.f, CameraYaw, 0.f));

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
	TopDownCamera->SetFieldOfView(63.f);

	// 컨트롤러 회전을 폰에 그대로 먹이지 않는다. 이동 방향으로 도는 것은 F05 에서 붙인다.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw   = false;
	bUseControllerRotationRoll  = false;

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
