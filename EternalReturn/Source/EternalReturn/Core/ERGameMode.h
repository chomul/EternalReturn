// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ERGameMode.generated.h"

/**
 * 매치 규칙의 서버측 주인.
 *
 * ⚠ GameMode 는 서버에만 존재한다. 클라에는 복제되지 않으므로
 *   클라가 알아야 하는 값은 GameState 에 둔다.
 *
 * 지금은 프레임워크 클래스 배선만 한다. 팀 배정·페이즈 전환은
 * 각각 별도 작업에서 얹는다.
 */
UCLASS()
class AERGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AERGameMode();
};
