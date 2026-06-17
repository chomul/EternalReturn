// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/ERPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Character/InputDataAsset.h"


AERPlayerController::AERPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	CachedDestination = FVector::ZeroVector;
	FollowTime = 0.f;
	bIsTouch = false;
	ShortPressThreshold = 0.5f;

}

void AERPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (IsLocalController())
	{
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
			{
				Subsystem->AddMappingContext(InputConfig->DefaultMappingContext, 0);
			}
		}
	}

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// Setup mouse input events
		EnhancedInputComponent->BindAction(InputConfig->SetDestinationClickAction, ETriggerEvent::Started, this, &AERPlayerController::OnInputStarted);
		EnhancedInputComponent->BindAction(InputConfig->SetDestinationClickAction, ETriggerEvent::Triggered, this, &AERPlayerController::OnSetDestinationTriggered);
		EnhancedInputComponent->BindAction(InputConfig->SetDestinationClickAction, ETriggerEvent::Completed, this, &AERPlayerController::OnSetDestinationReleased);
		EnhancedInputComponent->BindAction(InputConfig->SetDestinationClickAction, ETriggerEvent::Canceled, this, &AERPlayerController::OnSetDestinationReleased);

		// Setup touch input events
		EnhancedInputComponent->BindAction(InputConfig->SetDestinationTouchAction, ETriggerEvent::Started, this, &AERPlayerController::OnInputStarted);
		EnhancedInputComponent->BindAction(InputConfig->SetDestinationTouchAction, ETriggerEvent::Triggered, this, &AERPlayerController::OnTouchTriggered);
		EnhancedInputComponent->BindAction(InputConfig->SetDestinationTouchAction, ETriggerEvent::Completed, this, &AERPlayerController::OnTouchReleased);
		EnhancedInputComponent->BindAction(InputConfig->SetDestinationTouchAction, ETriggerEvent::Canceled, this, &AERPlayerController::OnTouchReleased);

		// Skill 
		EnhancedInputComponent->BindAction(InputConfig->SetSkillQAction, ETriggerEvent::Completed, this, &AERPlayerController::OnSkill_Q_Released);
		EnhancedInputComponent->BindAction(InputConfig->SetSkillWAction, ETriggerEvent::Completed, this, &AERPlayerController::OnSkill_W_Released);
		EnhancedInputComponent->BindAction(InputConfig->SetSkillEAction, ETriggerEvent::Completed, this, &AERPlayerController::OnSkill_E_Released);
		EnhancedInputComponent->BindAction(InputConfig->SetSkillRAction, ETriggerEvent::Completed, this, &AERPlayerController::OnSkill_R_Released);
		EnhancedInputComponent->BindAction(InputConfig->SetSkillDAction, ETriggerEvent::Completed, this, &AERPlayerController::OnSkill_D_Released);
		EnhancedInputComponent->BindAction(InputConfig->SetSkillFAction, ETriggerEvent::Completed, this, &AERPlayerController::OnSkill_F_Released);
	}
}

void AERPlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void AERPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (NavigationPath.Num() == 0) return;

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	FVector CurrentTarget = NavigationPath[CurrentPathIndex];
	FVector Direction = CurrentTarget - ControlledPawn->GetActorLocation();

	// 현재 Waypoint 도달하면 다음으로
	if (Direction.Size2D() < 50.f)
	{
		CurrentPathIndex++;
		if (CurrentPathIndex >= NavigationPath.Num())
		{
			NavigationPath.Empty();
			return;
		}
		CurrentTarget = NavigationPath[CurrentPathIndex];
		Direction = CurrentTarget - ControlledPawn->GetActorLocation();
	}

	ControlledPawn->AddMovementInput(Direction.GetSafeNormal(), 1.f);
}

void AERPlayerController::OnInputStarted()
{
	StopMovement();
}

void AERPlayerController::OnSetDestinationTriggered()
{
	FollowTime += GetWorld()->GetDeltaSeconds();
	
	// We look for the location in the world where the player has pressed the input
	FHitResult Hit;
	bool bHitSuccessful = false;
	if (bIsTouch)
	{
		bHitSuccessful = GetHitResultUnderFinger(ETouchIndex::Touch1, ECollisionChannel::ECC_Visibility, true, Hit);
	}
	else
	{
		bHitSuccessful = GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit);
	}

	// If we hit a surface, cache the location
	if (bHitSuccessful)
	{
		CachedDestination = Hit.Location;
	}
	
	// Move towards mouse pointer or touch
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn != nullptr)
	{
		FVector WorldDirection = (CachedDestination - ControlledPawn->GetActorLocation()).GetSafeNormal();
		ControlledPawn->AddMovementInput(WorldDirection, 1.0, false);
	}
}

void AERPlayerController::OnSetDestinationReleased()
{
	if (FollowTime <= ShortPressThreshold)
	{
		// 클라이언트에서 직접 경로 계산
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		APawn* ControlledPawn = GetPawn();
        
		if (NavSys && ControlledPawn)
		{
			UNavigationPath* NavPath = NavSys->FindPathToLocationSynchronously(
				GetWorld(), ControlledPawn->GetActorLocation(), CachedDestination);
                
			if (NavPath && NavPath->IsValid())
			{
				NavigationPath = NavPath->PathPoints;
				CurrentPathIndex = 0;
			}
		}
		
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FXCursor, CachedDestination, FRotator::ZeroRotator, FVector(1.f, 1.f, 1.f), true, true, ENCPoolMethod::None, true);
	}

	FollowTime = 0.f;
}

void AERPlayerController::OnTouchTriggered()
{
	bIsTouch = true;
	OnSetDestinationTriggered();
}

void AERPlayerController::OnTouchReleased()
{
	bIsTouch = false;
	OnSetDestinationReleased();
}

void AERPlayerController::OnSkill_Q_Released()
{
}

void AERPlayerController::OnSkill_W_Released()
{
}

void AERPlayerController::OnSkill_E_Released()
{
}

void AERPlayerController::OnSkill_R_Released()
{
}

void AERPlayerController::OnSkill_D_Released()
{
}

void AERPlayerController::OnSkill_F_Released()
{
}