// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ERPlayerController.generated.h"

class UNiagaraSystem;
class UInputMappingContext;
class UInputAction;
class UInputDataAsset;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS()
class ETERNALRETURN_API AERPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	AERPlayerController();

	/** Time Threshold to know if it was a short press */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	float ShortPressThreshold;

	/** FX Class that we will spawn when clicking */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UNiagaraSystem* FXCursor;

	/** MappingContext */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputDataAsset* InputConfig;

protected:
	/** True if the controlled character should navigate to the mouse cursor. */
	uint32 bMoveToMouseCursor : 1;

	virtual void SetupInputComponent() override;

	// To add mapping context
	virtual void BeginPlay();

	virtual void PlayerTick(float DeltaTime) override;

	/** Input handlers for SetDestination action. */
	void OnInputStarted();
	void OnSetDestinationTriggered();
	void OnSetDestinationReleased();
	void OnTouchTriggered();
	void OnTouchReleased();
	
	/* Skill */
	void OnSkill_Q_Released();
	void OnSkill_W_Released();
	void OnSkill_E_Released();
	void OnSkill_R_Released();
	void OnSkill_D_Released();
	void OnSkill_F_Released();

private:
	FVector CachedDestination;

	bool bIsTouch; // Is it a touch device
	float FollowTime; // For how long it has been pressed

	TArray<FVector> NavigationPath;
	int32 CurrentPathIndex = 0;
	
};


