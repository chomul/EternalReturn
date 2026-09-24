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
#include "TimerManager.h"

FString FERProficiencyKey::ToString() const
{
	return Track == EERProficiencyTrack::Weapon
		? UEnum::GetDisplayValueAsText(WeaponType).ToString()
		: UEnum::GetDisplayValueAsText(Track).ToString();
}

UERGrowthComponent::UERGrowthComponent()
{
	SetIsReplicatedByDefault(true);
}

void UERGrowthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UERGrowthComponent, Level);
	DOREPLIFETIME_CONDITION(UERGrowthComponent, Exp, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UERGrowthComponent, Proficiencies, COND_OwnerOnly);
}

void UERGrowthComponent::BeginPlay()
{
	Super::BeginPlay();
	// 이동 트랙 — 서버가 1초마다 폰 위치 차이를 잰다 (Tick 아님 · CLAUDE.md §2). 클라 이동은 ServerMove 로 서버 폰에 반영된다.
	if (GetOwner() && GetOwner()->HasAuthority() && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(MoveSampleTimer, this, &UERGrowthComponent::SampleMovement, 1.f, true);
	}
}

void UERGrowthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(MoveSampleTimer);
	}
	Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────────
// 테이블
// ─────────────────────────────────────────────────────────────

namespace
{
	const UDataTable* LoadTable(const TSoftObjectPtr<UDataTable>& Ptr, const UScriptStruct* RowStruct, const TCHAR* SettingName)
	{
		if (Ptr.IsNull())
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[성장] Project Settings > Game > ER Growth 에 %s 가 비어 있다."), SettingName);
			return nullptr;
		}
		const UDataTable* Table = Ptr.LoadSynchronous();
		if (!Table)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[성장] %s 를 로드하지 못했다: %s"), SettingName, *Ptr.ToString());
			return nullptr;
		}
		if (Table->GetRowStruct() != RowStruct)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[성장] %s 의 행 구조가 %s 가 아니다 (%s)."), *Table->GetName(), *RowStruct->GetName(), *GetNameSafe(Table->GetRowStruct()));
			return nullptr;
		}
		return Table;
	}
}

const UDataTable* UERGrowthComponent::GetLevelTable()
{
	return LoadTable(UERGrowthSettings::Get().LevelExpTable, FERLevelExpRow::StaticStruct(), TEXT("LevelExpTable"));
}

const UDataTable* UERGrowthComponent::GetProficiencyTable()
{
	return LoadTable(UERGrowthSettings::Get().ProficiencyExpTable, FERProficiencyExpRow::StaticStruct(), TEXT("ProficiencyExpTable"));
}

int32 UERGrowthComponent::GetMaxLevel()
{
	const UDataTable* Table = GetLevelTable();
	return Table ? Table->GetRowMap().Num() + 1 : 1;
}

int32 UERGrowthComponent::GetProficiencyMaxLevel()
{
	const UDataTable* Table = GetProficiencyTable();
	return Table ? Table->GetRowMap().Num() + 1 : 1;
}

const FERLevelExpRow* UERGrowthComponent::FindLevelRow(int32 InLevel)
{
	const UDataTable* Table = GetLevelTable();
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

float UERGrowthComponent::ProficiencyRequiredExp(EERProficiencyTrack Track, int32 InLevel)
{
	const UDataTable* Table = GetProficiencyTable();
	if (!Table)
	{
		return 0.f;
	}
	const FName RowName(*FString::Printf(TEXT("Lv%d"), InLevel));
	const FERProficiencyExpRow* Row = Table->FindRow<FERProficiencyExpRow>(RowName, TEXT("ERGrowth"), /*bWarnIfRowMissing=*/false);
	if (!Row)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[숙련도] ProficiencyExpTable 에 %s 행이 없다."), *RowName.ToString());
		return 0.f;
	}
	return static_cast<float>(Row->Get(Track));
}

// ─────────────────────────────────────────────────────────────
// ⭐ 유일한 입구 — 숙련도 적립 → 실험체 경험치
// ─────────────────────────────────────────────────────────────

int32 UERGrowthComponent::GetProficiencyLevel(const FERProficiencyKey& Key) const
{
	const FERProficiency* Found = Proficiencies.FindByPredicate([&Key](const FERProficiency& P) { return P.Key == Key; });
	return Found ? Found->Level : 1;
}

void UERGrowthComponent::AddProficiencyExp(const FERProficiencyKey& Key, float Amount, const TCHAR* Reason)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Key.IsValid() || Amount <= 0.f)
	{
		return;
	}

	FERProficiency* P = Proficiencies.FindByPredicate([&Key](const FERProficiency& E) { return E.Key == Key; });
	if (!P)
	{
		P = &Proficiencies.AddDefaulted_GetRef();
		P->Key = Key;
	}

	const int32 MaxLevel = GetProficiencyMaxLevel();
	const FString KeyName = Key.ToString();
	if (P->Level < MaxLevel)
	{
		P->Exp += Amount;
		UE_LOG(LogEternalReturn, Log, TEXT("[숙련도] %s %s +%.1f (%s) -> %.1f / %.0f (Lv.%d)"),
			*GetOwner()->GetName(), *KeyName, Amount, Reason, P->Exp, ProficiencyRequiredExp(Key.Track, P->Level), P->Level);

		bool bLeveled = false;
		while (P->Level < MaxLevel)
		{
			const float Need = ProficiencyRequiredExp(Key.Track, P->Level);
			if (Need <= 0.f || P->Exp < Need)
			{
				break;
			}
			P->Exp -= Need;
			++P->Level;
			bLeveled = true;
			UE_LOG(LogEternalReturn, Log, TEXT("[숙련도] %s %s 레벨업 -> Lv.%d (남은 %.1f)"), *GetOwner()->GetName(), *KeyName, P->Level, P->Exp);
			OnProficiencyLevelUp.Broadcast(Key, P->Level);
		}
		if (P->Level >= MaxLevel)
		{
			P->Exp = 0.f;
		}
		// 지금 든 무기군이 올랐을 때만 공속 · 증폭이 바뀐다.
		if (bLeveled && Key.Track == EERProficiencyTrack::Weapon && Key.WeaponType == GetEquippedWeaponType())
		{
			RefreshProficiencyBonus();
		}
	}
	else
	{
		UE_LOG(LogEternalReturn, Verbose, TEXT("[숙련도] %s %s 최대 레벨 — +%.1f (%s) 는 실험체 경험치로만"), *GetOwner()->GetName(), *KeyName, Amount, Reason);
	}

	// ⭐ 실험체 경험치 = 숙련도 경험치의 합 (트랙이 최대 레벨이어도 실험체 경험치는 들어간다 `[자체]`).
	AddCharacterExp(Amount * UERGrowthSettings::Get().LevelExpPerProficiencyExp, Reason);
}

void UERGrowthComponent::AddCharacterExp(float Amount, const TCHAR* Reason)
{
	if (Amount <= 0.f)
	{
		return;
	}
	const int32 MaxLevel = GetMaxLevel();
	if (Level >= MaxLevel)
	{
		UE_LOG(LogEternalReturn, Verbose, TEXT("[성장] %s 최대 레벨(%d) — 경험치 +%.1f 무시"), *GetOwner()->GetName(), MaxLevel, Amount);
		return;
	}

	Exp += Amount;
	UE_LOG(LogEternalReturn, Log, TEXT("[성장] %s 경험치 +%.1f (%s) -> %.1f (Lv.%d)"), *GetOwner()->GetName(), Amount, Reason, Exp, Level);

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
		UE_LOG(LogEternalReturn, Log, TEXT("[성장] %s 레벨업 -> Lv.%d (남은 경험치 %.1f)"), *GetOwner()->GetName(), Level, Exp);
		ApplyLevelGrowth(Level);
		OnLevelUp.Broadcast(Level);
	}

	// 최대 레벨 도달 — 남은 경험치는 버린다 `[자체]`. 게이지가 "가득" 이 아니라 0 으로 보이게.
	if (Level >= MaxLevel)
	{
		Exp = 0.f;
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
// 적립 훅
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

void UERGrowthComponent::AddEquippedWeaponProficiencyExp(float Amount, const TCHAR* Reason)
{
	const EERWeaponType Type = GetEquippedWeaponType();
	if (Type == EERWeaponType::None)
	{
		UE_LOG(LogEternalReturn, Verbose, TEXT("[숙련도] %s 무기가 없어 무기 숙련도 +%.1f (%s) 는 버린다"), *GetOwner()->GetName(), Amount, Reason);
		return;
	}
	AddProficiencyExp(FERProficiencyKey::Weapon(Type), Amount, Reason);
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
	const float Per100 = bWildlife ? S.WeaponExpPerWildlifeDamage100 : S.WeaponExpPerPlayerDamage100;
	AddEquippedWeaponProficiencyExp(Damage * Per100 / 100.f, bWildlife ? TEXT("야생동물 피해") : TEXT("실험체 피해"));
}

void UERGrowthComponent::OnDamageTaken(float Damage)
{
	if (Damage <= 0.f)
	{
		return;
	}
	AddProficiencyExp(FERProficiencyKey::Of(EERProficiencyTrack::Defense), Damage * UERGrowthSettings::Get().DefenseExpPerDamageTaken100 / 100.f, TEXT("받은 피해"));
}

void UERGrowthComponent::OnPlayerKilled(int32 VictimLevel)
{
	const UERGrowthSettings& S = UERGrowthSettings::Get();
	AddEquippedWeaponProficiencyExp(S.WeaponExpPerPlayerKillBase + S.WeaponExpPerPlayerKillPerLevel * VictimLevel,
		*FString::Printf(TEXT("실험체 처치 Lv.%d"), VictimLevel));
}

void UERGrowthComponent::OnWildlifeKilled(int32 WildlifeLevel, float HuntExp)
{
	// ⭐ 금액은 **종 정의가 정한다** — 종별 기본값 + 레벨당 (역기획서 §1.2 [확인] 2026-09-23). 여기서 배율을 곱하지 않는다.
	AddProficiencyExp(FERProficiencyKey::Of(EERProficiencyTrack::Hunt), FMath::Max(0.f, HuntExp),
		*FString::Printf(TEXT("야생동물 처치 Lv.%d"), WildlifeLevel));
}

void UERGrowthComponent::OnItemCrafted(FName ResultId, bool bFirstTime)
{
	const FERItemRow* Item = ERItem::Find(ResultId);
	if (!Item)
	{
		return;
	}
	const UERGrowthSettings& S = UERGrowthSettings::Get();
	const int32 GradeIndex = static_cast<int32>(Item->Grade);
	const float Bonus = bFirstTime ? 1.f + S.FirstCraftBonus : 1.f;
	const TCHAR* Reason = bFirstTime ? TEXT("제작 · 최초") : TEXT("제작");

	// 제작 트랙 — 모든 제작.
	if (S.CraftExpByGrade.IsValidIndex(GradeIndex))
	{
		AddProficiencyExp(FERProficiencyKey::Of(EERProficiencyTrack::Craft), S.CraftExpByGrade[GradeIndex] * Bonus, Reason);
	}
	else
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[숙련도] CraftExpByGrade 에 등급 %d 항목이 없다 — 제작 경험치 0."), GradeIndex);
	}

	// 무기 트랙 — 무기를 만들면 그 무기군에도 (원작: 무기 제작 100~600).
	if (Item->Slot == EEREquipSlot::Weapon && Item->WeaponType != EERWeaponType::None)
	{
		if (S.WeaponCraftExpByGrade.IsValidIndex(GradeIndex))
		{
			AddProficiencyExp(FERProficiencyKey::Weapon(Item->WeaponType), S.WeaponCraftExpByGrade[GradeIndex] * Bonus, Reason);
		}
		else
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[숙련도] WeaponCraftExpByGrade 에 등급 %d 항목이 없다 — 무기 제작 경험치 0."), GradeIndex);
		}
	}
}

void UERGrowthComponent::OnBoxOpened(FName LootRow)
{
	AddProficiencyExp(FERProficiencyKey::Of(EERProficiencyTrack::Search), UERGrowthSettings::Get().SearchExpPerBox, *FString::Printf(TEXT("상자 %s"), *LootRow.ToString()));
}

void UERGrowthComponent::SampleMovement()
{
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
	if (!Pawn)
	{
		bHasLastSample = false;   // 사망 ~ 부활 사이 — 다음 표본부터 다시
		return;
	}
	const FVector Now = Pawn->GetActorLocation();
	if (bHasLastSample)
	{
		const UERGrowthSettings& S = UERGrowthSettings::Get();
		const float Meters = FVector::Dist2D(LastSampledLocation, Now) / 100.f;
		if (Meters > 0.05f && Meters <= S.MoveSampleMaxMeters)   // 순간이동 · 부활 · 넉백 폭주는 무시
		{
			MoveAccumMeters += Meters;
			if (MoveAccumMeters >= 100.f)   // 100m 단위로 적립 — 로그가 초마다 찍히지 않게
			{
				const float Chunks = FMath::FloorToFloat(MoveAccumMeters / 100.f);
				MoveAccumMeters -= Chunks * 100.f;
				AddProficiencyExp(FERProficiencyKey::Of(EERProficiencyTrack::Move), Chunks * S.MoveExpPer100m, TEXT("이동 100m"));
			}
		}
	}
	LastSampledLocation = Now;
	bHasLastSample = true;
}

// ─────────────────────────────────────────────────────────────
// 무기 숙련도 효과 — 공속 + 증폭 (F10-04 · E18 A9)
// ─────────────────────────────────────────────────────────────

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
	const float AmpMag = Amp->AmpPerLevel * ProfLevel / 100.f;            // % → 비율 (ERDamageExecution 이 1 + Amp 로 쓴다). Lv.1 부터 1단 `[자체]`
	const float SpeedMag = Amp->AttackSpeedPerLevel * ProfLevel / 100.f;  // 공속 어트리뷰트에 Additive (기본 1.0 → +0.02 × Lv)

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UERProficiencyEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_SkillAmp,    Amp->AmpType == EERAmpType::Skill       ? AmpMag : 0.f);
	Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_BasicAtkAmp, Amp->AmpType == EERAmpType::BasicAttack ? AmpMag : 0.f);
	Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_AttackSpeed, SpeedMag);
	ProficiencyEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);

	UE_LOG(LogEternalReturn, Log, TEXT("[숙련도] %s %s Lv.%d -> %s +%.1f%% · 공격 속도 +%.1f%% 적용"),
		*GetOwner()->GetName(), *UEnum::GetDisplayValueAsText(WeaponType).ToString(), ProfLevel,
		Amp->AmpType == EERAmpType::Skill ? TEXT("스킬 증폭") : TEXT("기본 공격 증폭"), AmpMag * 100.f, SpeedMag * 100.f);
}

void UERGrowthComponent::OnRep_Level()
{
	// F17 HUD 훅 자리. 지금은 로그만.
	UE_LOG(LogEternalReturn, Log, TEXT("[성장][클라] %s Lv.%d"), *GetNameSafe(GetOwner()), Level);
}
