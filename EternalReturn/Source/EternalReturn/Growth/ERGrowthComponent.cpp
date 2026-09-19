// Copyright Epic Games, Inc. All Rights Reserved.

#include "Growth/ERGrowthComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/ERCharacterBase.h"
#include "Character/ERCharacterData.h"
#include "Engine/DataTable.h"
#include "EternalReturn.h"
#include "Growth/ERGrowthSettings.h"
#include "Growth/ERLevelUpEffect.h"
#include "Growth/ERProficiencyEffect.h"
#include "Item/ERInventoryComponent.h"
#include "Item/ERItemData.h"
#include "Core/ERPlayerState.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

UERGrowthComponent::UERGrowthComponent()
{
	SetIsReplicatedByDefault(true);
}

void UERGrowthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UERGrowthComponent, Level);
	DOREPLIFETIME_CONDITION(UERGrowthComponent, Exp, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UERGrowthComponent, WeaponProficiencies, COND_OwnerOnly);
}

// ─────────────────────────────────────────────────────────────
// 테이블
// ─────────────────────────────────────────────────────────────

const UDataTable* UERGrowthComponent::GetTable()
{
	const UERGrowthSettings& Settings = UERGrowthSettings::Get();
	if (Settings.LevelExpTable.IsNull())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[성장] Project Settings > Game > ER Growth 에 LevelExpTable 이 비어 있다."));
		return nullptr;
	}
	const UDataTable* Table = Settings.LevelExpTable.LoadSynchronous();
	if (!Table)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[성장] LevelExpTable 을 로드하지 못했다: %s"), *Settings.LevelExpTable.ToString());
		return nullptr;
	}
	if (Table->GetRowStruct() != FERLevelExpRow::StaticStruct())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[성장] %s 의 행 구조가 FERLevelExpRow 가 아니다 (%s)."),
			*Table->GetName(), *GetNameSafe(Table->GetRowStruct()));
		return nullptr;
	}
	return Table;
}

int32 UERGrowthComponent::GetMaxLevel()
{
	const UDataTable* Table = GetTable();
	return Table ? Table->GetRowMap().Num() + 1 : 1;
}

const FERLevelExpRow* UERGrowthComponent::FindLevelRow(int32 InLevel)
{
	const UDataTable* Table = GetTable();
	if (!Table)
	{
		return nullptr;
	}
	const FName RowName(*FString::Printf(TEXT("Lv%d"), InLevel));
	const FERLevelExpRow* Row = Table->FindRow<FERLevelExpRow>(RowName, TEXT("ERGrowth"), /*bWarnIfRowMissing=*/false);
	if (!Row)
	{
		// ⚠ 행이 빠졌으면 그 레벨에서 멈춘다. 조용히 넘어가지 않는다 (CLAUDE.md §8).
		UE_LOG(LogEternalReturn, Error, TEXT("[성장] LevelExpTable 에 %s 행이 없다."), *RowName.ToString());
	}
	return Row;
}

// ─────────────────────────────────────────────────────────────
// 경험치 · 레벨
// ─────────────────────────────────────────────────────────────

void UERGrowthComponent::AddExp(int32 Amount, EERExpSource Source)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0)
	{
		return;
	}

	const int32 MaxLevel = GetMaxLevel();
	if (Level >= MaxLevel)
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[성장] %s 최대 레벨(%d) — 경험치 +%d 무시"), *GetOwner()->GetName(), MaxLevel, Amount);
		return;
	}

	Exp += Amount;
	UE_LOG(LogEternalReturn, Log, TEXT("[성장] %s 경험치 +%d (%s) -> %d (Lv.%d)"),
		*GetOwner()->GetName(), Amount, *UEnum::GetDisplayValueAsText(Source).ToString(), Exp, Level);

	// ⭐ 한 번에 여러 레벨. 레벨마다 훅을 순서대로 — 02 · 03 이 레벨별 처리를 한다.
	while (Level < MaxLevel)
	{
		const FERLevelExpRow* Row = FindLevelRow(Level);
		if (!Row || Exp < Row->RequiredExp)
		{
			break;
		}
		Exp -= Row->RequiredExp;
		++Level;
		UE_LOG(LogEternalReturn, Log, TEXT("[성장] %s 레벨업 -> Lv.%d (남은 경험치 %d)"), *GetOwner()->GetName(), Level, Exp);
		ApplyLevelGrowth(Level);
		OnLevelUp.Broadcast(Level);
	}

	// 최대 레벨 도달 — 남은 경험치는 버린다 `[자체]`. 게이지가 "가득" 이 아니라 0 으로 보이게.
	if (Level >= MaxLevel)
	{
		Exp = 0;
	}
}

void UERGrowthComponent::ApplyLevelGrowth(int32 NewLevel)
{
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	const AERCharacterBase* Character = PS ? Cast<AERCharacterBase>(PS->GetPawn()) : nullptr;
	const UERCharacterData* Data = Character ? Character->GetCharacterData() : nullptr;
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (!Data || !ASC)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[성장] %s Lv.%d 스탯 성장 미적용 — 폰(%s) · 실험체 데이터(%s) · ASC(%s)"),
			*GetOwner()->GetName(), NewLevel, *GetNameSafe(Character), *GetNameSafe(Data), *GetNameSafe(ASC));
		return;
	}

	const FERCharStatGrowth& G = Data->Growth;
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UERLevelUpEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_MaxHP, G.MaxHPPerLevel);
	Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_HP, G.MaxHPPerLevel);      // 현재 체력도 증가분만큼 `[자체]`
	Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_HPRegen, G.HPRegenPerLevel);
	Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_AttackPower, G.AttackPowerPerLevel);
	Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_Defense, G.DefensePerLevel);
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);

	UE_LOG(LogEternalReturn, Log, TEXT("[성장] %s Lv.%d 스탯 +(체력 %.1f · 공격력 %.2f · 방어력 %.2f · 재생 %.3f) -> 기본 체력 %.1f · 공격력 %.2f · 방어력 %.2f"),
		*GetOwner()->GetName(), NewLevel, G.MaxHPPerLevel, G.AttackPowerPerLevel, G.DefensePerLevel, G.HPRegenPerLevel,
		ASC->GetNumericAttributeBase(UERAttributeSet::GetMaxHPAttribute()),
		ASC->GetNumericAttributeBase(UERAttributeSet::GetAttackPowerAttribute()),
		ASC->GetNumericAttributeBase(UERAttributeSet::GetDefenseAttribute()));
}

// ─────────────────────────────────────────────────────────────
// 무기 숙련도 (F10-04)
// ─────────────────────────────────────────────────────────────

EERWeaponType UERGrowthComponent::GetEquippedWeaponType() const
{
	const AERPlayerState* PS = Cast<AERPlayerState>(GetOwner());
	const UERInventoryComponent* Inventory = PS ? PS->GetInventory() : nullptr;
	const FName ItemId = Inventory ? Inventory->GetEquippedItem(EEREquipSlot::Weapon) : NAME_None;
	if (ItemId.IsNone())
	{
		return EERWeaponType::None;
	}
	const FERItemRow* Item = ERItem::Find(ItemId);
	return Item ? Item->WeaponType : EERWeaponType::None;
}

int32 UERGrowthComponent::GetWeaponProficiencyLevel(EERWeaponType WeaponType) const
{
	const FERWeaponProficiency* Found = WeaponProficiencies.FindByPredicate([WeaponType](const FERWeaponProficiency& P) { return P.WeaponType == WeaponType; });
	return Found ? Found->Level : 1;
}

float UERGrowthComponent::ProficiencyRequiredExp(int32 InLevel)
{
	const UERGrowthSettings& S = UERGrowthSettings::Get();
	return static_cast<float>(S.ProficiencyExpLv2 + S.ProficiencyExpStep * (InLevel - 1));
}

void UERGrowthComponent::AddWeaponProficiencyExp(EERWeaponType WeaponType, float Amount, const TCHAR* Reason)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || WeaponType == EERWeaponType::None || Amount <= 0.f)
	{
		return;
	}

	FERWeaponProficiency* P = WeaponProficiencies.FindByPredicate([WeaponType](const FERWeaponProficiency& E) { return E.WeaponType == WeaponType; });
	if (!P)
	{
		P = &WeaponProficiencies.AddDefaulted_GetRef();
		P->WeaponType = WeaponType;
	}

	const int32 MaxLevel = UERGrowthSettings::Get().ProficiencyMaxLevel;
	if (P->Level >= MaxLevel)
	{
		return;
	}

	P->Exp += Amount;
	UE_LOG(LogEternalReturn, Log, TEXT("[숙련도] %s %s +%.1f (%s) -> %.1f / %.0f (Lv.%d)"),
		*GetOwner()->GetName(), *UEnum::GetDisplayValueAsText(WeaponType).ToString(), Amount, Reason,
		P->Exp, ProficiencyRequiredExp(P->Level), P->Level);

	bool bLeveled = false;
	while (P->Level < MaxLevel && P->Exp >= ProficiencyRequiredExp(P->Level))
	{
		P->Exp -= ProficiencyRequiredExp(P->Level);
		++P->Level;
		bLeveled = true;
		UE_LOG(LogEternalReturn, Log, TEXT("[숙련도] %s %s 레벨업 -> Lv.%d (남은 %.1f)"),
			*GetOwner()->GetName(), *UEnum::GetDisplayValueAsText(WeaponType).ToString(), P->Level, P->Exp);
		OnWeaponProficiencyLevelUp.Broadcast(WeaponType, P->Level);
	}
	if (P->Level >= MaxLevel)
	{
		P->Exp = 0.f;
	}

	// 지금 든 무기군이 올랐을 때만 증폭이 바뀐다.
	if (bLeveled && WeaponType == GetEquippedWeaponType())
	{
		RefreshProficiencyBonus();
	}
}

void UERGrowthComponent::AddEquippedWeaponProficiencyExp(float Amount, const TCHAR* Reason)
{
	AddWeaponProficiencyExp(GetEquippedWeaponType(), Amount, Reason);
}

void UERGrowthComponent::OnDamageDealt(AActor* Target, float Damage)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Damage <= 0.f)
	{
		return;
	}
	// 대상 종류는 태그 — F03-05 와 같은 방식. F12 가 없어도 동작한다.
	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	const bool bWildlife = TargetASC && TargetASC->HasMatchingGameplayTag(ERTags::Actor_Type_Wildlife);
	const UERGrowthSettings& S = UERGrowthSettings::Get();
	const float Per100 = bWildlife ? S.ProficiencyExpPerWildlifeDamage100 : S.ProficiencyExpPerPlayerDamage100;
	AddEquippedWeaponProficiencyExp(Damage * Per100 / 100.f, bWildlife ? TEXT("야생동물 피해") : TEXT("실험체 피해"));
}

void UERGrowthComponent::OnItemCrafted(FName ResultId, bool bFirstTime)
{
	const FERItemRow* Item = ERItem::Find(ResultId);
	if (!Item || Item->Slot != EEREquipSlot::Weapon || Item->WeaponType == EERWeaponType::None)
	{
		return;   // 방어구 · 재료 제작은 무기 숙련도 대상이 아니다 (제작 숙련도는 초기 버전 삭제 — 역기획서 §9)
	}
	const UERGrowthSettings& S = UERGrowthSettings::Get();
	const int32 GradeIndex = static_cast<int32>(Item->Grade);
	if (!S.CraftProficiencyExpByGrade.IsValidIndex(GradeIndex))
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[숙련도] CraftProficiencyExpByGrade 에 등급 %d 항목이 없다 — 제작 경험치 0."), GradeIndex);
		return;
	}
	const float Amount = S.CraftProficiencyExpByGrade[GradeIndex] * (bFirstTime ? 1.f + S.FirstCraftBonus : 1.f);
	AddWeaponProficiencyExp(Item->WeaponType, Amount, bFirstTime ? TEXT("제작 · 최초") : TEXT("제작"));
}

void UERGrowthComponent::RefreshProficiencyBonus()
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (!GetOwner() || !GetOwner()->HasAuthority() || !ASC)
	{
		return;
	}

	// ⭐ 제거 후 재적용 — 수동 차감 없음 (장비 GE 와 같은 이유).
	if (ProficiencyEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(ProficiencyEffectHandle);
		ProficiencyEffectHandle.Invalidate();
		UE_LOG(LogEternalReturn, Log, TEXT("[숙련도] %s 증폭 제거"), *GetOwner()->GetName());
	}

	const EERWeaponType WeaponType = GetEquippedWeaponType();
	if (WeaponType == EERWeaponType::None)
	{
		return;
	}

	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	const AERCharacterBase* Character = PS ? Cast<AERCharacterBase>(PS->GetPawn()) : nullptr;
	const UERCharacterData* Data = Character ? Character->GetCharacterData() : nullptr;
	const FERWeaponAmp* Amp = Data ? Data->WeaponProficiencyAmp.Find(WeaponType) : nullptr;
	if (!Amp)
	{
		// 들 수 있는데 계수가 없다 — 조용히 0 이 되지 않게 드러낸다 (CLAUDE.md §8).
		UE_LOG(LogEternalReturn, Warning, TEXT("[숙련도] %s 의 WeaponProficiencyAmp 에 %s 가 없다 — 증폭 0."),
			*GetNameSafe(Data), *UEnum::GetDisplayValueAsText(WeaponType).ToString());
		return;
	}

	const int32 ProfLevel = GetWeaponProficiencyLevel(WeaponType);
	const float Magnitude = Amp->AmpPerLevel * ProfLevel / 100.f;   // % → 비율 (ERDamageExecution 이 1 + Amp 로 쓴다). Lv.1 부터 1단 `[자체]`

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UERProficiencyEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_SkillAmp,    Amp->AmpType == EERAmpType::Skill       ? Magnitude : 0.f);
	Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_BasicAtkAmp, Amp->AmpType == EERAmpType::BasicAttack ? Magnitude : 0.f);
	ProficiencyEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);

	UE_LOG(LogEternalReturn, Log, TEXT("[숙련도] %s %s Lv.%d -> %s +%.1f%% 적용"),
		*GetOwner()->GetName(), *UEnum::GetDisplayValueAsText(WeaponType).ToString(), ProfLevel,
		Amp->AmpType == EERAmpType::Skill ? TEXT("스킬 증폭") : TEXT("기본 공격 증폭"), Magnitude * 100.f);
}

void UERGrowthComponent::OnRep_Level()
{
	// F17 HUD 훅 자리. 지금은 로그만.
	UE_LOG(LogEternalReturn, Log, TEXT("[성장][클라] %s Lv.%d"), *GetNameSafe(GetOwner()), Level);
}
