// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ERItemSettings.generated.h"

class UDataTable;

/**
 * 아이템 테이블의 위치. Project Settings > Game > ER Items.
 *
 * DeveloperSettings 인 이유는 ERStatCapSettings 와 같다 — 프로젝트 전역 값 하나, 애셋 없이 ini 로 diff.
 * 코드가 테이블 경로를 하드코딩하지 않는다 (StaticLoadObject 에 "/Game/..." 을 박으면 애셋 이동 때 조용히 깨진다).
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "ER Items"))
class UERItemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UERItemSettings& Get() { return *GetDefault<UERItemSettings>(); }

	/** 아이템 정의 테이블 (`DT_Items`). 행 구조는 FERItemRow 여야 한다 — 아니면 ERItem::GetTable 이 Error 를 낸다. */
	UPROPERTY(config, EditAnywhere, Category = "아이템", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> ItemTable;

	/** 루트 테이블 (`DT_Loot`, F09-03). 행 구조 FERLootRow — 상자 · 채집물 · (F12) 야생동물 드랍. */
	UPROPERTY(config, EditAnywhere, Category = "아이템", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> LootTable;

	/** 가방 칸 수 (장비 칸 5 제외). ⭐ 원작 확인값 10 (사용자 확인 2026-09-17). */
	UPROPERTY(config, EditAnywhere, Category = "인벤토리", meta = (ClampMin = "1"))
	int32 InventorySlots = 10;

	/** 습득 가능 거리 (m). ⚠ 자체 결정값 — 원작 (미확인) (장비 역기획서 §7.4). 서버가 이 거리로 검증한다. */
	UPROPERTY(config, EditAnywhere, Category = "드롭", meta = (ClampMin = "0.1"))
	float PickupRange = 2.f;

	/** 시체(드롭 액터) 소멸 시간(초). 0 = 정리 안 함. ⚠ 자체 결정값 — 원작 (미확인) (§7.3). */
	UPROPERTY(config, EditAnywhere, Category = "드롭", meta = (ClampMin = "0"))
	float DropLifetime = 0.f;
};
