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
}
