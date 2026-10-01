// Copyright Epic Games, Inc. All Rights Reserved.
//
// [에디터] F19 — Docs/3_EditorTasks/Data/DT_Character.csv 의 행을 이미 있는 DA_Char_<이름> 에 덮어쓴다 (사용자 2026-10-01 "6캐릭터 내용 다 넣어주는 거").
//   CSV 가 원본 (memory: csv-is-data-source). 사용자가 에디터에서 `ER.Char.ImportCSV` → Save All.
//   CSV 가 가진 필드만 쓴다: 기초 스탯 · 레벨당 · 무기 목록 · 무기 숙련 증폭. **공격 속도 · 스킬 · 연출 · 스킨은 건드리지 않는다**
//   (공격 속도: 원작 표의 0.13 등은 "캐릭터 몫" 이고 우리 칸은 초당 공격 횟수 — 합치는 방식 (미확인) · EditorTasks F19 K1).

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "Character/ERCharacterData.h"
#include "EternalReturn.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	UERCharacterData* FindCharByName(const FString& AssetName)
	{
		TArray<FAssetData> Assets;
		FAssetRegistryModule::GetRegistry().GetAssetsByClass(UERCharacterData::StaticClass()->GetClassPathName(), Assets);
		const FAssetData* Hit = Assets.FindByPredicate([&AssetName](const FAssetData& A) { return A.AssetName.ToString() == AssetName; });
		return Hit ? Cast<UERCharacterData>(Hit->GetAsset()) : nullptr;
	}

	/** "SniperRifle:BasicAttack:2.0:3.6|Dagger:BasicAttack:2.4:0" → 무기 목록 · 증폭. 틀린 칸은 로그 남기고 건너뛴다. */
	bool ParseWeapons(const FString& Cell, const FString& Who, TArray<EERWeaponType>& OutTypes, TMap<EERWeaponType, FERWeaponAmp>& OutAmp)
	{
		const UEnum* WeaponEnum = StaticEnum<EERWeaponType>();
		const UEnum* AmpEnum = StaticEnum<EERAmpType>();
		TArray<FString> Entries;
		Cell.ParseIntoArray(Entries, TEXT("|"));
		bool bOk = true;
		for (const FString& E : Entries)
		{
			TArray<FString> P;
			E.TrimStartAndEnd().ParseIntoArray(P, TEXT(":"));
			const int64 W = P.Num() > 0 ? WeaponEnum->GetValueByNameString(P[0].TrimStartAndEnd()) : INDEX_NONE;
			const int64 A = P.Num() > 1 ? AmpEnum->GetValueByNameString(P[1].TrimStartAndEnd()) : INDEX_NONE;
			if (P.Num() != 4 || W == INDEX_NONE || A == INDEX_NONE)
			{
				UE_LOG(LogEternalReturn, Error, TEXT("[캐릭터 임포트] %s 무기 칸 '%s' — 형식은 무기:증폭종류:레벨당증폭:레벨당공속 (예 SniperRifle:BasicAttack:2.0:3.6)"), *Who, *E);
				bOk = false;
				continue;
			}
			const EERWeaponType Type = static_cast<EERWeaponType>(W);
			OutTypes.AddUnique(Type);
			FERWeaponAmp& Amp = OutAmp.FindOrAdd(Type);
			Amp.AmpType = static_cast<EERAmpType>(A);
			Amp.AmpPerLevel = FCString::Atof(*P[2]);
			Amp.AttackSpeedPerLevel = FCString::Atof(*P[3]);
		}
		return bOk;
	}

	void ImportCharCsv(const TArray<FString>& Args)
	{
		FString Path = Args.IsEmpty() ? FString() : Args[0];
		if (Path.IsEmpty()) { Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Docs/3_EditorTasks/Data/DT_Character.csv")); }
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[캐릭터 임포트] CSV 를 못 읽었다: %s"), *Path);
			return;
		}
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		if (Lines.Num() < 2)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[캐릭터 임포트] 행이 없다."));
			return;
		}
		TArray<FString> Header;
		Lines[0].TrimStartAndEnd().ParseIntoArray(Header, TEXT(","), /*CullEmpty=*/false);
		auto Col = [&Header](const TCHAR* Name) { return Header.IndexOfByPredicate([Name](const FString& H) { return H.TrimStartAndEnd().Equals(Name, ESearchCase::IgnoreCase); }); };
		const int32 CHP = Col(TEXT("MaxHP")), CRegen = Col(TEXT("HPRegen")), CAtk = Col(TEXT("AttackPower")), CDef = Col(TEXT("Defense")), CMove = Col(TEXT("MoveSpeed")),
			CHPLv = Col(TEXT("MaxHPPerLevel")), CRegenLv = Col(TEXT("HPRegenPerLevel")), CAtkLv = Col(TEXT("AttackPowerPerLevel")), CDefLv = Col(TEXT("DefensePerLevel")),
			CWeapons = Col(TEXT("Weapons")), CVP = Col(TEXT("MaxVP")), CVPRegen = Col(TEXT("VPRegen"));
		if (CHP < 0 || CAtk < 0 || CDef < 0 || CWeapons < 0)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[캐릭터 임포트] 헤더에 MaxHP · AttackPower · Defense · Weapons 가 있어야 한다: %s"), *Lines[0]);
			return;
		}

		int32 Updated = 0, Missing = 0;
		for (int32 L = 1; L < Lines.Num(); ++L)
		{
			TArray<FString> F;
			Lines[L].TrimStartAndEnd().ParseIntoArray(F, TEXT(","), /*CullEmpty=*/false);   // 이 CSV 는 따옴표 칸이 없다 (무기 구분은 | · :)
			if (F.Num() < Header.Num() || F[0].TrimStartAndEnd().IsEmpty()) { continue; }
			const FString AssetName = TEXT("DA_Char_") + F[0].TrimStartAndEnd();
			UERCharacterData* D = FindCharByName(AssetName);
			if (!D)
			{
				UE_LOG(LogEternalReturn, Warning, TEXT("[캐릭터 임포트] %s — 애셋이 없다 (이 명령은 만들지 않는다 · 에디터에서 먼저 만든다)"), *AssetName);
				++Missing;
				continue;
			}
			auto Num = [&F](int32 C, float Fallback) { return C >= 0 && !F[C].TrimStartAndEnd().IsEmpty() ? FCString::Atof(*F[C]) : Fallback; };
			TArray<EERWeaponType> Types;
			TMap<EERWeaponType, FERWeaponAmp> Amp;
			if (!ParseWeapons(F[CWeapons], AssetName, Types, Amp))
			{
				UE_LOG(LogEternalReturn, Error, TEXT("[캐릭터 임포트] %s — 무기 칸이 틀려서 이 행은 건너뛴다"), *AssetName);
				continue;
			}
			D->Modify();
			D->BaseStats.MaxHP = Num(CHP, D->BaseStats.MaxHP);
			D->BaseStats.HPRegen = Num(CRegen, D->BaseStats.HPRegen);
			D->BaseStats.AttackPower = Num(CAtk, D->BaseStats.AttackPower);
			D->BaseStats.Defense = Num(CDef, D->BaseStats.Defense);
			D->BaseStats.MoveSpeed = Num(CMove, D->BaseStats.MoveSpeed);
			D->BaseStats.MaxVP = Num(CVP, D->BaseStats.MaxVP);          // VP 안 쓰는 실험체는 0 (사용자 2026-10-01) · 빈 칸이면 유지
			D->BaseStats.VPRegen = Num(CVPRegen, D->BaseStats.VPRegen);
			D->Growth.MaxHPPerLevel = Num(CHPLv, D->Growth.MaxHPPerLevel);
			D->Growth.HPRegenPerLevel = Num(CRegenLv, D->Growth.HPRegenPerLevel);
			D->Growth.AttackPowerPerLevel = Num(CAtkLv, D->Growth.AttackPowerPerLevel);
			D->Growth.DefensePerLevel = Num(CDefLv, D->Growth.DefensePerLevel);
			D->WeaponTypes = Types;
			D->WeaponProficiencyAmp = Amp;
			D->MarkPackageDirty();
			++Updated;
			FString WeaponText;
			for (const TPair<EERWeaponType, FERWeaponAmp>& P : Amp)
			{
				WeaponText += FString::Printf(TEXT(" %s(%s %.1f · 공속 %.1f)"), *StaticEnum<EERWeaponType>()->GetNameStringByValue(static_cast<int64>(P.Key)),
					P.Value.AmpType == EERAmpType::Skill ? TEXT("스증") : TEXT("평증"), P.Value.AmpPerLevel, P.Value.AttackSpeedPerLevel);
			}
			UE_LOG(LogEternalReturn, Log, TEXT("[캐릭터 임포트] %s — 체력 %.0f (+%.0f) · VP %.0f · 공격력 %.0f (+%.1f) · 방어력 %.0f (+%.1f) · 이동 %.2f · 공속 %.2f (유지) · 무기%s"),
				*AssetName, D->BaseStats.MaxHP, D->Growth.MaxHPPerLevel, D->BaseStats.MaxVP, D->BaseStats.AttackPower, D->Growth.AttackPowerPerLevel,
				D->BaseStats.Defense, D->Growth.DefensePerLevel, D->BaseStats.MoveSpeed, D->BaseStats.AttackSpeed, *WeaponText);
		}
		UE_LOG(LogEternalReturn, Log, TEXT("[캐릭터 임포트] 끝 — 덮어씀 %d · 애셋 없음 %d. **Save All (Ctrl+Shift+S)** 로 저장하라."), Updated, Missing);
	}

	FAutoConsoleCommand GImportCharCsvCmd(
		TEXT("ER.Char.ImportCSV"),
		TEXT("[에디터] DT_Character.csv → 이미 있는 DA_Char_* 에 스탯 · 레벨당 · 무기 · 숙련 증폭 덮어쓰기 (공격 속도 · 스킬 · 연출 유지). ER.Char.ImportCSV [경로]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ImportCharCsv));
}

#endif // WITH_EDITOR
