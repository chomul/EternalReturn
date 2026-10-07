// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Item/ERItemTypes.h"

class UAnimInstance;
class USkeletalMesh;
class USoundBase;

#include "ERPresentationData.generated.h"

/**
 * 붙이는 조각 하나 (Argument 64 W1 · B1) — 무기 · 소품. 각 머신이 자기 화면에만 만든다 (복제 안 함 · 데디 서버는 안 만든다).
 * ⚠ 손으로 안 채운다 — `ER.Pres.Fill <캐릭터>` 가 `Docs/3_EditorTasks/Data/Attach.json` 에서 넣는다.
 */
USTRUCT()
struct FERAttachPiece
{
	GENERATED_BODY()

	/** 스켈레탈 메시 또는 스태틱 메시 */
	UPROPERTY(EditDefaultsOnly, meta = (AllowedClasses = "/Script/Engine.SkeletalMesh,/Script/Engine.StaticMesh"))
	TObjectPtr<UObject> Mesh;

	/** 붙일 소켓 · 뼈 (몸 메시 · 발사 바이크는 루트). 비면 원점 */
	UPROPERTY(EditDefaultsOnly)
	FName Socket;

	/** 소켓 기준 위치 · 회전 · 크기 (보통은 소켓을 옮겨 맞춘다) */
	UPROPERTY(EditDefaultsOnly)
	FTransform Offset;
};

/** 조각 묶음 — TMap 값으로 쓰려고 (무기 하나 = 조각 여럿일 수 있다: 쌍권총 · 드론) */
USTRUCT()
struct FERAttachPieces
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TArray<FERAttachPiece> Pieces;
};

/** 소품 — 태그가 붙은 동안만 몸에 붙는다 (매그너스 R 바이크 = State.Riding) */
USTRUCT()
struct FERAttachProp
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TArray<FERAttachPiece> Pieces;

	/** 이 태그가 있는 동안 몸에 붙인다. 비면 몸에는 안 붙는다 (발사 바이크처럼 다른 액터만 쓴다) */
	UPROPERTY(EditDefaultsOnly)
	FGameplayTag ShowWhile;
};

/**
 * 스킬 몽타주 섹션 이름 (F19-01 K8 · 사용자 2026-10-02 "모든 스킬을 몽타주로 · 한 단계짜리도 통일" · 섹션 이름 Execute).
 *   스킬 하나 = 몽타주 하나 `AM_<캐릭터|종>_<무기>_<동작>`. 코드가 이 이름으로 넘긴다 — 섹션 이름을 **정확히** 이렇게.
 *   - `Execute` 가 있으면 **판정 순간**에 그 섹션으로 (멧돼지 돌진 · 순차 사격은 발마다) · 없으면 처음부터 끝까지 (한 단계 스킬)
 *   - `Loop` = 채널 · 발 사이 조준 (반복) · `End` = 마무리 (Execute → End 연결 · 쏠 사람이 없을 때)
 *   몽타주가 없으면 예전처럼 시퀀스 + `<슬롯>.Execute` 애니.
 */
namespace ERPresSection
{
	inline const FName Execute(TEXT("Execute"));
	inline const FName Loop(TEXT("Loop"));
	inline const FName End(TEXT("End"));
}

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
 * 음성 키 하나의 재생 규칙 (Argument 73 G-A). ⚠ 손으로 안 채운다 — `ER.Pres.Fill` 이 `Data/Presentation/_Voice.json` 에서.
 * 각 클라가 틀 때 본다 (복제 안 함). 누가 듣는지는 키를 보내는 길이 정한다 (주변 = 큐 · 본인 = Client RPC).
 */
USTRUCT()
struct FERVoiceRule
{
	GENERATED_BODY()

	/** 틀 확률 0~1 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float Chance = 1.f;

	/** 같은 키를 다시 말할 때까지 초 (이 캐릭터 · 이 머신) */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float Cooldown = 0.f;

	/** 하던 말을 끊고 말한다 (스킬 · 사망). 아니면 말하는 중엔 건너뜀 */
	UPROPERTY(EditDefaultsOnly)
	bool bInterrupt = false;
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
	/** 애니 줄 — 키 `Ability.Slot.*` · `Pres.Anim.*` (Argument 74 — 소리 · 음성은 아래 칸. ⚠ 이름 그대로 — 애셋 참조) */
	UPROPERTY(EditDefaultsOnly, Category = "연출", meta = (TitleProperty = "Key"))
	TArray<FERPresentationEntry> Entries;

	/** 효과음 줄 — 키 `Pres.Sfx.*` (Argument 74) */
	UPROPERTY(EditDefaultsOnly, Category = "소리", meta = (TitleProperty = "Key"))
	TArray<FERPresentationEntry> Sounds;

	/** 음성 줄 — 키 `Pres.Voice.*` (Argument 73 · 74) */
	UPROPERTY(EditDefaultsOnly, Category = "음성", meta = (TitleProperty = "Key"))
	TArray<FERPresentationEntry> Voices;

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

	/** 음성 키 → 재생 규칙 (캐릭터 기본 DA 에서만 · Argument 73). 없는 키 = 확률 1 · 간격 0 · 안 끊음 */
	UPROPERTY(EditDefaultsOnly, Category = "음성", meta = (Categories = "Pres.Voice"))
	TMap<FGameplayTag, FERVoiceRule> VoiceRules;
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

	/** 애니 덮어쓰기 (Argument 74 — 소리 · 음성은 아래 칸. ⚠ 이름 그대로) */
	UPROPERTY(EditDefaultsOnly, Category = "연출", meta = (TitleProperty = "Key"))
	TArray<FERPresentationEntry> Overrides;

	/** 효과음 덮어쓰기 — 키 `Pres.Sfx.*` */
	UPROPERTY(EditDefaultsOnly, Category = "소리", meta = (TitleProperty = "Key"))
	TArray<FERPresentationEntry> OverrideSounds;

	/** 음성 덮어쓰기 — 키 `Pres.Voice.*` */
	UPROPERTY(EditDefaultsOnly, Category = "음성", meta = (TitleProperty = "Key"))
	TArray<FERPresentationEntry> OverrideVoices;

	/**
	 * 모션 소리 바꿈 (Argument 59 N2) — 애니 노티파이 `ER 소리` 가 기본 소리(S000)를 들고 있으면 이 스킨에선 짝을 튼다.
	 * ⚠ 손으로 안 채운다 — `ER.Pres.Fill` 이 Character_FX 의 S000 과 이 스킨 폴더에서 **같은 파일명**을 짝짓는다 (원본 규칙: 스킨 소리 = 같은 이름).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "연출")
	TMap<TObjectPtr<USoundBase>, TObjectPtr<USoundBase>> SoundSwaps;

	/** 손에 드는 무기 — 장착 무기 종류마다 (Argument 64 W1). 없는 종류는 아무것도 안 붙는다. ⚠ Fill 이 Attach.json 에서 */
	UPROPERTY(EditDefaultsOnly, Category = "부착")
	TMap<EERWeaponType, FERAttachPieces> WeaponMeshes;

	/** 소품 — 키(Pres.Prop.*) → 조각 · 켜는 태그 (Argument 64 B1). ⚠ Fill 이 Attach.json 에서 */
	UPROPERTY(EditDefaultsOnly, Category = "부착", meta = (Categories = "Pres.Prop"))
	TMap<FGameplayTag, FERAttachProp> Props;
};
