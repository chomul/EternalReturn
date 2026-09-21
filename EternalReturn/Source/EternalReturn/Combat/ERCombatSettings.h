// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ERCombatSettings.generated.h"

/** 전투 상태 (F11-04 에서 만듦 — 무기 교체 판정. 비전투 재생 · 귀환도 이걸 쓴다). Project Settings > Game > ER Combat. */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "ER Combat"))
class UERCombatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UERCombatSettings& Get() { return *GetDefault<UERCombatSettings>(); }

	/** 피해를 주거나 받은 뒤 전투 상태(State.InCombat)가 유지되는 초. 다시 피해가 오가면 갱신. ⚠ 자체 결정값 — 원작 (미확인). */
	UPROPERTY(config, EditAnywhere, Category = "전투 상태", meta = (ClampMin = "0.1"))
	float CombatStateSeconds = 5.f;
};
