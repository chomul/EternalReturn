// Copyright Epic Games, Inc. All Rights Reserved.
//
// ⚠⚠ **임시 파일이다. 무기 UI(F17)가 생기면 통째로 삭제한다.**

#include "Engine/World.h"
#include "EternalReturn.h"
#include "GAS/ERSkillData.h"
#include "Weapon/ERWeaponLibrary.h"
#include "Weapon/ERWeaponTypes.h"

namespace
{

bool ParseWeaponType(const FString& Name, EERWeaponType& Out)
{
	const UEnum* Enum = StaticEnum<EERWeaponType>();
	const int64 Value = Enum ? Enum->GetValueByNameString(Name) : INDEX_NONE;
	if (Value == INDEX_NONE)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[무기디버그] '%s' 는 EERWeaponType 이 아니다 (Hammer · Axe · Dagger · Shuriken · SniperRifle · Pistol · Bat · Throw …)"), *Name);
		return false;
	}
	Out = static_cast<EERWeaponType>(Value);
	return true;
}

// ER.Weapon.Show <Type>
void WeaponShowCmd(const TArray<FString>& Args, UWorld* World)
{
	EERWeaponType Type;
	if (Args.Num() < 1 || !ParseWeaponType(Args[0], Type))
	{
		return;
	}
	const FERWeaponClassRow* Row = ERWeapon::Find(Type);   // 없으면 Error 로그
	if (!Row)
	{
		return;
	}
	FString Upgrades;
	for (const int32 L : Row->UpgradeLevels) { Upgrades += FString::Printf(TEXT("%d "), L); }
	UE_LOG(LogEternalReturn, Warning, TEXT("[무기디버그] %s | 사거리 %.1fm | D %s (Lv.%d~%d) | 평타 %s | 해금 %d · 강화 %s"),
		*Args[0], Row->AttackRange,
		Row->DSkill ? *GetNameSafe(Row->DSkill) : TEXT("(없음)"), Row->DSkill ? Row->DSkill->InitialLevel : 0, Row->DSkill ? Row->DSkill->MaxLevel : 0,
		Row->AttackData ? *GetNameSafe(Row->AttackData) : TEXT("(없음 — 실험체 평타 유지)"),
		Row->UnlockLevel, *Upgrades);
}

// ER.Weapon.Validate
void WeaponValidateCmd(const TArray<FString>& Args, UWorld* World)
{
	ERWeapon::ValidateTable();
}

} // namespace

static FAutoConsoleCommandWithWorldAndArgs GERWeaponShowCmd(
	TEXT("ER.Weapon.Show"), TEXT("[임시] 무기 계열 행. ER.Weapon.Show <Hammer|Axe|…>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WeaponShowCmd));
static FAutoConsoleCommandWithWorldAndArgs GERWeaponValidateCmd(
	TEXT("ER.Weapon.Validate"), TEXT("[임시] WeaponClassTable 전 행 검사"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WeaponValidateCmd));
