// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "ERSoundSettings.generated.h"

class USoundBase;

/** 공용 소리 한 상황 — 소리 중 무작위 · Count 번 이어서 (매번 다시 고름). */
USTRUCT()
struct FERCommonSound
{
	GENERATED_BODY()

	/** 무작위로 하나 */
	UPROPERTY(config, EditAnywhere)
	TArray<TSoftObjectPtr<USoundBase>> Sounds;

	/** 몇 번 이어서 (제작 — 사용자 2026-10-08 "여러개가 연속으로 섞여서") */
	UPROPERTY(config, EditAnywhere, meta = (ClampMin = "1"))
	int32 Count = 1;

	/** 사이 간격 (초) */
	UPROPERTY(config, EditAnywhere, meta = (ClampMin = "0"))
	float Interval = 0.f;
};

/**
 * 캐릭터와 무관한 공용 소리 (Argument 78 T2). Project Settings > Game > ER Sound.
 *
 * ⭐ 원칙 — 캐릭터 · 스킨마다 다름 = 캐릭터 연출 DA · 물체마다 다름 = DT_Loot 행 · **누구든 같음 = 여기**.
 *   재생은 UERSoundSubsystem — 캐릭터 DA 에 같은 키가 있으면 그쪽이 먼저다 (스킨이 덮어쓸 길을 남긴다).
 * 상황을 더하려면 태그(ERGameplayTags) + 칸 하나 + Find 의 표 한 줄.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "ER Sound"))
class UERSoundSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UERSoundSettings& Get() { return *GetDefault<UERSoundSettings>(); }

	/** 키(Pres.Sfx.*) → 칸. 없는 키 = nullptr */
	const FERCommonSound* Find(FGameplayTag Key) const;

	/** 칸 전부 (키 · 칸) — 서브시스템이 미리 로드 · Fill 이 옛 복사 줄을 지울 때 */
	TArray<TPair<FGameplayTag, const FERCommonSound*>> GetAll() const;

	/** 빈사로 쓰러질 때 — 주변 (Pres.Sfx.Downed) */
	UPROPERTY(config, EditAnywhere, Category = "빈사 · 사망")
	FERCommonSound Downed;

	/** 실험체 사망 — 주변 (Pres.Sfx.Die · 야생동물은 종마다 다르니 야생동물 DA) */
	UPROPERTY(config, EditAnywhere, Category = "빈사 · 사망")
	FERCommonSound Die;

	/** 소생돼 일어설 때 — 주변 (Pres.Sfx.Resurrect) */
	UPROPERTY(config, EditAnywhere, Category = "빈사 · 사망")
	FERCommonSound Resurrect;

	/** 팀원 빈사 알림 — 우리 팀만 2D (Pres.Sfx.TeamDown) */
	UPROPERTY(config, EditAnywhere, Category = "팀 알림")
	FERCommonSound TeamDown;

	/** 팀원 소생 완료 — 우리 팀만 2D (Pres.Sfx.TeamRevival) */
	UPROPERTY(config, EditAnywhere, Category = "팀 알림")
	FERCommonSound TeamRevival;

	/** 적 팀 전멸 (터미네이트) — 전멸시킨 팀만 2D (Pres.Sfx.Terminate) */
	UPROPERTY(config, EditAnywhere, Category = "팀 알림")
	FERCommonSound Terminate;

	/** 제작 완료 — 주변 · 여러 번 이어서 (Pres.Sfx.Craft) */
	UPROPERTY(config, EditAnywhere, Category = "행동")
	FERCommonSound Craft;

	/** 상자 처음 열 때 UI 소리 — 본인 2D (Pres.Sfx.BoxOpenUI · 상자 모양 소리는 DT_Loot OpenSounds) */
	UPROPERTY(config, EditAnywhere, Category = "행동")
	FERCommonSound BoxOpenUI;
};
