// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ERGameMode.h"
#include "Core/ERPlayerState.h"
#include "Character/ERCharacterBase.h"

AERGameMode::AERGameMode()
{
	DefaultPawnClass = AERCharacterBase::StaticClass();
	PlayerStateClass = AERPlayerState::StaticClass();
}
