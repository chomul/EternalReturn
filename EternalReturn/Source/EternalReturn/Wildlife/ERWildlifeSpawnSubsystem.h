// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ERWildlifeSpawnSubsystem.generated.h"

class AERWildlifeCharacter;
class AERWildlifeSpawnPoint;
class AERGameState;

/**
 * 야생동물 스폰 스케줄러 (F12-03). **서버 전용** — 클라에는 스폰된 동물 액터만 간다.
 *
 * ⭐ 시계는 AERGameState 의 페이즈 카운트다운 (Argument 34 A). 규칙은 종 정의(UERWildlifeData)가 든다:
 *   - 최초 생성: (일차 · 낮/밤 · 그 페이즈 타이머 남은 시간) 에 도달하면
 *   - 재생성: **처치 순간부터** RespawnSeconds (닭 · 박쥐 · 멧돼지 · 들개) / 다음 낮(늑대) / 다음 밤(곰) / 없음(보스)
 *   - 레벨: 스폰 시점에 max(기초 레벨, 섬 최고 실험체 레벨) `[자체]` — 살아 있는 개체는 바꾸지 않는다
 * ⭐ 1초 타이머로 슬롯을 훑는다 (Tick 금지). 디버그로 시계를 건너뛰어도(ER.Match.Skip) 같은 비교로 따라온다.
 */
UCLASS()
class ETERNALRETURN_API UERWildlifeSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** [디버그] 슬롯 상태 로그 (ER.Wild.Spawners). */
	void DumpSlots() const;

	/** 같은 자리의 살아 있는 다른 개체 — 무리 어그로 전파 (역기획서 §6.5 "무리 전체"). */
	void GetPackMates(const AERWildlifeCharacter* Animal, TArray<AERWildlifeCharacter*>& Out) const;

	/** 섬 최고 실험체 레벨 (PlayerArray 순회). 없으면 1. */
	int32 GetIslandMaxPlayerLevel() const;

private:
	/** 스폰 자리의 한 마리분. */
	struct FSlot
	{
		TWeakObjectPtr<AERWildlifeSpawnPoint> Point;
		int32 Index = 0;
		TWeakObjectPtr<AERWildlifeCharacter> Animal;
		bool bSpawnedOnce = false;
		/** 고정 주기 재생성 시각 (서버 월드 시각). < 0 = 대기 없음. */
		double RespawnAt = -1.0;
		/** 낮/밤 재생성 — 이 페이즈 번호 이상이 되면 스폰. < 0 = 대기 없음. */
		int32 RespawnPhase = -1;
	};

	void Poll();
	void SpawnSlot(int32 SlotIndex);
	void OnAnimalKilled(int32 SlotIndex);
	bool IsFirstSpawnDue(const FSlot& Slot, const AERGameState& GS) const;

	TArray<FSlot> Slots;
	FTimerHandle PollTimer;
};
