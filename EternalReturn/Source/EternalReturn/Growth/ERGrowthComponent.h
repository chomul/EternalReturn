// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "Growth/ERGrowthTypes.h"
#include "ERGrowthComponent.generated.h"

/**
 * 성장 — **PlayerState 에 붙는다** (Docs/4_Argument/22_실험체레벨_저장형태.md 방안 A). 인벤토리 · 스킬 포인트와 같은 자리.
 *
 * F10-01 경험치 축적 · 실험체 레벨 · F10-02 레벨당 스탯(Instant GE, ApplyLevelGrowth) · F10-04 무기 숙련도(피해 → 경험치, Infinite 증폭 GE).
 * 스킬 포인트(03) 는 OnLevelUp 을 받아서 한다.
 *
 * ⭐ 경험치 추가 · 레벨업 판정은 **서버 전용.** Level 은 전원(상대 레벨 표시), Exp 는 소유자만 (역기획서 §7).
 * ⚠ 어트리뷰트가 아닌 이유: 어트리뷰트는 세트 단위로 전원 복제라 경험치를 숨길 수 없다 (Argument 22).
 */
UCLASS(ClassGroup = (ER), meta = (BlueprintSpawnableComponent))
class ETERNALRETURN_API UERGrowthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UERGrowthComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 레벨업마다 한 번 (여러 레벨이 한 번에 오르면 레벨마다). 서버 전용. 스탯 GE 적용 **뒤에** 불린다. 03 스킬 포인트가 받는다. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnLevelUp, int32 /*NewLevel*/);
	FOnLevelUp OnLevelUp;

	int32 GetLevel() const { return Level; }
	int32 GetExp() const { return Exp; }

	/** 테이블 행 수 + 1. 테이블이 없으면 1 (레벨업 불가 — Error 로그는 GetTable 이 낸다). */
	static int32 GetMaxLevel();

	/** Lv → Lv+1 필요 경험치. 행이 없으면 nullptr. */
	static const FERLevelExpRow* FindLevelRow(int32 InLevel);

	/**
	 * [서버] 경험치를 더하고 레벨업을 판정한다. 한 번에 여러 레벨이 오를 수 있다 (while).
	 * 최대 레벨이면 **더 쌓지 않는다** — 자체 결정값 (원작 (미확인)).
	 */
	void AddExp(int32 Amount, EERExpSource Source);

	// ── 무기 숙련도 (F10-04) ───────────────────────────────────

	/** 숙련도 레벨업마다. 서버 전용. F11 이 5 / 10 / 15 를 해석해 D 를 부여 · 강화한다 — 여기서는 알리기만. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWeaponProficiencyLevelUp, EERWeaponType /*WeaponType*/, int32 /*NewLevel*/);
	FOnWeaponProficiencyLevelUp OnWeaponProficiencyLevelUp;

	/** 없으면 Lv.1 (아직 안 쌓인 무기군). */
	int32 GetWeaponProficiencyLevel(EERWeaponType WeaponType) const;
	const TArray<FERWeaponProficiency>& GetWeaponProficiencies() const { return WeaponProficiencies; }

	/** Lv → Lv+1 필요 숙련도 경험치 = ExpLv2 + Step × (Lv−1). */
	static float ProficiencyRequiredExp(int32 InLevel);

	/** [서버] 무기군 하나에 숙련도 경험치. 레벨업 판정 · 증폭 갱신 · 델리게이트. */
	void AddWeaponProficiencyExp(EERWeaponType WeaponType, float Amount, const TCHAR* Reason);

	/** [서버] **장착 중인 무기군**에 숙련도 경험치. 무기가 없으면 아무것도 안 한다 — 자체 결정값 (맨손 (미확인)). */
	void AddEquippedWeaponProficiencyExp(float Amount, const TCHAR* Reason);

	/** [서버] 무기를 제작했다 (F09-02 → UERInventoryComponent::OnItemCrafted) — 그 무기군 숙련도 += 등급값 × (최초 1 + FirstCraftBonus). 무기가 아니면 무시. */
	void OnItemCrafted(FName ResultId, bool bFirstTime);

	/**
	 * [서버] 내가 Target 에게 Damage 를 입혔다 — 피해 100당 실험체 63 / 야생동물 5 (대상 태그 Actor.Type.Wildlife) 를 장착 무기군에.
	 * 피격자의 UERAttributeSet::OnDamageTaken 을 받은 AERPlayerState 가 가해자의 이 함수를 부른다 (처치 경험치와 같은 경로).
	 */
	void OnDamageDealt(AActor* Target, float Damage);

	/**
	 * [서버] 장착 무기군 × 그 숙련도 레벨의 증폭 GE 를 다시 건다 (핸들 하나 — 제거 후 재적용). 무기가 없으면 제거만.
	 * 숙련도 레벨업 · 무기 교체(UERInventoryComponent::OnEquippedChanged) 때 부른다.
	 */
	void RefreshProficiencyBonus();

protected:
	UFUNCTION()
	void OnRep_Level();

	/** 무기군별 숙련도. 소유자만 (적의 D 해금 은닉, §7). 처음 경험치가 들어올 때 원소가 생긴다. */
	UPROPERTY(Replicated)
	TArray<FERWeaponProficiency> WeaponProficiencies;

	/** 서버 전용. 현재 걸려 있는 증폭 GE. */
	FActiveGameplayEffectHandle ProficiencyEffectHandle;

	/** 실험체 레벨 1~최대. 전원 복제 — HUD 가 상대 레벨을 그린다. */
	UPROPERTY(ReplicatedUsing = OnRep_Level)
	int32 Level = 1;

	/** 현재 레벨에서 쌓인 경험치 (레벨업 때 필요치를 빼고 남긴다). 소유자만. */
	UPROPERTY(Replicated)
	int32 Exp = 0;

private:
	static const class UDataTable* GetTable();

	/** 장착 무기군. 없으면 None. */
	EERWeaponType GetEquippedWeaponType() const;

	/**
	 * [서버] 레벨 1회분 스탯 성장 — 폰의 UERCharacterData.Growth 를 UERLevelUpEffect(Instant) 로 ASC 에 적용 (F10-02).
	 * ⚠ 폰이 없으면(사망 중) 적용하지 못한다 — Warning. 부활 시 재적용은 F14 가 정한다.
	 */
	void ApplyLevelGrowth(int32 NewLevel);
};
