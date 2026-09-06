<<<<<<< HEAD
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/ERCharacterBase.h"
#include "Core/ERPlayerState.h"
#include "EternalReturn.h"
#include "AbilitySystemComponent.h"

AERCharacterBase::AERCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;
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
=======
// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/ERCharacterBase.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"


// Sets default values
AERCharacterBase::AERCharacterBase()
{
	// Set size for player capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate character to camera direction
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Rotate character to moving direction
	GetCharacterMovement()->RotationRate = FRotator(0.f, 640.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;

	// Create a camera boom...
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true); // Don't want arm to rotate when character does
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	CameraBoom->bDoCollisionTest = false; // Don't want to pull camera in when it collides with level

	// Create a camera...
	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Activate ticking in order to update the cursor every frame.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bReplicates = true;
	SetReplicateMovement(true);
}

void AERCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void AERCharacterBase::BeginPlay()
{
	Super::BeginPlay();
>>>>>>> 6100863f0a6ea62466e1603dfe2b39fdbd5c6654
}
