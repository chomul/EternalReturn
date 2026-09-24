// Copyright Epic Games, Inc. All Rights Reserved.
//
// [임시] F12-03 매치 시계 디버그 명령 — 26분을 기다리지 않고 스폰 스케줄을 검증한다. F14 에서 정리한다.

#include "Core/ERGameMode.h"
#include "Core/ERGameState.h"
#include "Engine/World.h"
#include "EternalReturn.h"
#include "HAL/IConsoleManager.h"

namespace
{

AERGameMode* ServerGameMode(UWorld* World)
{
	AERGameMode* GM = World ? World->GetAuthGameMode<AERGameMode>() : nullptr;
	if (!GM)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[매치디버그] 서버 창에서만 된다."));
	}
	return GM;
}

// ER.Match.Show — 지금 일차 · 낮/밤 · 남은 시간 (클라 창에서도 — 복제 확인용)
void MatchShowCmd(const TArray<FString>&, UWorld* World)
{
	const AERGameState* GS = World ? World->GetGameState<AERGameState>() : nullptr;
	if (!GS)
	{
		return;
	}
	const float Remain = GS->GetPhaseRemaining();
	UE_LOG(LogEternalReturn, Warning, TEXT("[매치디버그][%s] %d일차 %s  %02d:%02d 남음 (페이즈 %.0f초)"),
		World->GetNetMode() == NM_Client ? TEXT("클라") : TEXT("서버"), GS->GetDay(), GS->IsNight() ? TEXT("밤") : TEXT("낮"),
		FMath::FloorToInt(Remain) / 60, FMath::FloorToInt(Remain) % 60, GS->GetPhaseDuration());
}

// ER.Match.Phase <일> <낮|밤> — 그 페이즈의 처음으로
void MatchPhaseCmd(const TArray<FString>& Args, UWorld* World)
{
	AERGameMode* GM = ServerGameMode(World);
	if (!GM || Args.Num() < 2)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[매치디버그] 사용법: ER.Match.Phase <일> <낮|밤>"));
		return;
	}
	const bool bNight = Args[1] == TEXT("밤") || Args[1].Equals(TEXT("night"), ESearchCase::IgnoreCase);
	GM->JumpToPhase(FCString::Atoi(*Args[0]), bNight);
}

// ER.Match.Skip <초> — 시계를 앞으로. 페이즈 경계를 넘으면 이어서 넘긴다
void MatchSkipCmd(const TArray<FString>& Args, UWorld* World)
{
	AERGameMode* GM = ServerGameMode(World);
	if (!GM || Args.Num() < 1)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[매치디버그] 사용법: ER.Match.Skip <초>"));
		return;
	}
	GM->SkipClock(FCString::Atof(*Args[0]));
	MatchShowCmd(Args, World);
}

static FAutoConsoleCommandWithWorldAndArgs GERMatchShowCmd(
	TEXT("ER.Match.Show"), TEXT("[임시] 매치 시계 — 일차 · 낮/밤 · 남은 시간"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MatchShowCmd));
static FAutoConsoleCommandWithWorldAndArgs GERMatchPhaseCmd(
	TEXT("ER.Match.Phase"), TEXT("[임시] 페이즈로 건너뛰기 (서버 창). ER.Match.Phase <일> <낮|밤>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MatchPhaseCmd));
static FAutoConsoleCommandWithWorldAndArgs GERMatchSkipCmd(
	TEXT("ER.Match.Skip"), TEXT("[임시] 시계 앞으로 (서버 창). ER.Match.Skip <초>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MatchSkipCmd));

}
