<<<<<<< HEAD
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ERGameMode.h"
#include "Core/ERPlayerState.h"
#include "Character/ERCharacterBase.h"

AERGameMode::AERGameMode()
{
	DefaultPawnClass = AERCharacterBase::StaticClass();
	PlayerStateClass = AERPlayerState::StaticClass();
=======
// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/ERGameMode.h"

#include "ERPlayerController.h"


AERGameMode::AERGameMode()
{
	PlayerControllerClass = AERPlayerController::StaticClass();


}

void AERGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
}

void AERGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
>>>>>>> 6100863f0a6ea62466e1603dfe2b39fdbd5c6654
}
