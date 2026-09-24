// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ERGameMode.h"
#include "Core/ERGameState.h"
#include "Core/ERMatchSettings.h"
#include "Core/ERPlayerController.h"
#include "Core/ERPlayerState.h"
#include "Character/ERCharacterBase.h"
#include "EternalReturn.h"
#include "TimerManager.h"

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

void AERGameMode::StartPlay()
{
	Super::StartPlay();
	if (UERMatchSettings::Get().bStartClockOnBeginPlay)
	{
		StartMatchClock();
	}
}

void AERGameMode::StartMatchClock()
{
	EnterPhase(0);
}

void AERGameMode::EnterPhase(int32 Index, float OverrideRemaining)
{
	const TArray<float>& Lengths = UERMatchSettings::Get().PhaseSeconds;
	AERGameState* GS = GetGameState<AERGameState>();
	if (!GS)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	if (!Lengths.IsValidIndex(Index))
	{
		// 마지막 페이즈가 끝났다 — 매치 종료 판정은 F14. 시계만 멈춘다.
		PhaseIndex = -1;
		UE_LOG(LogEternalReturn, Log, TEXT("[매치] 마지막 페이즈가 끝났다 — 시계 정지 (매치 종료는 F14)"));
		return;
	}
	PhaseIndex = Index;
	const int32 NewDay = Index / 2 + 1;
	const bool bNewNight = (Index % 2) == 1;
	const float Duration = FMath::Max(1.f, Lengths[Index]);
	const float Remaining = OverrideRemaining > 0.f ? FMath::Min(OverrideRemaining, Duration) : Duration;
	const float Now = static_cast<float>(GS->GetServerWorldTimeSeconds());

	GS->SetPhase(NewDay, bNewNight, Duration, Now + Remaining);
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &AERGameMode::OnPhaseTimerExpired, Remaining, false);
	if (OverrideRemaining <= 0.f)
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[매치] %d일차 %s 시작 — %.0f초"), NewDay, bNewNight ? TEXT("밤") : TEXT("낮"), Duration);
	}
}

void AERGameMode::OnPhaseTimerExpired()
{
	EnterPhase(PhaseIndex + 1);
}

void AERGameMode::JumpToPhase(int32 InDay, bool bInNight)
{
	EnterPhase(AERGameState::ToPhaseIndex(FMath::Max(1, InDay), bInNight));
}

void AERGameMode::SkipClock(float Seconds)
{
	AERGameState* GS = GetGameState<AERGameState>();
	float Left = Seconds;
	while (GS && Left > 0.f && PhaseIndex >= 0)
	{
		const float Remaining = GS->GetPhaseRemaining();
		if (Left < Remaining)
		{
			EnterPhase(PhaseIndex, Remaining - Left);   // 같은 페이즈 안 — 남은 시간만 줄인다
			return;
		}
		Left -= Remaining;
		EnterPhase(PhaseIndex + 1);
	}
}
