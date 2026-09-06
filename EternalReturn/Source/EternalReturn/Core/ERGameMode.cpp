// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ERGameMode.h"
#include "Core/ERGameState.h"
#include "Core/ERPlayerController.h"
#include "Core/ERPlayerState.h"
#include "Character/ERCharacterBase.h"
#include "EternalReturn.h"

AERGameMode::AERGameMode()
{
	DefaultPawnClass      = AERCharacterBase::StaticClass();
	PlayerStateClass      = AERPlayerState::StaticClass();
	PlayerControllerClass = AERPlayerController::StaticClass();
	GameStateClass        = AERGameState::StaticClass();
}

void AERGameMode::InitGameState()
{
	Super::InitGameState();

	if (AERGameState* ERGameState = GetGameState<AERGameState>())
	{
		ERGameState->InitTeams(TeamCount);
	}
}

void AERGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// GameMode 는 서버에만 있으므로 여기 도달했다면 이미 서버다.
	// 그래도 값 변경 앞에는 가드를 둔다.
	if (!HasAuthority() || !NewPlayer)
	{
		return;
	}

	AERPlayerState* ERPlayerState = NewPlayer->GetPlayerState<AERPlayerState>();
	AERGameState*   ERGameState   = GetGameState<AERGameState>();
	if (!ERPlayerState || !ERGameState)
	{
		// PostLogin 시점에 PlayerState 가 없을 수 있다. 로그만 남기고 넘어간다.
		UE_LOG(LogEternalReturn, Warning, TEXT("[Team] PostLogin — PlayerState/GameState 없음. 팀 배정을 건너뛴다"));
		return;
	}

	// 지금은 접속 순서대로 빈 자리를 채운다.
	// 파티 우선 배정은 매치메이킹이 생긴 뒤의 일이다. (미확인)
	const int32 AssignedTeam = ERGameState->AssignToFirstOpenTeam(ERPlayerState, TeamSize);

	if (AssignedTeam == INDEX_NONE)
	{
		// 자체 결정값: 정원 초과는 미배정으로 두고 접속은 막지 않는다.
		// 관전 처리로 바꿀지는 매치 진행 작업에서 정한다.
		UE_LOG(LogEternalReturn, Warning,
			TEXT("[Team] 정원 초과 — %s 는 미배정(INDEX_NONE)으로 남는다 (TeamSize=%d, TeamCount=%d)"),
			*GetNameSafe(ERPlayerState), TeamSize, TeamCount);
		return;
	}

	ERPlayerState->TeamId = AssignedTeam;

	UE_LOG(LogEternalReturn, Log,
		TEXT("[Team] %s → Team %d (%d/%d)"),
		*GetNameSafe(ERPlayerState), AssignedTeam,
		ERGameState->GetTeamMemberCount(AssignedTeam), TeamSize);
}

void AERGameMode::Logout(AController* Exiting)
{
	if (HasAuthority() && Exiting)
	{
		if (AERGameState* ERGameState = GetGameState<AERGameState>())
		{
			ERGameState->RemoveFromTeams(Exiting->GetPlayerState<AERPlayerState>());
		}
	}

	Super::Logout(Exiting);
}
