// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ERWeaponSettings.generated.h"

class UDataTable;

/** 무기 계열 테이블의 위치. Project Settings > Game > ER Weapons. (ERItemSettings 와 같은 이유로 DeveloperSettings) */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "ER Weapons"))
class UERWeaponSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UERWeaponSettings& Get() { return *GetDefault<UERWeaponSettings>(); }

	/** 무기 계열 테이블 (`DT_WeaponClass`). 행 구조 FERWeaponClassRow · RowName = 계열명. */
	UPROPERTY(config, EditAnywhere, Category = "무기", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> WeaponClassTable;
};
