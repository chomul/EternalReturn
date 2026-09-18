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
 * 무기군 하나의 숙련도 (F10-04). 무기군별로 따로 쌓인다 (재키 도끼 5 · 단검 2 동시 가능). 소유자만 본다 — 적의 D 해금을 숨긴다 (§7).
 * ⚠ TArray 원소다 — TMap 은 복제되지 않는다.
 */
USTRUCT()
struct FERWeaponProficiency
{
	GENERATED_BODY()

	UPROPERTY()
	EERWeaponType WeaponType = EERWeaponType::None;

	/** 현재 레벨에서 쌓인 경험치. 피해 100당 63 처럼 소수가 나와 float. */
	UPROPERTY()
	float Exp = 0.f;

	UPROPERTY()
	int32 Level = 1;
};

/** 경험치 출처. 지금은 로그 구분용 — 출처별 계수는 (미확인). */
UENUM()
enum class EERExpSource : uint8
{
	/** 디버그 명령 */
	Debug,
	/** 야생동물 처치 (값은 F12) */
	Wildlife,
	/** 실험체 처치 */
	PlayerKill,
};
