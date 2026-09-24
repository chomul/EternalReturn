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
 * 지금은 프레임워크 배선과 팀 배정만 한다. 페이즈 전환은 별도 작업에서 얹는다.
 */
UCLASS()
class AERGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AERGameMode();

	virtual void InitGameState() override;
	virtual void StartPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	// ── 매치 시계 (F12-03 · Argument 34 A) — 서버 타이머로 페이즈를 넘긴다 (Tick 없음) ──
	/** [서버] 1일차 낮부터 시작. */
	void StartMatchClock();
	/** [서버 · 디버그] 해당 페이즈의 처음으로 건너뛴다. */
	void JumpToPhase(int32 InDay, bool bInNight);
	/** [서버 · 디버그] 시계를 Seconds 만큼 앞으로 — 페이즈 경계를 넘으면 다음 페이즈로 이어진다. */
	void SkipClock(float Seconds);

protected:
	void EnterPhase(int32 Index, float OverrideRemaining = -1.f);
	void OnPhaseTimerExpired();

	/** 지금 페이즈 (0 = 1일차 낮). -1 = 시계 전 · 끝남. */
	int32 PhaseIndex = -1;
	FTimerHandle PhaseTimer;

	/**
	 * 한 팀의 인원. 기본 3 (3인 × 8팀 = 24명).
	 *
	 * ⭐ 상수로 박지 않는다. PIE 는 2~4명으로 테스트하므로 3으로 고정하면
	 *   팀이 하나밖에 안 생겨서 팀 구분을 확인할 수 없다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Match", meta = (ClampMin = "1"))
	int32 TeamSize = 3;

	/** 팀 개수. 기본 8. */
	UPROPERTY(EditDefaultsOnly, Category = "Match", meta = (ClampMin = "1"))
	int32 TeamCount = 8;
};
