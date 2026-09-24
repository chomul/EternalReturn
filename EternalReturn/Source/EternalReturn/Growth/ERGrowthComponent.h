// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "Growth/ERGrowthTypes.h"
#include "ERGrowthComponent.generated.h"

/**
 * 성장 — 숙련도 6종 · 실험체 레벨 (F10-01 · 03 · 04 · 05). PlayerState 의 서브오브젝트 — 부활로 폰이 바뀌어도 남는다 (Argument 22 A).
 *
 * ⭐⭐ **경험치 입구는 AddProficiencyExp 하나.** 숙련도가 오르면 그만큼(× LevelExpPerProficiencyExp) 실험체 경험치가 오른다 —
 *   원작 "실험체 레벨은 사실상 전체 숙련도" (인게임 확인 2026-09-19 · Argument 26 B · E18 A6). 처치 경험치 같은 별도 값은 없다.
 *
 * 복제: Level 은 전원(시야 안 적의 레벨은 보인다 §7) · Exp · Proficiencies 는 소유자만. 판정은 전부 서버.
 */
UCLASS(ClassGroup = (ER), meta = (BlueprintSpawnableComponent))
class ETERNALRETURN_API UERGrowthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UERGrowthComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ── 실험체 레벨 ────────────────────────────────────────────
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnLevelUp, int32 /*NewLevel*/);
	FOnLevelUp OnLevelUp;

	int32 GetLevel() const { return Level; }
	float GetExp() const { return Exp; }
	static int32 GetMaxLevel();
	static const FERLevelExpRow* FindLevelRow(int32 InLevel);

	// ── 숙련도 6종 (F10-05) ────────────────────────────────────
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnProficiencyLevelUp, const FERProficiencyKey& /*Key*/, int32 /*NewLevel*/);
	/** 어느 트랙이든 레벨업. 무기 트랙은 PS 가 받아 D 해금 (F11-03). 나머지 트랙의 효과는 (미확인) — 자리만. */
	FOnProficiencyLevelUp OnProficiencyLevelUp;

	/** ⭐ [서버] 유일한 경험치 입구. ① 그 트랙 레벨업(DT_ProficiencyExp) → OnProficiencyLevelUp ② 실험체 Exp += Amount × 비율 → 레벨업. */
	void AddProficiencyExp(const FERProficiencyKey& Key, float Amount, const TCHAR* Reason);

	int32 GetProficiencyLevel(const FERProficiencyKey& Key) const;
	int32 GetWeaponProficiencyLevel(EERWeaponType WeaponType) const { return GetProficiencyLevel(FERProficiencyKey::Weapon(WeaponType)); }
	const TArray<FERProficiency>& GetProficiencies() const { return Proficiencies; }
	static int32 GetProficiencyMaxLevel();
	/** Lv N → N+1 필요 경험치 (표). 행이 없으면 0 (= 못 오른다, Error 로그). */
	static float ProficiencyRequiredExp(EERProficiencyTrack Track, int32 InLevel);

	// ── 적립 훅 — 이벤트가 부른다 (PS · Inventory · F12) ──────
	/** 내가 준 피해 → 든 무기군 (대상이 야생동물이면 계수 다름). PS 의 OnDamageTaken(가해자 쪽) */
	void OnDamageDealt(AActor* Target, float Damage);
	/** 내가 받은 피해 → 방어. PS 의 OnDamageTaken(피격자 쪽) */
	void OnDamageTaken(float Damage);
	/** 실험체 처치 → 든 무기군 (적 레벨 함수). PS 의 OnOutOfHealth(처치자 쪽) */
	void OnPlayerKilled(int32 VictimLevel);
	/** 야생동물 처치 → 사냥 (동물 레벨 함수). F12 가 부른다 */
	void OnWildlifeKilled(int32 WildlifeLevel, float HuntExp);
	/** 제작 → 제작 트랙 + (무기면) 그 무기군. Inventory->OnItemCrafted */
	void OnItemCrafted(FName ResultId, bool bFirstTime);
	/** 상자를 처음 열었다 → 탐색. Inventory->OnBoxOpened */
	void OnBoxOpened(FName LootRow);
	/** 든 무기군에 직접 (디버그). */
	void AddEquippedWeaponProficiencyExp(float Amount, const TCHAR* Reason);

	/** [서버] 든 무기군 숙련도 → 공속 · 증폭 GE 재적용 (F10-04 · E18 A9). 장착 변경 · 무기 트랙 레벨업 때. */
	void RefreshProficiencyBonus();

protected:
	UFUNCTION()
	void OnRep_Level();

	UPROPERTY(Replicated)
	TArray<FERProficiency> Proficiencies;

	FActiveGameplayEffectHandle ProficiencyEffectHandle;

	UPROPERTY(ReplicatedUsing = OnRep_Level)
	int32 Level = 1;

	/** 현재 레벨에서 쌓인 실험체 경험치. 숙련도 × 비율이라 소수. */
	UPROPERTY(Replicated)
	float Exp = 0.f;

private:
	static const class UDataTable* GetLevelTable();
	static const class UDataTable* GetProficiencyTable();
	EERWeaponType GetEquippedWeaponType() const;
	/** 실험체 경험치 적립 + 레벨업 (AddProficiencyExp 만 부른다). */
	void AddCharacterExp(float Amount, const TCHAR* Reason);
	void ApplyLevelGrowth(int32 NewLevel);
	/** 이동 트랙 — 1초마다 폰 위치 차이. 서버. */
	void SampleMovement();

	FTimerHandle MoveSampleTimer;
	FVector LastSampledLocation = FVector::ZeroVector;
	bool bHasLastSample = false;
	float MoveAccumMeters = 0.f;
};
