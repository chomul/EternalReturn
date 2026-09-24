// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ERWildlifeSpawnPoint.generated.h"

class UERWildlifeData;

/**
 * 야생동물 스폰 자리 (F12-03). 맵에 놓는다 — **로직 없음**, 데이터만. 스폰 · 재생성은 UERWildlifeSpawnSubsystem 이 한다.
 * ⭐ 원작: 같은 자리에 레벨이 오른 채 다시 스폰된다 (사용자 2026-09-23) — 그래서 "자리" 가 단위다.
 * 지역별 종 · 마리수(§3.2 · 12.0 조정값)는 F13 지역 배치가 이 액터를 놓는 것으로 채운다. 코드에 지역 이름 없음.
 */
UCLASS()
class ETERNALRETURN_API AERWildlifeSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AERWildlifeSpawnPoint();

	/** 종 정의 (DA_Wild_*). 스폰 시각 · 재생성 · 기초 레벨은 이 애셋이 정한다. */
	UPROPERTY(EditAnywhere, Category = "스폰")
	TObjectPtr<UERWildlifeData> Species;

	/** 이 자리의 마리 수 — 무리(변이 닭 무리 등)는 2 이상. 한 마리마다 따로 재생성된다 (bPackRespawn 이면 전원 함께). */
	UPROPERTY(EditAnywhere, Category = "스폰", meta = (ClampMin = "1", ClampMax = "10"))
	int32 Count = 1;

	/**
	 * 이 자리의 변이체 (예: 닭 자리 → DA_Wild_MutantChicken). 비면 변이 없음.
	 * ⭐ 원작 [확인] (나무위키 · 사용자 2026-09-24): 첫 스폰엔 **일반만** — 잡힌 뒤 **재생성할 때마다** 확률로 변이가 나온다.
	 *   확률은 Project Settings > ER Wildlife > `MutantChance` (전 자리 공통 · 기본 20% `[자체]`).
	 */
	UPROPERTY(EditAnywhere, Category = "스폰")
	TObjectPtr<UERWildlifeData> MutantSpecies;

	/**
	 * 무리 동시 리젠 — **전부 잡혀야** 타이머가 돈다 · 모두 같이 다시 나온다 [확인] (나무위키 "야생동물 무리" · 사용자 2026-09-24).
	 * 변이 무리 자리에 켠다. 늑대 2마리처럼 따로 리젠하는 무리는 끈다 (먼저 나온 늑대를 때리면 울부짖기 — 원작).
	 */
	UPROPERTY(EditAnywhere, Category = "스폰")
	bool bPackRespawn = false;

	/** 무리일 때 흩어지는 반경 (cm). 자리 중심에서 이 안의 무작위 위치 · 한 마리면 무시. */
	UPROPERTY(EditAnywhere, Category = "스폰", meta = (ClampMin = "0"))
	float ScatterRadius = 150.f;

	/** [서버] i 번째 개체의 스폰 위치. 한 마리면 자리 그대로. */
	FVector GetSpawnLocation(int32 Index) const;
};
