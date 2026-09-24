// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ERMatchSettings.generated.h"

/**
 * 매치 시계 설정 (F12-03 · Argument 34 A). Project Settings > Game > ER Match.
 * ⭐ 시계 자체는 AERGameState(복제) + AERGameMode(서버 진행)에 있다 — F14 매치 진행이 그대로 이어받는다.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "ER Match"))
class UERMatchSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	static const UERMatchSettings& Get() { return *GetDefault<UERMatchSettings>(); }

	/**
	 * 페이즈 길이(초) — **[0] 1일차 낮 · [1] 1일차 밤 · [2] 2일차 낮 …** 순서.
	 * 1~6일차 · 7일차 낮 [확인] (사용자 2026-09-23 · 매치 역기획서 §1). 7일차 밤 60 · 8일차 낮 150 은 (미확인).
	 * 마지막 페이즈가 끝나면 시계가 멈춘다 (매치 종료는 F14).
	 */
	UPROPERTY(config, EditAnywhere, Category = "시계")
	TArray<float> PhaseSeconds = { 140.f, 110.f, 140.f, 120.f, 130.f, 110.f, 100.f, 110.f, 80.f, 80.f, 70.f, 50.f, 50.f, 60.f, 150.f };

	/** 게임 시작(StartPlay)과 함께 1일차 낮을 시작한다. 로비 · 시작 지역 선택은 F14 — 그때 끈다. */
	UPROPERTY(config, EditAnywhere, Category = "시계")
	bool bStartClockOnBeginPlay = true;
};
