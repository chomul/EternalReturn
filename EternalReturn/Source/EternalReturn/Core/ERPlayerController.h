// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ERPlayerController.generated.h"

/**
 * 소유 클라이언트와 서버에만 존재한다.
 *
 * AController 생성자가 bOnlyRelevantToOwner = true 로 시작하므로
 * (Controller.cpp:51) 남의 컨트롤러는 내 클라에 오지 않는다.
 * 즉 "나만 아는 것"을 두기 좋은 자리다.
 *
 * ⚠ 입력·카메라는 아직 넣지 않는다. 별도 작업에서 얹는다.
 */
UCLASS()
class AERPlayerController : public APlayerController
{
	GENERATED_BODY()
};
