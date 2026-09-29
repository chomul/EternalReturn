// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Item/ERItemTypes.h"

class UAnimInstance;
class USkeletalMesh;

#include "ERPresentationData.generated.h"

/**
 * 연출 한 줄 — "이 키(동작)에는 이 애셋들" (F12.5-01 · Argument 39).
 *
 * ⭐ 키는 **스킬 슬롯 태그 그대로**(`Ability.Slot.Attack` · `.Q` …) 또는 `Pres.*` (춤 · 사망 …).
 *   스킬은 자기 슬롯만 알고, 어떤 애니가 나올지는 시전자가 고른다 (Argument 39 ① B).
 */
USTRUCT(BlueprintType)
struct FERPresentationEntry
{
	GENERATED_BODY()

	/** 무기 계열. None = 무기 무관. 무기 세트 DA 안에서는 무시된다 (세트 자체가 무기별). 스킨 덮어쓰기에서는 "이 무기일 때만". */
	UPROPERTY(EditDefaultsOnly, Category = "연출")
	EERWeaponType Weapon = EERWeaponType::None;

	/**
	 * 모드 (Argument 42 ⑤ — 사용자 2026-09-28 "줄에 모드 칸"). 비면 평소 줄. 값이 있으면 **그 모드 태그가 붙어 있는 동안만** 쓰이고 같은 키의 평소 줄을 덮는다.
	 *   예) 저격총 세트: { Mode.Sniper · Ability.Slot.Attack · Katja_Sniperrifle_Skill_Shot } — 모드 중 사격
	 * 모드 **상태**(진입 · 유지 · 해제 포즈)는 여기 아님 — AnimBP 상태머신 + 무기 레이어 ModeStart/Idle/Run/End (Argument 42 ④ MB).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "연출", meta = (Categories = "Mode"))
	FGameplayTag Mode;

	UPROPERTY(EditDefaultsOnly, Category = "연출", meta = (Categories = "Ability.Slot,Pres"))
	FGameplayTag Key;

	/**
	 * 애니(`UAnimSequenceBase` — 시퀀스 또는 몽타주) · 사운드 · 이펙트. 여러 개면 애니는 **번갈아**(atk01 → atk02), 소리는 랜덤 (05).
	 * ⭐ 한 동작짜리는 시퀀스를 그대로 넣는다 — GAS 가 동적 몽타주로 복제한다 (Argument 39 ⑤ Y). 여러 단계(Start/Loop/End)만 몽타주로.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "연출", meta = (AllowedClasses = "/Script/Engine.AnimSequenceBase,/Script/Engine.SoundBase,/Script/Niagara.NiagaraSystem"))
	TArray<TObjectPtr<UObject>> Assets;
};

/**
 * 연출 표 하나 — 캐릭터 기본 · 무기 세트 · 야생동물 (F12.5-01 · Argument 39 ③ T3).
 *
 * ⭐ 로드 단위를 나눈다 — 전 캐릭터 표 한 장은 판에 없는 캐릭터까지 전부 올린다 (사용자 2026-09-24).
 *   캐릭터 기본(`DA_Pres_<Char>`)은 캐릭터 DA 가 하드로 · 무기 세트(`DA_Pres_<Char>_<Weapon>`)는 여기서 **소프트** — 장착한 계열만 로드.
 * ⚠ 채우기는 `ER.Pres.Fill` (파일명 규칙) — 손으로 수백 줄 넣지 않는다.
 */
UCLASS(BlueprintType, Const, Meta = (DisplayName = "ER 연출 데이터"))
class ETERNALRETURN_API UERPresentationData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "연출")
	TArray<FERPresentationEntry> Entries;

	/** 무기 계열별 세트 (캐릭터 기본 DA 에서만). 소프트 — 장착하면 그때 로드한다 (Argument 39 개정 2). */
	UPROPERTY(EditDefaultsOnly, Category = "무기")
	TMap<EERWeaponType, TSoftObjectPtr<UERPresentationData>> WeaponSets;

	/**
	 * 대기 · 뛰기 자세 레이어 (`ALI_ERWeaponLayers` 구현, Argument 40 L2 · K2). 연출 컴포넌트가 메인 AnimBP 에 Link 한다.
	 *   무기 세트 DA: 그 무기의 자세 (`ABPL_<Char>_<Weapon>`)
	 *   캐릭터 기본 DA: **맨손** 자세 (`ABPL_<Char>_Unarmed` — Common_wait · Common_run) — 무기 없음 · 세트 로딩 중 · 세트에 레이어 없음일 때
	 */
	UPROPERTY(EditDefaultsOnly, Category = "무기")
	TSubclassOf<UAnimInstance> AnimLayer;
};

/**
 * 스킨 하나 (F12.5-01 · Argument 39 ② S2).
 *
 * ⭐ **기본과 다른 것만** 적는다 — 없는 키는 캐릭터 기본으로 떨어진다 (사용자 2026-09-24 "기본 세팅해 놓고 특정 스킨만 덧씌우는").
 *   교체 단위는 키 하나: 스킨에 그 키가 있으면 Assets 전체가 스킨 것.
 * ⚠ 캐릭터 DA 의 `Skins` 가 소프트로 든다 — 고른 스킨만 로드된다. 그래서 **기본을 S000 스킨에 두지 않는다** (다른 스킨 판에선 안 올라온다).
 */
UCLASS(BlueprintType, Const, Meta = (DisplayName = "ER 스킨 데이터"))
class ETERNALRETURN_API UERSkinData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 비면 BP 의 메시 그대로. */
	UPROPERTY(EditDefaultsOnly, Category = "몸")
	TObjectPtr<USkeletalMesh> Mesh;

	/** 비면 BP 의 AnimBP 그대로. */
	UPROPERTY(EditDefaultsOnly, Category = "몸")
	TSubclassOf<UAnimInstance> AnimClass;

	UPROPERTY(EditDefaultsOnly, Category = "연출")
	TArray<FERPresentationEntry> Overrides;
};
