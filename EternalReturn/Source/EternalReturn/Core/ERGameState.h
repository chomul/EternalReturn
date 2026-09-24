// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ERGameState.generated.h"

/** 팀 하나의 구성. TArray<TArray<>> 는 복제되지 않으므로 한 겹 감싼다. */
USTRUCT()
struct FERTeamRoster
{
	GENERATED_BODY()

	/** 이 팀에 속한 PlayerState 들. 서버가 채우고 전원에게 복제된다. */
	UPROPERTY()
	TArray<TObjectPtr<class APlayerState>> Members;
};

/**
 * 모든 클라이언트가 알아야 하는 매치 상태.
 *
 * GameMode 는 서버에만 있으므로, 클라가 봐야 하는 값은 여기 둔다.
 *
 * ⚠ bAlwaysRelevant = true 이고 NetPriority 가 높다(GameStateBase.cpp).
 *   즉 여기 넣는 것은 전원에게 간다 — 인벤토리 같은 큰 데이터를 넣지 않는다.
 *   팀 구성은 24명 규모라 문제없다.
 */
UCLASS()
class AERGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** [서버 전용] 팀 배열을 TeamCount 크기로 준비한다. 이미 그 크기면 아무것도 하지 않는다. */
	void InitTeams(int32 TeamCount);

	/**
	 * [서버 전용] 정원이 남은 첫 팀에 넣고 그 팀 번호를 돌려준다.
	 * 모든 팀이 찼으면 INDEX_NONE.
	 */
	int32 AssignToFirstOpenTeam(class APlayerState* PlayerState, int32 TeamSize);

	/** [서버 전용] 나간 플레이어를 팀에서 뺀다. */
	void RemoveFromTeams(class APlayerState* PlayerState);

	/** 해당 팀의 현재 인원. 범위를 벗어나면 0. */
	int32 GetTeamMemberCount(int32 TeamId) const;

	int32 GetTeamCount() const { return Teams.Num(); }

	// ── 매치 시계 (F12-03 · Argument 34 A) ─────────────────────────────
	// ⭐ 원작 시계는 **페이즈 카운트다운** 이다 — "1일차 02:20" = 1일차 낮 타이머가 2:20 남았을 때 (사용자 확인 2026-09-23).
	//   복제는 페이즈가 바뀔 때만. 남은 시간은 클라가 서버 시각으로 계산한다 (매 틱 복제하지 않는다).

	/** 페이즈가 바뀌었다 (Day, bNight). 서버 · 클라 모두 — 클라는 OnRep 에서. 야생동물 스폰 · (F14) 금지구역 · 부활 · 보스가 구독한다. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPhaseChanged, int32 /*Day*/, bool /*bNight*/);
	FOnPhaseChanged OnPhaseChanged;

	/** [서버 전용] AERGameMode 만 부른다. */
	void SetPhase(int32 InDay, bool bInNight, float InDuration, float InEndServerTime);

	/** 시계가 돌고 있나 (1일차 낮이 시작됐나). */
	bool IsClockRunning() const { return Day > 0; }
	int32 GetDay() const { return Day; }
	bool IsNight() const { return bNight; }
	/** 0 = 1일차 낮 · 1 = 1일차 밤 · 2 = 2일차 낮 … (야생동물 스폰 규칙 비교용). 시계 전이면 -1. */
	int32 GetPhaseIndex() const { return Day > 0 ? ToPhaseIndex(Day, bNight) : -1; }
	static int32 ToPhaseIndex(int32 InDay, bool bInNight) { return (InDay - 1) * 2 + (bInNight ? 1 : 0); }
	/** 이 페이즈의 남은 시간(초) — 원작 타이머 표기와 같은 값. 서버 · 클라 공통. */
	float GetPhaseRemaining() const;
	float GetPhaseDuration() const { return PhaseDuration; }

protected:
	UFUNCTION()
	void OnRep_Phase();

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	int32 Day = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	bool bNight = false;

	UPROPERTY(Replicated)
	float PhaseDuration = 0.f;

	/** 서버 월드 시각 기준 페이즈 종료 시각. 남은 시간 = 이 값 − GetServerWorldTimeSeconds(). */
	UPROPERTY(Replicated)
	float PhaseEndServerTime = 0.f;

	UPROPERTY(Replicated)
	TArray<FERTeamRoster> Teams;
};
