// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "ERGameMode.generated.h"

/**
 * 
 */
UCLASS()
class ETERNALRETURN_API AERGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	AERGameMode();
	
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
};
