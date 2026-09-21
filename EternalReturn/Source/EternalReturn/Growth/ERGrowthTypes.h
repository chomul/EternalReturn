// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Item/ERItemTypes.h"
#include "ERGrowthTypes.generated.h"

/**
 * 레벨업 필요 경험치 한 행. RowName = "Lv1" … (Lv N → N+1 에 필요한 경험치). **행 수 + 1 = 최대 레벨** — 20 을 코드에 박지 않는다.
 * ⭐ 원작 확인값은 1→2 = 800 · 5→6 = 2,500 뿐 (12.1 패치노트). 나머지는 `[자체]` — 출처를 행에 남긴다 (역기획서 §11).
 */
USTRUCT(BlueprintType)
struct FERLevelExpRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 다음 레벨까지 필요 경험치. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 RequiredExp = 1;

	/** 이 레벨업으로 받는 스킬 포인트. 기본 1 (§2.1). 지급은 F10-03. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 SkillPointGranted = 1;

	/** 값의 출처 — "확인" / "역산" / "자체". 나중에 원작 값을 구했을 때 갈아끼울 행을 찾기 위한 표시. 코드는 읽지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Source = TEXT("자체");
};

/**
 * 숙련도 트랙 6종 (F10-05 · E18 A10). ⭐ **실험체 레벨 = 이 6종에 쌓인 경험치의 합** (인게임 확인 2026-09-19, Argument 26 B).
 * Weapon 만 무기군별로 따로 (FERProficiencyKey.WeaponType), 나머지는 트랙 하나씩.
 */
UENUM()
enum class EERProficiencyTrack : uint8
{
	/** 무기 — 피해 · 처치 · 무기 제작. 무기군별. D 해금 · 공속 · 증폭 (F11-03 · F10-04) */
	Weapon,
	/** 방어 — 받은 피해. 레벨 효과 (미확인) */
	Defense,
	/** 사냥 — 야생동물 처치 (F12 가 부른다). 레벨 효과 (미확인) */
	Hunt,
	/** 제작 — 모든 제작. 레벨 효과 (미확인) */
	Craft,
	/** 탐색 — 상자 열기. 레벨 효과 (미확인) */
	Search,
	/** 이동 — 이동 거리. 레벨 효과 (미확인) */
	Move,
};

/** 트랙 + (무기면) 무기군. 숙련도 항목의 열쇠. */
USTRUCT()
struct FERProficiencyKey
{
	GENERATED_BODY()

	UPROPERTY()
	EERProficiencyTrack Track = EERProficiencyTrack::Weapon;

	/** Track == Weapon 일 때만 의미. */
	UPROPERTY()
	EERWeaponType WeaponType = EERWeaponType::None;

	static FERProficiencyKey Weapon(EERWeaponType Type) { FERProficiencyKey K; K.Track = EERProficiencyTrack::Weapon; K.WeaponType = Type; return K; }
	static FERProficiencyKey Of(EERProficiencyTrack InTrack) { FERProficiencyKey K; K.Track = InTrack; return K; }

	bool operator==(const FERProficiencyKey& O) const
	{
		return Track == O.Track && (Track != EERProficiencyTrack::Weapon || WeaponType == O.WeaponType);
	}
	bool IsValid() const { return Track != EERProficiencyTrack::Weapon || WeaponType != EERWeaponType::None; }
	FString ToString() const;
};

/**
 * 숙련도 항목 하나 (트랙 또는 무기군). 소유자만 본다 — 적의 D 해금을 숨긴다 (§7).
 * ⚠ TArray 원소다 — TMap 은 복제되지 않는다.
 */
USTRUCT()
struct FERProficiency
{
	GENERATED_BODY()

	UPROPERTY()
	FERProficiencyKey Key;

	/** 현재 레벨에서 쌓인 경험치. 피해 100당 50 처럼 소수가 나와 float. */
	UPROPERTY()
	float Exp = 0.f;

	UPROPERTY()
	int32 Level = 1;
};

/**
 * 숙련도 필요 경험치 한 행 (`DT_ProficiencyExp`). RowName = "Lv1" … (Lv N → N+1). **행 수 + 1 = 숙련도 최대 레벨** (19행 → 20).
 * 6열 전부 **인게임 확인** (2026-09-19 · 역기획서 성장 §3.0). 무기 500·600·700·900·1650… · 방어 230+270n · 사냥 180+200n · 제작 370+125n · 이동 300+20n · 탐색 비선형.
 */
USTRUCT(BlueprintType)
struct FERProficiencyExpRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1")) int32 Weapon = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1")) int32 Defense = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1")) int32 Hunt = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1")) int32 Craft = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1")) int32 Search = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1")) int32 Move = 1;

	int32 Get(EERProficiencyTrack Track) const
	{
		switch (Track)
		{
		case EERProficiencyTrack::Weapon:  return Weapon;
		case EERProficiencyTrack::Defense: return Defense;
		case EERProficiencyTrack::Hunt:    return Hunt;
		case EERProficiencyTrack::Craft:   return Craft;
		case EERProficiencyTrack::Search:  return Search;
		case EERProficiencyTrack::Move:    return Move;
		}
		return 1;
	}
};
