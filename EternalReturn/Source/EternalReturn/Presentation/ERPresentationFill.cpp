// Copyright Epic Games, Inc. All Rights Reserved.
//
// [에디터] F12.5-01 — ER.Pres.Fill <캐릭터|All|Wild|Weapon> [Force]
//   애니 파일명 규칙을 읽어 연출 DA 를 만든다 · 채운다 (Argument 39 ③ "손으로 수백 줄 넣지 않는다"). 사용자가 실행 · Save All.
//   ⚠ 기본은 이미 있는 줄 · 참조를 건드리지 않는다 (사람이 고쳤을 수 있다). Force 면 규칙이 찾은 줄만 덮는다 — 규칙 밖 줄은 그대로.
//
//   실험체  /Game/ER/Characters/<Char>/Animations/<Char>_<토큰>_<동작>
//     토큰 Common → 기본 표 · 무기 토큰(1Hand · Axe · Snipe …) → 무기 세트 · 토큰 사이 S0nn → 그 스킨의 덮어쓰기
//     동작 atk01 · atk02 → Attack / weaponskill → D / skill01~04 → Q~R / (Common) dance · death · collect
//     여러 단계(_start · _loop · _end) · 모드 토큰(Saw · bike · BareHands · Shadow · Scissors · Empty) → 규칙 밖 목록 (F19 에서 몽타주로)
//   야생동물  .../<종>_<01|Mutant|FogMutant>/Animations/*_atk01 · _atk02 · _death (폴더 위치 무관 — E30) + 소리 SFX/Monster (종 표)

#if WITH_EDITOR

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Character/ERCharacterData.h"
#include "Engine/SkeletalMesh.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayTags.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Presentation/ERPresentationData.h"
#include "Sound/SoundBase.h"
#include "UObject/Package.h"
#include "Wildlife/ERWildlifeData.h"

namespace
{
	const FString PresRoot = TEXT("/Game/ER/Presentation");

	struct FFillStats
	{
		int32 Created = 0;
		int32 Written = 0;
		int32 Kept = 0;
		TArray<FString> Unmatched;
	};

	IAssetRegistry& Registry()
	{
		return FAssetRegistryModule::GetRegistry();
	}

	/**
	 * 이름이 같은 T 를 **폴더와 무관하게** 찾는다. 없으면 Folder 에 새로 만든다.
	 * ⚠ 경로로만 찾으면 사용자가 옮긴 애셋을 못 찾고 빈 사본을 만든다 — 2026-09-27 재키 11개 사본 · 캐릭터 DA 가 사본으로 재연결돼 AnimClass 가 비어 보였다
	 *   (야생동물 ImportCSV 와 같은 사고 · ERWildlifeImport FindExistingByName).
	 */
	template <typename T>
	T* FindOrCreate(const FString& Folder, const FString& Name, FFillStats& Stats)
	{
		TArray<FAssetData> Same;
		Registry().GetAssetsByClass(T::StaticClass()->GetClassPathName(), Same);
		Same.RemoveAll([&Name](const FAssetData& A) { return A.AssetName.ToString() != Name; });
		if (Same.Num() > 1)
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[연출 채우기] %s 가 %d곳에 있다 — %s 를 쓴다. 중복을 지워라."), *Name, Same.Num(), *Same[0].PackageName.ToString());
		}
		if (!Same.IsEmpty())
		{
			return Cast<T>(Same[0].GetAsset());
		}
		const FString PackageName = Folder / Name;
		if (Registry().GetAssetByObjectPath(FSoftObjectPath(PackageName + TEXT(".") + Name)).IsValid())
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[연출 채우기] %s 에 다른 클래스 애셋이 있다 — 건너뛴다"), *PackageName);
			return nullptr;
		}
		UPackage* Package = CreatePackage(*PackageName);
		T* Obj = NewObject<T>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Obj);
		Package->MarkPackageDirty();
		++Stats.Created;
		return Obj;
	}

	/** 같은 (모드, 무기, 키) 줄이 있으면 Force 일 때만 덮는다. 반환: 썼으면 true. 모드 칸이 다른 줄(사람이 쓴 모드 줄)은 건드리지 않는다. */
	bool WriteEntry(TArray<FERPresentationEntry>& Entries, EERWeaponType Weapon, FGameplayTag Key, const TArray<UObject*>& Assets, bool bForce, FFillStats& Stats,
		FGameplayTag Mode = FGameplayTag())
	{
		FERPresentationEntry* Same = Entries.FindByPredicate([&](const FERPresentationEntry& E) { return E.Mode == Mode && E.Weapon == Weapon && E.Key == Key; });
		if (Same && !bForce)
		{
			++Stats.Kept;
			return false;
		}
		if (!Same)
		{
			Same = &Entries.AddDefaulted_GetRef();
			Same->Mode = Mode;
			Same->Weapon = Weapon;
			Same->Key = Key;
		}
		Same->Assets.Reset();
		for (UObject* A : Assets)
		{
			Same->Assets.Add(A);
		}
		++Stats.Written;
		return true;
	}

	TArray<FAssetData> FindAssets(const FString& Path, UClass* Class)
	{
		FARFilter Filter;
		Filter.PackagePaths.Add(FName(*Path));
		Filter.bRecursivePaths = true;
		Filter.ClassPaths.Add(Class->GetClassPathName());
		Filter.bRecursiveClasses = true;
		TArray<FAssetData> Out;
		Registry().GetAssets(Filter, Out);
		Out.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
		return Out;
	}

	/** 파일명 무기 토큰 → 계열. 이름이 enum 과 다른 것만 별칭 (재키 1Hand · 다니엘 OneHandSword = 단검 — 같은 무기, 다른 모션 · Argument 39 F1). */
	EERWeaponType TokenToWeapon(const FString& Token)
	{
		static const TMap<FString, EERWeaponType> Alias = {
			{ TEXT("1hand"), EERWeaponType::Dagger },
			{ TEXT("onehandsword"), EERWeaponType::Dagger },
			{ TEXT("2hand"), EERWeaponType::TwoHandSword },
			{ TEXT("dual"), EERWeaponType::DualSword },
			{ TEXT("snipe"), EERWeaponType::SniperRifle },
			{ TEXT("sniperrifle"), EERWeaponType::SniperRifle },
			{ TEXT("shriken"), EERWeaponType::Shuriken },
		};
		if (const EERWeaponType* W = Alias.Find(Token.ToLower()))
		{
			return *W;
		}
		const int64 V = StaticEnum<EERWeaponType>()->GetValueByNameString(Token);
		return V == INDEX_NONE ? EERWeaponType::None : static_cast<EERWeaponType>(V);
	}

	FGameplayTag ActionToKey(const FString& ActLower, bool bCommon)
	{
		if (ActLower == TEXT("atk01") || ActLower == TEXT("atk02")) { return ERTags::Ability_Slot_Attack; }
		if (ActLower == TEXT("weaponskill") || ActLower == TEXT("normalweaponskill")) { return ERTags::Ability_Slot_D; }
		if (ActLower == TEXT("skill01")) { return ERTags::Ability_Slot_Q; }
		if (ActLower == TEXT("skill02")) { return ERTags::Ability_Slot_W; }
		if (ActLower == TEXT("skill03")) { return ERTags::Ability_Slot_E; }
		if (ActLower == TEXT("skill04")) { return ERTags::Ability_Slot_R; }
		if (bCommon && ActLower == TEXT("dance")) { return ERTags::Pres_Anim_Dance; }
		if (bCommon && ActLower == TEXT("death")) { return ERTags::Pres_Anim_Death; }
		if (bCommon && ActLower == TEXT("collect")) { return ERTags::Pres_Anim_Gather; }   // 공용 채집 · 재료별(_wood …)은 나중 (Argument 46)
		return FGameplayTag();
	}

	/**
	 * 소리 이름 → (무기, 키) (F12.5-05 · Argument 49). 무기 기본(`SFX/Attack · Attack_Hit · Skill · Skill_Hit`)과 캐릭터 · 스킨(`Character_FX`) 이 같은 규칙.
	 *   attack<무기>… → Pres.Sfx.Attack · hit<무기>… → Hit · skill<무기>… → SkillCast · hitSkill<무기>… → SkillHit
	 *   <캐릭터>_<무기>_NormalAttack → Attack · <캐릭터>_<무기>_NormalAttack_Hit / _Normal_Hit → Hit
	 * `_in` · `_v` · `_wall` 변형 · 그 밖은 false (규칙 밖 — 뜻이 (미확인) 이거나 스킬 Q~R 소리 → F19).
	 */
	bool SoundToKey(const FString& Name, EERWeaponType& OutWeapon, FGameplayTag& OutKey)
	{
		const FString L = Name.ToLower();
		if (L.Contains(TEXT("_in")) || L.Contains(TEXT("_v")) || L.Contains(TEXT("_wall")))
		{
			return false;
		}
		struct FPrefix { const TCHAR* Prefix; FGameplayTag Key; };
		const FPrefix Prefixes[] = {   // hitskill 이 hit 보다 먼저
			{ TEXT("hitskill"), ERTags::Pres_Sfx_SkillHit }, { TEXT("attack"), ERTags::Pres_Sfx_Attack },
			{ TEXT("hit"), ERTags::Pres_Sfx_Hit }, { TEXT("skill"), ERTags::Pres_Sfx_SkillCast },
		};
		for (const FPrefix& P : Prefixes)
		{
			if (Name.StartsWith(P.Prefix, ESearchCase::IgnoreCase))
			{
				FString Token = Name.RightChop(FCString::Strlen(P.Prefix));
				Token.Split(TEXT("_"), &Token, nullptr);   // "Axe_r1" → "Axe" (없으면 그대로)
				OutWeapon = TokenToWeapon(Token);
				OutKey = P.Key;
				return OutWeapon != EERWeaponType::None;
			}
		}
		TArray<FString> T;
		Name.ParseIntoArray(T, TEXT("_"));
		if (T.Num() >= 3)
		{
			const FString Rest = FString::Join(TArray<FString>(T.GetData() + 2, T.Num() - 2), TEXT("_")).ToLower();
			const FGameplayTag Key = Rest == TEXT("normalattack") ? ERTags::Pres_Sfx_Attack
				: (Rest == TEXT("normalattack_hit") || Rest == TEXT("normal_hit")) ? ERTags::Pres_Sfx_Hit : FGameplayTag();
			if (Key.IsValid())
			{
				OutWeapon = TokenToWeapon(T[1]);
				OutKey = Key;
				return OutWeapon != EERWeaponType::None;
			}
		}
		return false;
	}

	/** (무기, 키) → 소리들. 이름순이라 r1 → r2. */
	using FSoundGroups = TMap<TPair<EERWeaponType, FGameplayTag>, TArray<UObject*>>;
	FSoundGroups GroupSounds(const TArray<FAssetData>& Sounds, FFillStats& Stats, const TCHAR* Where)
	{
		FSoundGroups Out;
		for (const FAssetData& A : Sounds)
		{
			EERWeaponType W = EERWeaponType::None;
			FGameplayTag Key;
			if (SoundToKey(A.AssetName.ToString(), W, Key))
			{
				Out.FindOrAdd({ W, Key }).Add(A.GetAsset());
			}
			else
			{
				Stats.Unmatched.Add(FString::Printf(TEXT("소리 %s (%s)"), *A.AssetName.ToString(), Where));
			}
		}
		return Out;
	}

	/** "S003" 형태면 대문자로 돌려준다. */
	FString AsSkinId(const FString& Token)
	{
		if (Token.Len() == 4 && (Token[0] == TEXT('S') || Token[0] == TEXT('s')) && Token.RightChop(1).IsNumeric())
		{
			return Token.ToUpper();
		}
		return FString();
	}

	/** 스킨 폴더의 몸 메시: 이름이 Mesh 또는 Character_* 인 것, 없으면 하나뿐일 때 그것. (레니 S001 은 무기 메시가 Mesh 폴더에 섞여 있다) */
	USkeletalMesh* PickSkinMesh(const FString& SkinFolder)
	{
		const TArray<FAssetData> Meshes = FindAssets(SkinFolder / TEXT("Mesh"), USkeletalMesh::StaticClass());
		for (const FAssetData& M : Meshes)
		{
			const FString N = M.AssetName.ToString();
			if (N == TEXT("Mesh") || N.StartsWith(TEXT("Character_")))
			{
				return Cast<USkeletalMesh>(M.GetAsset());
			}
		}
		return Meshes.Num() == 1 ? Cast<USkeletalMesh>(Meshes[0].GetAsset()) : nullptr;
	}

	using FKeyGroups = TMap<TPair<EERWeaponType, FGameplayTag>, TArray<UObject*>>;

	/**
	 * 모드 애니 규칙 (Argument 42 ⑥ A2) — "이 무기 토큰 · 이 동작" 은 "이 모드의 이 키들". 모드가 생길 때 한 줄씩 (F19: 재키 Saw · 매그너스 bike · 다니엘 mask).
	 * 동작표에 { 모드 · 키 · 애니 } 줄로 들어간다 — 무기 모드는 무기 세트에.
	 */
	struct FModeRule
	{
		const TCHAR* Token;           // 파일명 두 번째 토큰 (소문자)
		const TCHAR* Act;             // 나머지 (소문자)
		FGameplayTag Mode;
		TArray<FGameplayTag> Keys;
	};
	const TArray<FModeRule>& ModeRules()
	{
		static const TArray<FModeRule> Rules = {
			// 카티야 저격 D — Katja_Sniperrifle_Skill_{Start,Loop,Shot,End}
			{ TEXT("sniperrifle"), TEXT("skill_start"), ERTags::Mode_Sniper, { ERTags::Pres_Anim_ModeStart } },
			{ TEXT("sniperrifle"), TEXT("skill_loop"),  ERTags::Mode_Sniper, { ERTags::Pres_Anim_ModeIdle, ERTags::Pres_Anim_ModeRun } },
			{ TEXT("sniperrifle"), TEXT("skill_shot"),  ERTags::Mode_Sniper, { ERTags::Ability_Slot_Attack } },
			{ TEXT("sniperrifle"), TEXT("skill_end"),   ERTags::Mode_Sniper, { ERTags::Pres_Anim_ModeEnd } },
		};
		return Rules;
	}
	using FModeGroups = TMap<TPair<FGameplayTag, FGameplayTag>, TArray<UObject*>>;   // (모드, 키) → 애니

	/** 이름이 LayerName 인 AnimBP 가 있으면 Pres.AnimLayer 에 (비어 있거나 Force 일 때). 없으면 규칙 밖 목록에. */
	void LinkLayer(UERPresentationData& Pres, const FString& LayerName, bool bForce, FFillStats& Stats)
	{
		if (Pres.AnimLayer && !bForce)
		{
			return;
		}
		TArray<FAssetData> Layers = FindAssets(TEXT("/Game"), UAnimBlueprint::StaticClass());
		Layers.RemoveAll([&LayerName](const FAssetData& L) { return L.AssetName.ToString() != LayerName; });
		const UAnimBlueprint* LayerBP = Layers.IsEmpty() ? nullptr : Cast<UAnimBlueprint>(Layers[0].GetAsset());
		if (LayerBP && LayerBP->GeneratedClass && LayerBP->GeneratedClass->IsChildOf(UAnimInstance::StaticClass()))
		{
			Pres.AnimLayer = LayerBP->GeneratedClass.Get();
			UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기] %s → 레이어 %s"), *Pres.GetName(), *LayerName);
		}
		else if (!Pres.AnimLayer)
		{
			Stats.Unmatched.Add(FString::Printf(TEXT("%s (레이어 %s 없음 — 만들면 다시 Fill)"), *Pres.GetName(), *LayerName));
		}
	}

	void FillCharacter(const FString& Char, bool bForce)
	{
		FFillStats Stats;
		const FString CharRoot = FString::Printf(TEXT("/Game/ER/Characters/%s"), *Char);
		const TArray<FAssetData> Anims = FindAssets(CharRoot / TEXT("Animations"), UAnimSequenceBase::StaticClass());
		if (Anims.IsEmpty())
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[연출 채우기] %s/Animations 에 애니가 없다"), *CharRoot);
			return;
		}

		// 1) 분류 — 대상(기본 / 무기 세트 / 스킨) × (무기, 키) → 애니들 (이름순이라 atk01 → atk02)
		FKeyGroups BaseGroups;
		TMap<EERWeaponType, FKeyGroups> WeaponGroups;
		TMap<FString, FKeyGroups> SkinGroups;
		TMap<EERWeaponType, FModeGroups> ModeGroups;   // 무기 세트에 들어갈 모드 줄
		for (const FAssetData& A : Anims)
		{
			const FString Name = A.AssetName.ToString();
			TArray<FString> Tokens;
			Name.ParseIntoArray(Tokens, TEXT("_"));
			FString SkinId;
			for (int32 i = Tokens.Num() - 1; i >= 1; --i)
			{
				const FString Sid = AsSkinId(Tokens[i]);
				if (!Sid.IsEmpty())
				{
					SkinId = Sid;
					Tokens.RemoveAt(i);
				}
			}
			if (Tokens.Num() < 3 || !Tokens[0].Equals(Char, ESearchCase::IgnoreCase))
			{
				Stats.Unmatched.Add(Name + TEXT(" (이름 형식)"));
				continue;
			}
			const bool bCommon = Tokens[1].Equals(TEXT("Common"), ESearchCase::IgnoreCase);
			const EERWeaponType Weapon = bCommon ? EERWeaponType::None : TokenToWeapon(Tokens[1]);
			if (!bCommon && Weapon == EERWeaponType::None)
			{
				Stats.Unmatched.Add(FString::Printf(TEXT("%s (토큰 %s — 모드 · 로비 · 미지원)"), *Name, *Tokens[1]));
				continue;
			}
			const FString Act = FString::Join(TArray<FString>(Tokens.GetData() + 2, Tokens.Num() - 2), TEXT("_")).ToLower();
			// 모드 애니 규칙이 먼저 (저격 Sniperrifle_Skill_* …)
			const FString TokenLower = Tokens[1].ToLower();
			const FModeRule* Rule = ModeRules().FindByPredicate([&](const FModeRule& R) { return TokenLower == R.Token && Act == R.Act; });
			if (Rule && Weapon != EERWeaponType::None && SkinId.IsEmpty())
			{
				WeaponGroups.FindOrAdd(Weapon);   // 모드 줄만 있는 무기도 세트가 생기게
				for (const FGameplayTag& K : Rule->Keys)
				{
					ModeGroups.FindOrAdd(Weapon).FindOrAdd({ Rule->Mode, K }).Add(A.GetAsset());
				}
				continue;
			}
			FGameplayTag Key = ActionToKey(Act, bCommon);
			// 단검 D 의 weaponskill = 순간이동 후 찌르기 = **리캐스트(2번째) 모션** — 1번째(망토)는 모션 없음 (사용자 2026-09-29)
			if (Weapon == EERWeaponType::Dagger && Key == ERTags::Ability_Slot_D)
			{
				Key = ERTags::Ability_Slot_D_Recast;
			}
			if (!Key.IsValid())
			{
				Stats.Unmatched.Add(FString::Printf(TEXT("%s (동작 %s)"), *Name, *Act));
				continue;
			}
			UObject* Asset = A.GetAsset();
			if (!SkinId.IsEmpty())
			{
				SkinGroups.FindOrAdd(SkinId).FindOrAdd({ Weapon, Key }).Add(Asset);
			}
			else if (Weapon == EERWeaponType::None)
			{
				BaseGroups.FindOrAdd({ Weapon, Key }).Add(Asset);
			}
			else
			{
				WeaponGroups.FindOrAdd(Weapon).FindOrAdd({ EERWeaponType::None, Key }).Add(Asset);
			}
		}

		// 2) 기본 표 · 무기 세트
		UERPresentationData* Base = FindOrCreate<UERPresentationData>(PresRoot / Char, FString::Printf(TEXT("DA_Pres_%s"), *Char), Stats);
		if (!Base)
		{
			return;
		}
		// 새로 만드는 무기 세트 · 스킨 DA 는 기본 DA 옆에 — 사용자가 폴더를 옮겼으면 그 폴더 (2026-09-28)
		const FString Folder = FPackageName::GetLongPackagePath(Base->GetOutermost()->GetName());
		Base->Modify();
		for (const TPair<TPair<EERWeaponType, FGameplayTag>, TArray<UObject*>>& G : BaseGroups)
		{
			WriteEntry(Base->Entries, G.Key.Key, G.Key.Value, G.Value, bForce, Stats);
		}
		// 맨손 레이어 — 무기 없을 때 대기 · 달리기 (사용자 2026-09-28)
		LinkLayer(*Base, FString::Printf(TEXT("ABPL_%s_Unarmed"), *Char), bForce, Stats);
		for (const TPair<EERWeaponType, FKeyGroups>& WG : WeaponGroups)
		{
			const FString WeaponName = StaticEnum<EERWeaponType>()->GetNameStringByValue(static_cast<int64>(WG.Key));
			UERPresentationData* Set = FindOrCreate<UERPresentationData>(Folder, FString::Printf(TEXT("DA_Pres_%s_%s"), *Char, *WeaponName), Stats);
			if (!Set)
			{
				continue;
			}
			Set->Modify();
			for (const TPair<TPair<EERWeaponType, FGameplayTag>, TArray<UObject*>>& G : WG.Value)
			{
				WriteEntry(Set->Entries, EERWeaponType::None, G.Key.Value, G.Value, bForce, Stats);
			}
			if (const FModeGroups* MG = ModeGroups.Find(WG.Key))
			{
				for (const TPair<TPair<FGameplayTag, FGameplayTag>, TArray<UObject*>>& G : *MG)
				{
					WriteEntry(Set->Entries, EERWeaponType::None, G.Key.Value, G.Value, bForce, Stats, G.Key.Key);
				}
			}
			// 무기 레이어 — 이름이 ABPL_<Char>_<Weapon> 인 AnimBP 가 있으면 연결 (Argument 40 L2 · 사용자가 에디터에서 만든 것)
			LinkLayer(*Set, FString::Printf(TEXT("ABPL_%s_%s"), *Char, *WeaponName), bForce, Stats);
			Set->MarkPackageDirty();
			TSoftObjectPtr<UERPresentationData>& Slot = Base->WeaponSets.FindOrAdd(WG.Key);
			if (Slot.IsNull() || bForce)
			{
				Slot = Set;
			}
		}
		Base->MarkPackageDirty();

		// 3) 스킨 — Skins/<폴더> 하나당 DA 하나. 몸 메시 + 파일명에 S0nn 이 붙은 애니(덮어쓰기)
		TArray<FString> SkinFolders;
		Registry().GetSubPaths(CharRoot / TEXT("Skins"), SkinFolders, /*bRecurse=*/false);
		SkinFolders.Sort();
		TArray<TPair<FString, UERSkinData*>> SkinAssets;
		for (const FString& SkinFolder : SkinFolders)
		{
			FString SkinId;
			TArray<FString> Parts;
			FPaths::GetCleanFilename(SkinFolder).ParseIntoArray(Parts, TEXT("_"));
			for (const FString& P : Parts)
			{
				if (SkinId.IsEmpty()) { SkinId = AsSkinId(P); }
			}
			if (SkinId.IsEmpty())
			{
				Stats.Unmatched.Add(SkinFolder + TEXT(" (스킨 폴더 이름에 S0nn 없음)"));
				continue;
			}
			UERSkinData* SkinDA = FindOrCreate<UERSkinData>(Folder, FString::Printf(TEXT("DA_Skin_%s_%s"), *Char, *SkinId), Stats);
			if (!SkinDA)
			{
				continue;
			}
			SkinDA->Modify();
			if (!SkinDA->Mesh || bForce)
			{
				SkinDA->Mesh = PickSkinMesh(SkinFolder);
				if (!SkinDA->Mesh)
				{
					UE_LOG(LogEternalReturn, Warning, TEXT("[연출 채우기] %s — 몸 메시를 못 골랐다 (%s/Mesh). DA 에서 직접 지정"), *SkinDA->GetName(), *SkinFolder);
				}
			}
			if (const FKeyGroups* Groups = SkinGroups.Find(SkinId))
			{
				for (const TPair<TPair<EERWeaponType, FGameplayTag>, TArray<UObject*>>& G : *Groups)
				{
					WriteEntry(SkinDA->Overrides, G.Key.Key, G.Key.Value, G.Value, bForce, Stats);
				}
			}
			SkinDA->MarkPackageDirty();
			SkinAssets.Add({ SkinId, SkinDA });
		}
		SkinAssets.Sort([](const TPair<FString, UERSkinData*>& A, const TPair<FString, UERSkinData*>& B) { return A.Key < B.Key; });   // S000 이 0 번

		// 3.5) 소리 (F12.5-05 · Argument 49) — Character_FX/<캐릭터>/s000 = 이 캐릭터 전용 → 무기 세트 · s00x = 스킨 전용 → 스킨 DA (무기 칸 채움)
		//   원작 3층: 무기 기본(SFX/Attack … — FillWeapon) < 캐릭터(s000) < 스킨(s00x). 스킨은 무기 기본과 **같은 파일명 · 다른 내용** (사운드_분류.md)
		int32 SoundRows = 0;
		{
			TArray<FString> FxFolders;
			Registry().GetSubPaths(TEXT("/Game/ER/Audio/SFX/Character_FX/") + Char.ToLower(), FxFolders, /*bRecurse=*/false);
			for (const FString& FxFolder : FxFolders)
			{
				const FString SkinId = AsSkinId(FPaths::GetCleanFilename(FxFolder));
				if (SkinId.IsEmpty())
				{
					continue;
				}
				const FSoundGroups Groups = GroupSounds(FindAssets(FxFolder, USoundBase::StaticClass()), Stats, *SkinId);
				if (Groups.IsEmpty())
				{
					continue;
				}
				if (SkinId == TEXT("S000"))
				{
					for (const TPair<TPair<EERWeaponType, FGameplayTag>, TArray<UObject*>>& G : Groups)
					{
						const FString WeaponName = StaticEnum<EERWeaponType>()->GetNameStringByValue(static_cast<int64>(G.Key.Key));
						UERPresentationData* Set = FindOrCreate<UERPresentationData>(Folder, FString::Printf(TEXT("DA_Pres_%s_%s"), *Char, *WeaponName), Stats);
						if (!Set)
						{
							continue;
						}
						Set->Modify();
						SoundRows += WriteEntry(Set->Entries, EERWeaponType::None, G.Key.Value, G.Value, bForce, Stats) ? 1 : 0;
						Set->MarkPackageDirty();
						TSoftObjectPtr<UERPresentationData>& Slot = Base->WeaponSets.FindOrAdd(G.Key.Key);
						if (Slot.IsNull())
						{
							Slot = Set;
							Base->MarkPackageDirty();
						}
					}
					continue;
				}
				const TPair<FString, UERSkinData*>* Skin = SkinAssets.FindByPredicate([&SkinId](const TPair<FString, UERSkinData*>& S) { return S.Key == SkinId; });
				if (!Skin)
				{
					Stats.Unmatched.Add(FString::Printf(TEXT("소리 폴더 %s — 스킨 DA 가 없다 (Skins/ 에 %s 폴더 없음)"), *FxFolder, *SkinId));
					continue;
				}
				Skin->Value->Modify();
				for (const TPair<TPair<EERWeaponType, FGameplayTag>, TArray<UObject*>>& G : Groups)
				{
					// 스킨 줄은 **무기 칸**을 채운다 — 그 무기를 들 때만 (연출 컴포넌트 "스킨·무기" 층)
					SoundRows += WriteEntry(Skin->Value->Overrides, G.Key.Key, G.Key.Value, G.Value, bForce, Stats) ? 1 : 0;
				}
				Skin->Value->MarkPackageDirty();
			}
		}

		// 4) 실험체 DA 에 연결 — 이름에 캐릭터 이름이 든 UERCharacterData 가 하나일 때만
		TArray<FAssetData> CharDAs = FindAssets(TEXT("/Game"), UERCharacterData::StaticClass());
		CharDAs.RemoveAll([&Char](const FAssetData& A) { return !A.AssetName.ToString().Contains(Char, ESearchCase::IgnoreCase); });
		if (CharDAs.Num() == 1)
		{
			if (UERCharacterData* CD = Cast<UERCharacterData>(CharDAs[0].GetAsset()))
			{
				CD->Modify();
				if (!CD->Presentation || bForce)
				{
					CD->Presentation = Base;
				}
				if (CD->Skins.IsEmpty() || bForce)
				{
					CD->Skins.Reset();
					for (const TPair<FString, UERSkinData*>& S : SkinAssets)
					{
						CD->Skins.Add(S.Value);
					}
				}
				CD->MarkPackageDirty();
				UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기] %s → Presentation %s · Skins %d개"), *CD->GetName(), *GetNameSafe(CD->Presentation), CD->Skins.Num());
			}
		}
		else
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[연출 채우기] 이름에 %s 가 든 실험체 DA 가 %d개 — 연결은 손으로 (Presentation = %s · Skins)"),
				*Char, CharDAs.Num(), *Base->GetName());
		}

		for (const FString& U : Stats.Unmatched)
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기]   규칙 밖: %s"), *U);
		}
		UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기] %s — 애니 %d · 새 애셋 %d · 줄 채움 %d (소리 %d) · 유지 %d · 무기 세트 %d · 스킨 %d · 규칙 밖 %d. **Save All**"),
			*Char, Anims.Num(), Stats.Created, Stats.Written, SoundRows, Stats.Kept, WeaponGroups.Num(), SkinAssets.Num(), Stats.Unmatched.Num());
	}

	/** 야생동물 DA → 애셋 폴더 (종 이름이 폴더와 다른 것만 별칭). */
	FString WildFolderOf(const UERWildlifeData& D)
	{
		static const TMap<EERWildlifeType, FString> TypeFolder = {
			{ EERWildlifeType::Chicken, TEXT("Chicken") }, { EERWildlifeType::Bat, TEXT("Bat") }, { EERWildlifeType::Boar, TEXT("Boar") },
			{ EERWildlifeType::Crow, TEXT("Raven") }, { EERWildlifeType::WildDog, TEXT("Dog") }, { EERWildlifeType::Wolf, TEXT("Wolf") },
			{ EERWildlifeType::Bear, TEXT("Bear") }, { EERWildlifeType::Alpha, TEXT("Alpha") }, { EERWildlifeType::Omega, TEXT("Omega") },
			{ EERWildlifeType::Wickeline, TEXT("Wickline") },
		};
		const FString* Type = TypeFolder.Find(D.Type);
		if (!Type)
		{
			return FString();
		}
		const TCHAR* Suffix = D.Variant == EERWildlifeVariant::Mutant ? TEXT("_Mutant") : D.Variant == EERWildlifeVariant::Corrupted ? TEXT("_FogMutant") : TEXT("_01");
		return *Type + Suffix;
	}

	void FillWild(bool bForce)
	{
		FFillStats Stats;
		int32 Linked = 0;
		// ⚠ 종 애니 폴더를 **경로로 박지 않는다** — 사용자가 /Game/ER/Monsters → /Game/ER/Wildlife 로 옮기자 전 종이 "애니 없음" 으로 빠졌다 (E30 · E26 과 같은 부류).
		//   ".../<종 폴더>/Animations/" 로 끝나는 패키지 경로면 어디든 쓴다.
		const TArray<FAssetData> AllAnims = FindAssets(TEXT("/Game"), UAnimSequenceBase::StaticClass());
		for (const FAssetData& A : FindAssets(TEXT("/Game"), UERWildlifeData::StaticClass()))
		{
			UERWildlifeData* D = Cast<UERWildlifeData>(A.GetAsset());
			const FString WildFolder = D ? WildFolderOf(*D) : FString();
			if (WildFolder.IsEmpty())
			{
				Stats.Unmatched.Add(A.AssetName.ToString() + TEXT(" (종 폴더 없음)"));
				continue;
			}
			FKeyGroups Groups;
			const FString AnimDirSuffix = TEXT("/") + WildFolder + TEXT("/Animations");
			for (const FAssetData& Anim : AllAnims)
			{
				if (!Anim.PackagePath.ToString().EndsWith(AnimDirSuffix, ESearchCase::IgnoreCase))
				{
					continue;
				}
				const FString N = Anim.AssetName.ToString().ToLower();
				const FGameplayTag Key = (N.EndsWith(TEXT("_atk01")) || N.EndsWith(TEXT("_atk02"))) ? ERTags::Ability_Slot_Attack
					: N.EndsWith(TEXT("_death")) ? ERTags::Pres_Anim_Death : FGameplayTag();
				if (Key.IsValid())
				{
					Groups.FindOrAdd({ EERWeaponType::None, Key }).Add(Anim.GetAsset());
				}
			}
			if (Groups.IsEmpty())
			{
				Stats.Unmatched.Add(FString::Printf(TEXT("%s (%s 에 atk · death 애니 없음 — 소리만 채운다)"), *A.AssetName.ToString(), *WildFolder));
			}
			// 소리 (F12.5-05 · Argument 49) — 원작 이름이 종마다 제각각이라 **종 표**로. 변이 · 잠식도 같은 종 소리. 접두어로 찾는다 (atk01 · atk02 · r1 · r2 가 변형)
			//   ⚠ "<종>Hit" 이 "그 종의 공격이 맞은 소리" 인지 "그 종이 맞은 소리" 인지 (미확인) — 무기 hit<무기> 와 같은 뜻으로 보고 **공격 타격음**에 둔다
			{
				struct FWildSfx { const TCHAR* Attack; const TCHAR* Hit; const TCHAR* Die; };
				static const TMap<EERWildlifeType, FWildSfx> SfxOf = {
					{ EERWildlifeType::Chicken,   { TEXT("chickenAttack"),   TEXT("chickenHit"),          TEXT("chickenDie") } },
					{ EERWildlifeType::Bat,       { TEXT("batAttack"),       TEXT("batHit"),              TEXT("batDie") } },
					{ EERWildlifeType::Boar,      { TEXT("boar_attack"),     TEXT("boarHit"),             TEXT("boarDie") } },   // boarAttack 아님 — 사용자가 들어보고 고름 (2026-09-29)
					{ EERWildlifeType::WildDog,   { TEXT("wildDogAttack"),   TEXT("wildDogHit"),          TEXT("wildDogDie") } },
					{ EERWildlifeType::Wolf,      { TEXT("wolfAttack"),      TEXT("wolfHit"),             TEXT("wolfDie") } },
					{ EERWildlifeType::Bear,      { TEXT("bearAttack"),      TEXT("bearHit"),             TEXT("bearDie") } },
					{ EERWildlifeType::Alpha,     { TEXT("AlphaOmega_atk0"), TEXT("AlphaOmega_atk_hit"),  TEXT("AlphaOmega_dead") } },
					{ EERWildlifeType::Omega,     { TEXT("AlphaOmega_atk0"), TEXT("AlphaOmega_atk_hit"),  TEXT("AlphaOmega_dead") } },
					{ EERWildlifeType::Wickeline, { TEXT("wicklineAttack"),  nullptr,                     TEXT("wicklineDie") } },
				};
				static TArray<FAssetData> MonsterSounds;   // 한 번만 찾는다
				if (MonsterSounds.IsEmpty())
				{
					MonsterSounds = FindAssets(TEXT("/Game/ER/Audio/SFX/Monster"), USoundBase::StaticClass());
				}
				const FWildSfx* Sfx = SfxOf.Find(D->Type);
				if (!Sfx)
				{
					Stats.Unmatched.Add(FString::Printf(TEXT("%s — 소리 없음 (원작 SFX/Monster 에 이 종 이름이 없다)"), *A.AssetName.ToString()));
				}
				auto AddSound = [&](const TCHAR* Prefix, FGameplayTag Key)
				{
					if (!Prefix)
					{
						return;
					}
					for (const FAssetData& S : MonsterSounds)
					{
						if (S.AssetName.ToString().StartsWith(Prefix, ESearchCase::CaseSensitive))   // wolfAttack ≠ wolf_attack (뜻 미확인 · 빼둔다)
						{
							Groups.FindOrAdd({ EERWeaponType::None, Key }).Add(S.GetAsset());
						}
					}
				};
				if (Sfx)
				{
					AddSound(Sfx->Attack, ERTags::Pres_Sfx_Attack);
					AddSound(Sfx->Hit, ERTags::Pres_Sfx_Hit);
					AddSound(Sfx->Die, ERTags::Pres_Sfx_Die);
				}
			}
			if (Groups.IsEmpty())
			{
				continue;   // 애니도 소리도 없다
			}
			UERPresentationData* Pres = FindOrCreate<UERPresentationData>(PresRoot / TEXT("Wildlife"), TEXT("DA_Pres_") + WildFolder, Stats);
			if (!Pres)
			{
				continue;
			}
			Pres->Modify();
			for (const TPair<TPair<EERWeaponType, FGameplayTag>, TArray<UObject*>>& G : Groups)
			{
				WriteEntry(Pres->Entries, EERWeaponType::None, G.Key.Value, G.Value, bForce, Stats);
			}
			Pres->MarkPackageDirty();
			if (!D->Presentation || bForce)
			{
				D->Modify();
				D->Presentation = Pres;
				D->MarkPackageDirty();
				++Linked;
			}
		}
		for (const FString& U : Stats.Unmatched)
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기]   규칙 밖: %s"), *U);
		}
		UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기] 야생동물 — 새 애셋 %d · 줄 채움 %d · 유지 %d · DA 연결 %d · 규칙 밖 %d. **Save All**"),
			Stats.Created, Stats.Written, Stats.Kept, Linked, Stats.Unmatched.Num());
	}

	/**
	 * 무기 공통 DA (F12.5-05 · Argument 49) — 원작의 **무기 기본** 소리 (`SFX/Attack · Attack_Hit · Skill · Skill_Hit`, 캐릭터 칸이 빈 것) → `DA_Pres_Weapon_<무기>`.
	 * 캐릭터 · 스킨 전용 소리는 FillCharacter (Character_FX) 가 무기 세트 · 스킨 DA 에 — 이 층을 덮는다.
	 * ⚠ 예전 임포트 `SFX/Weapon/<무기>/` 는 읽지 않는다 (같은 소리 중복). DT_WeaponClass.Presentation 연결은 CSV 에서.
	 */
	void FillWeapon(bool bForce)
	{
		const FString Folder = TEXT("/Game/ERCharacter/CharData/Presentation/Weapon");
		FFillStats Stats;
		FSoundGroups All;
		for (const TCHAR* Sub : { TEXT("Attack"), TEXT("Attack_Hit"), TEXT("Skill"), TEXT("Skill_Hit") })
		{
			const FSoundGroups G = GroupSounds(FindAssets(FString(TEXT("/Game/ER/Audio/SFX/")) + Sub, USoundBase::StaticClass()), Stats, Sub);
			for (const TPair<TPair<EERWeaponType, FGameplayTag>, TArray<UObject*>>& P : G)
			{
				All.FindOrAdd(P.Key).Append(P.Value);
			}
		}
		TMap<EERWeaponType, TArray<FString>> Made;
		for (const TPair<TPair<EERWeaponType, FGameplayTag>, TArray<UObject*>>& P : All)
		{
			const FString Name = StaticEnum<EERWeaponType>()->GetNameStringByValue(static_cast<int64>(P.Key.Key));
			UERPresentationData* DA = FindOrCreate<UERPresentationData>(Folder, TEXT("DA_Pres_Weapon_") + Name, Stats);
			if (!DA)
			{
				continue;
			}
			DA->Modify();
			if (WriteEntry(DA->Entries, EERWeaponType::None, P.Key.Value, P.Value, bForce, Stats))
			{
				DA->MarkPackageDirty();
			}
			Made.FindOrAdd(P.Key.Key).Add(FString::Printf(TEXT("%s %d"), *P.Key.Value.ToString().RightChop(9), P.Value.Num()));   // "Pres.Sfx." 떼고
		}
		for (const TPair<EERWeaponType, TArray<FString>>& M : Made)
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기]   무기 공통 %s — %s"), *StaticEnum<EERWeaponType>()->GetNameStringByValue(static_cast<int64>(M.Key)), *FString::Join(M.Value, TEXT(" · ")));
		}
		for (const FString& U : Stats.Unmatched)
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기]   규칙 밖: %s"), *U);
		}
		UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기] 무기 공통 %d개 — 새 애셋 %d · 줄 채움 %d · 유지 %d · 규칙 밖 %d. **Save All** → DT_WeaponClass CSV 의 Presentation 칸 · Reimport"),
			Made.Num(), Stats.Created, Stats.Written, Stats.Kept, Stats.Unmatched.Num());
	}

	void PresFillCmd(const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[연출 채우기] 사용법: ER.Pres.Fill <캐릭터|All|Wild|Weapon> [Force]  예) ER.Pres.Fill Jackie"));
			return;
		}
		const bool bForce = Args.Num() >= 2 && Args[1].Equals(TEXT("Force"), ESearchCase::IgnoreCase);
		if (Args[0].Equals(TEXT("Wild"), ESearchCase::IgnoreCase))
		{
			FillWild(bForce);
			return;
		}
		if (Args[0].Equals(TEXT("Weapon"), ESearchCase::IgnoreCase))
		{
			FillWeapon(bForce);
			return;
		}
		if (Args[0].Equals(TEXT("All"), ESearchCase::IgnoreCase))
		{
			TArray<FString> CharFolders;
			Registry().GetSubPaths(TEXT("/Game/ER/Characters"), CharFolders, /*bRecurse=*/false);
			for (const FString& F : CharFolders)
			{
				FillCharacter(FPaths::GetCleanFilename(F), bForce);
			}
			FillWild(bForce);
			FillWeapon(bForce);
			return;
		}
		FillCharacter(Args[0], bForce);
	}
}

static FAutoConsoleCommand GERPresFillCmd(
	TEXT("ER.Pres.Fill"), TEXT("[에디터] 애니 파일명 규칙으로 연출 DA 생성 · 채우기. ER.Pres.Fill <캐릭터|All|Wild|Weapon> [Force] — 끝나면 Save All"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&PresFillCmd));

#endif // WITH_EDITOR
