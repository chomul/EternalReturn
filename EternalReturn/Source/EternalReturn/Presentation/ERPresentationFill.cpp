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
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Character/ERCharacterData.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayTags.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Internationalization/Regex.h"
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
		TSet<UObject*> Touched;   // 줄을 쓰거나 본 DA — 끝에 TidyAll 이 칸 정리 · 정렬 (Argument 74)
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

	/** 키 뿌리 → 칸 (Argument 74): Pres.Voice → 음성 · Pres.Sfx → 소리 · 나머지(Ability.Slot · Pres.Anim) → 애니 */
	enum class EListKind : uint8 { Anim, Sound, Voice };
	EListKind KindOf(const FGameplayTag& Key)
	{
		return Key.MatchesTag(ERTags::Pres_Voice) ? EListKind::Voice : Key.MatchesTag(ERTags::Pres_Sfx) ? EListKind::Sound : EListKind::Anim;
	}
	TArray<FERPresentationEntry>* ListOf(UERPresentationData& DA, EListKind K) { return K == EListKind::Voice ? &DA.Voices : K == EListKind::Sound ? &DA.Sounds : &DA.Entries; }
	TArray<FERPresentationEntry>* ListOf(UERSkinData& DA, EListKind K) { return K == EListKind::Voice ? &DA.OverrideVoices : K == EListKind::Sound ? &DA.OverrideSounds : &DA.Overrides; }

	/**
	 * 잘못된 칸에 있는 줄을 제 칸으로 (Argument 74 — 예전 DA 는 전부 Entries · Overrides 에 있었다). 제 칸에 같은 (모드, 무기, 키) 줄이 이미 있으면 옛 줄은 지운다.
	 * 그다음 칸마다 키 → 모드 → 무기 순으로 정렬 (보기용 — 조회는 키로 하고 한 층 안에 같은 키 줄은 하나라 순서는 결과와 무관). 반환: 옮긴 줄 수
	 */
	template <typename TDA>
	int32 Tidy(TDA& DA)
	{
		int32 Moved = 0, Dropped = 0;
		for (EListKind From : { EListKind::Anim, EListKind::Sound, EListKind::Voice })
		{
			TArray<FERPresentationEntry>& Src = *ListOf(DA, From);
			for (int32 i = Src.Num() - 1; i >= 0; --i)
			{
				const EListKind To = KindOf(Src[i].Key);
				if (To == From || !Src[i].Key.IsValid())
				{
					continue;
				}
				TArray<FERPresentationEntry>& Dst = *ListOf(DA, To);
				const FERPresentationEntry& E = Src[i];
				if (Dst.ContainsByPredicate([&E](const FERPresentationEntry& D) { return D.Mode == E.Mode && D.Weapon == E.Weapon && D.Key == E.Key; }))
				{
					++Dropped;
				}
				else
				{
					Dst.Add(E);
					++Moved;
				}
				Src.RemoveAt(i);
			}
		}
		for (EListKind K : { EListKind::Anim, EListKind::Sound, EListKind::Voice })
		{
			ListOf(DA, K)->StableSort([](const FERPresentationEntry& A, const FERPresentationEntry& B)
			{
				if (A.Key != B.Key) { return A.Key.GetTagName().LexicalLess(B.Key.GetTagName()); }
				if (A.Mode != B.Mode) { return A.Mode.GetTagName().LexicalLess(B.Mode.GetTagName()); }
				return static_cast<uint8>(A.Weapon) < static_cast<uint8>(B.Weapon);
			});
		}
		if (Moved + Dropped > 0)
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기] %s — 칸 나누기: 옮김 %d · 중복 지움 %d (애니 %d · 소리 %d · 음성 %d)"), *DA.GetName(), Moved, Dropped,
				ListOf(DA, EListKind::Anim)->Num(), ListOf(DA, EListKind::Sound)->Num(), ListOf(DA, EListKind::Voice)->Num());
		}
		return Moved;
	}

	/** 키에 맞는 칸에 쓴다 (Argument 74). 쓰기 전에 그 DA 를 정리해 옛 칸의 같은 줄을 찾게 한다 — Force 가 아니면 손댄 줄을 지킨다 */
	template <typename TDA>
	bool WriteEntry(TDA& DA, EERWeaponType Weapon, FGameplayTag Key, const TArray<UObject*>& Assets, bool bForce, FFillStats& Stats, FGameplayTag Mode = FGameplayTag())
	{
		bool bAlready = false;
		Stats.Touched.Add(&DA, &bAlready);
		if (!bAlready)
		{
			DA.Modify();
			Tidy(DA);
		}
		return WriteEntry(*ListOf(DA, KindOf(Key)), Weapon, Key, Assets, bForce, Stats, Mode);
	}

	/** Fill 끝 — 손댄 DA 마다 다시 정리 · 정렬 */
	void TidyAll(FFillStats& Stats)
	{
		for (UObject* O : Stats.Touched)
		{
			if (UERPresentationData* P = Cast<UERPresentationData>(O)) { Tidy(*P); P->MarkPackageDirty(); }
			else if (UERSkinData* K = Cast<UERSkinData>(O)) { Tidy(*K); K->MarkPackageDirty(); }
		}
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

	/** 스킬 키 → 판정 순간 키 (Ability.Slot.Q → Ability.Slot.Q.Execute). 없으면 빈 태그. */
	FGameplayTag ExecuteKeyOf(const FGameplayTag& SlotKey)
	{
		if (SlotKey == ERTags::Ability_Slot_Q) { return ERTags::Ability_Slot_Q_Execute; }
		if (SlotKey == ERTags::Ability_Slot_W) { return ERTags::Ability_Slot_W_Execute; }
		if (SlotKey == ERTags::Ability_Slot_E) { return ERTags::Ability_Slot_E_Execute; }
		if (SlotKey == ERTags::Ability_Slot_R) { return ERTags::Ability_Slot_R_Execute; }
		return FGameplayTag();
	}

	FGameplayTag ActionToKey(const FString& ActLower, bool bCommon)
	{
		// 여러 단계 스킬 (F19-01 카티야 R Skill04_Start / Loop / Fire / End · Argument 53 L2) — 위클라인과 같은 규칙:
		//   _start = 선딜 (같은 키의 에디터 몽타주 AM_…_Start 가 있으면 그게 대신 — Start → Loop 섹션 반복) · _fire · _shot = 판정 순간
		//   _loop · _end 는 넣지 않는다 (loop 은 몽타주 섹션 · end 는 아직 자리 없음 → 규칙 밖)
		int32 Under = INDEX_NONE;
		if (ActLower.StartsWith(TEXT("skill")) && ActLower.FindChar(TEXT('_'), Under))
		{
			const FGameplayTag SlotKey = ActionToKey(ActLower.Left(Under), bCommon);
			const FString Phase = ActLower.Mid(Under + 1);
			if (SlotKey.IsValid() && Phase == TEXT("start")) { return SlotKey; }
			if (SlotKey.IsValid() && (Phase == TEXT("fire") || Phase == TEXT("shot"))) { return ExecuteKeyOf(SlotKey); }
			return FGameplayTag();
		}
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
		// _movespeed = 단검 D 망토(이속) 소리 — 공용 "스킬 시전" 에 섞이면 재키 Q 가 이걸 냈다 (2026-10-06 · 사용자 "공용 스킬 소리에서 빼") · 단검 D 정리 때 따로
		if (L.Contains(TEXT("_in")) || L.Contains(TEXT("_v")) || L.Contains(TEXT("_wall")) || L.Contains(TEXT("_movespeed")))
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
			// <캐릭터>_<무기>_Shot = 평타 발사음 · Reinforce_Shot / _Hit / _Ready = 다음 평타 강화 (카티야 P · K8 · Docs/3_EditorTasks/Audio/Katja.md)
			const FGameplayTag Key = (Rest == TEXT("normalattack") || Rest == TEXT("shot")) ? ERTags::Pres_Sfx_Attack
				: (Rest == TEXT("normalattack_hit") || Rest == TEXT("normal_hit")) ? ERTags::Pres_Sfx_Hit
				: Rest == TEXT("reinforce_shot") ? ERTags::Pres_Sfx_AttackEnhanced
				: Rest == TEXT("reinforce_hit") ? ERTags::Pres_Sfx_HitEnhanced
				: Rest == TEXT("reinforce_ready") ? ERTags::Pres_Sfx_EnhanceReady : FGameplayTag();
			if (Key.IsValid())
			{
				OutWeapon = TokenToWeapon(T[1]);
				OutKey = Key;
				return OutWeapon != EERWeaponType::None;
			}
		}
		// 스킬 판정 순간 소리 (F19-01 K8 · Argument 59) — <캐릭터>_<무기>_Skill0N_Shot[_0K] → SkillCast.<슬롯> · _Hit[_0K] → SkillHit.<슬롯>
		//   _02 · _03 은 이름순으로 뒤에 붙는다 → 순차 사격 발 번호로 고른다 (PickSound). 그 밖 (Aiming · Scan …) 은 모션 소리 — 노티파이 (규칙 밖)
		if (T.Num() >= 4 && T.Num() <= 5 && (T.Num() == 4 || T[4].IsNumeric()))
		{
			const FString Event = T[3].ToLower();
			const FGameplayTag Slot = ActionToKey(T[2].ToLower(), /*bCommon=*/false);
			// Aiming = 순차 사격 발 사이 조준 (R 만 · 사용자 2026-10-02 "각 Shot 전에 조준") — 첫 Aiming 은 노티파이로도 쓴다 (Start)
			struct FSlotSfx { FGameplayTag Slot; FGameplayTag Cast; FGameplayTag Hit; FGameplayTag Aim; };
			static const FSlotSfx SlotSfx[] = {
				{ ERTags::Ability_Slot_Q, ERTags::Pres_Sfx_SkillCast_Q, ERTags::Pres_Sfx_SkillHit_Q, FGameplayTag() },
				{ ERTags::Ability_Slot_W, ERTags::Pres_Sfx_SkillCast_W, ERTags::Pres_Sfx_SkillHit_W, FGameplayTag() },
				{ ERTags::Ability_Slot_E, ERTags::Pres_Sfx_SkillCast_E, ERTags::Pres_Sfx_SkillHit_E, FGameplayTag() },
				{ ERTags::Ability_Slot_R, ERTags::Pres_Sfx_SkillCast_R, ERTags::Pres_Sfx_SkillHit_R, ERTags::Pres_Sfx_SkillAim_R },
			};
			for (const FSlotSfx& S : SlotSfx)
			{
				const FGameplayTag Key = S.Slot != Slot ? FGameplayTag()
					: Event == TEXT("shot") ? S.Cast : Event == TEXT("hit") ? S.Hit : Event == TEXT("aiming") ? S.Aim : FGameplayTag();
				if (Key.IsValid())
				{
					OutWeapon = TokenToWeapon(T[1]);
					OutKey = Key;
					return OutWeapon != EERWeaponType::None;
				}
			}
		}
		// 무기 토큰 없는 캐릭터 스킬 소리 (F19-02 · Docs/3_EditorTasks/Audio/Magnus.md) — <캐릭터>_Skill0N_<사건>[_rK] → **캐릭터 기본 표** (무기 무관 · OutWeapon None)
		//   사건: Attack · Activation = 시전 · Hit = 타격 · Impact = 타격 뒤 늦게 · Drive = 반복. _rK 는 무작위 변형 (같은 키로 묶인다)
		if (T.Num() == 3 || (T.Num() == 4 && T[3].StartsWith(TEXT("r"), ESearchCase::IgnoreCase) && T[3].RightChop(1).IsNumeric()))
		{
			// 예외 — 같은 사건 이름이 스킬마다 뜻이 다르다. 배치표(Audio/<캐릭터>.md)가 원본이고 이 표는 그 거울이다
			static const TMap<FString, FGameplayTag> Exceptions = {
				{ TEXT("magnus_skill02_attack"), ERTags::Pres_Sfx_SkillLoop_W },       // 도는 동안 나는 소리 (사용자 2026-10-04)
				{ TEXT("magnus_skill04_attack"), ERTags::Pres_Sfx_SkillRecast_R },     // 바이크 발사
				{ TEXT("magnus_skill04_goactive"), ERTags::Pres_Sfx_SkillLoopStart_R }, // 시동 (사용자 2026-10-04)
			};
			const FString Base3 = FString::Join(TArray<FString>(T.GetData(), 3), TEXT("_")).ToLower();
			FGameplayTag Key = Exceptions.FindRef(Base3);
			if (!Key.IsValid())
			{
				const FGameplayTag Slot = ActionToKey(T[1].ToLower(), /*bCommon=*/false);
				const FString Event = T[2].ToLower();
				struct FCharSfx { FGameplayTag Slot; FGameplayTag Cast; FGameplayTag Hit; FGameplayTag HitLate; FGameplayTag Loop; };
				static const FCharSfx CharSfx[] = {
					{ ERTags::Ability_Slot_Q, ERTags::Pres_Sfx_SkillCast_Q, ERTags::Pres_Sfx_SkillHit_Q, ERTags::Pres_Sfx_SkillHitLate_Q, FGameplayTag() },
					{ ERTags::Ability_Slot_W, ERTags::Pres_Sfx_SkillCast_W, ERTags::Pres_Sfx_SkillHit_W, FGameplayTag(), ERTags::Pres_Sfx_SkillLoop_W },
					{ ERTags::Ability_Slot_E, ERTags::Pres_Sfx_SkillCast_E, ERTags::Pres_Sfx_SkillHit_E, FGameplayTag(), FGameplayTag() },
					{ ERTags::Ability_Slot_R, ERTags::Pres_Sfx_SkillCast_R, ERTags::Pres_Sfx_SkillHit_R, FGameplayTag(), ERTags::Pres_Sfx_SkillLoop_R },
				};
				for (const FCharSfx& S : CharSfx)
				{
					if (S.Slot == Slot)
					{
						Key = (Event == TEXT("attack") || Event == TEXT("activation")) ? S.Cast : Event == TEXT("hit") ? S.Hit
							: Event == TEXT("impact") ? S.HitLate : Event == TEXT("drive") ? S.Loop : FGameplayTag();
					}
				}
			}
			if (Key.IsValid())
			{
				OutWeapon = EERWeaponType::None;
				OutKey = Key;
				return true;
			}
		}
		return false;
	}

	/**
	 * 연출 채우기 규칙 — **데이터** `Docs/3_EditorTasks/Data/Presentation/*.json` (Argument 72 A1 · 2026-10-07 C++ 에서 옮김). 고치면 Fill 만 다시 (빌드 · 재시작 불필요).
	 * AnimRules  : 파일명 토큰(무기 · 모드 · common · *) + 동작 → 모드 · 키들. Keys 가 비면 그 애니는 안 씀 (규칙 밖 목록에 남김)
	 * SoundRules : 소리 이름(소문자 정확히) → 키 · 모드 · 무기 (배치표 `Audio/<캐릭터>.md` 의 거울). Key 가 비면 안 씀 · 같은 키 여러 줄 = 무작위 변형
	 * 파일 이름 = 캐릭터 (그 캐릭터만) · `_` 로 시작하면 누구나 (카티야 저격 D 처럼 무기 규칙)
	 */
	struct FModeRule
	{
		FString Token;
		FString Act;
		FGameplayTag Mode;            // 비면 모드 아닌 줄
		TArray<FGameplayTag> Keys;
		FString Char;                 // 비면 누구나
		bool bToBase = false;         // 기본 표로 (무기 무관) — 아니면 그 무기 세트
		bool bAlsoNormal = false;     // 규칙을 쓰고도 평소 규칙도 탄다
	};
	struct FCharSound
	{
		FString Name;
		FGameplayTag Key;
		FGameplayTag Mode;
		EERWeaponType Weapon = EERWeaponType::None;
	};
	TArray<FModeRule> GModeRules;
	TArray<FCharSound> GCharSounds;
	const TArray<FModeRule>& ModeRules() { return GModeRules; }
	const TArray<FCharSound>& CharSoundRules() { return GCharSounds; }

	/**
	 * 음성 규칙 (Argument 73) — `VoiceRules` 칸. 상황 키(`<캐릭터>_<상황키>_<n>_ko[_k]` 의 가운데)를 정규식 Match 로 → 음성 키 · 재생 규칙.
	 * bWeaponFromMatch = 첫 괄호가 무기 이름 (learnweaponskill_Pistol) → 그 무기 세트 DA 에. `VoiceWeaponAlias` = 원작 무기 이름 → EERWeaponType 이름
	 */
	struct FVoiceFillRule
	{
		FString Match;
		FGameplayTag Key;
		FERVoiceRule Rule;
		bool bWeaponFromMatch = false;
	};
	TArray<FVoiceFillRule> GVoiceRules;
	TMap<FString, FString> GVoiceWeaponAlias;   // 소문자 → enum 이름

	FGameplayTag RuleTag(const FString& S, const FString& Who, int32& Errors)
	{
		if (S.IsEmpty())
		{
			return FGameplayTag();
		}
		const FGameplayTag T = FGameplayTag::RequestGameplayTag(*S, /*ErrorIfNotFound=*/false);
		if (!T.IsValid())
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[연출 규칙] %s — 태그 '%s' 가 없다"), *Who, *S);
			++Errors;
		}
		return T;
	}

	/** 매 Fill 마다 다시 읽는다. 반환: 에러 수 */
	int32 LoadFillRules()
	{
		GModeRules.Reset();
		GCharSounds.Reset();
		GVoiceRules.Reset();
		GVoiceWeaponAlias.Reset();
		const FString Dir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Docs/3_EditorTasks/Data/Presentation"));
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.json")), /*Files=*/true, /*Directories=*/false);
		int32 Errors = 0;
		for (const FString& F : Files)
		{
			FString Text;
			TSharedPtr<FJsonObject> Root;
			if (!FFileHelper::LoadFileToString(Text, *(Dir / F)) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
			{
				UE_LOG(LogEternalReturn, Error, TEXT("[연출 규칙] %s — JSON 을 못 읽었다"), *F);
				++Errors;
				continue;
			}
			const FString BaseName = FPaths::GetBaseFilename(F);
			const FString Char = BaseName.StartsWith(TEXT("_")) ? FString() : BaseName.ToLower();
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (Root->TryGetArrayField(TEXT("AnimRules"), Arr))
			{
				for (const TSharedPtr<FJsonValue>& V : *Arr)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					if (!O.IsValid()) { continue; }
					FModeRule R;
					O->TryGetStringField(TEXT("Token"), R.Token);
					O->TryGetStringField(TEXT("Act"), R.Act);
					R.Token = R.Token.ToLower();
					R.Act = R.Act.ToLower();
					const FString Who = F + TEXT(" ") + R.Token + TEXT("_") + R.Act;
					FString ModeName;
					O->TryGetStringField(TEXT("Mode"), ModeName);
					R.Mode = RuleTag(ModeName, Who, Errors);
					const TArray<TSharedPtr<FJsonValue>>* Keys = nullptr;
					if (O->TryGetArrayField(TEXT("Keys"), Keys))
					{
						for (const TSharedPtr<FJsonValue>& K : *Keys)
						{
							const FGameplayTag T = RuleTag(K->AsString(), Who, Errors);
							if (T.IsValid()) { R.Keys.Add(T); }
						}
					}
					R.Char = Char;
					O->TryGetBoolField(TEXT("bToBase"), R.bToBase);
					O->TryGetBoolField(TEXT("bAlsoNormal"), R.bAlsoNormal);
					GModeRules.Add(R);
				}
			}
			if (Root->TryGetArrayField(TEXT("SoundRules"), Arr))
			{
				for (const TSharedPtr<FJsonValue>& V : *Arr)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					if (!O.IsValid()) { continue; }
					FCharSound C;
					O->TryGetStringField(TEXT("Name"), C.Name);
					C.Name = C.Name.ToLower();
					const FString Who = F + TEXT(" ") + C.Name;
					FString KeyName, ModeName, WeaponName;
					O->TryGetStringField(TEXT("Key"), KeyName);
					O->TryGetStringField(TEXT("Mode"), ModeName);
					C.Key = RuleTag(KeyName, Who, Errors);
					C.Mode = RuleTag(ModeName, Who, Errors);
					if (O->TryGetStringField(TEXT("Weapon"), WeaponName) && !WeaponName.IsEmpty())
					{
						const int64 W = StaticEnum<EERWeaponType>()->GetValueByNameString(WeaponName);
						if (W == INDEX_NONE)
						{
							UE_LOG(LogEternalReturn, Error, TEXT("[연출 규칙] %s — 무기 '%s' 가 없다"), *Who, *WeaponName);
							++Errors;
						}
						else
						{
							C.Weapon = static_cast<EERWeaponType>(W);
						}
					}
					GCharSounds.Add(C);
				}
			}
			if (Root->TryGetArrayField(TEXT("VoiceRules"), Arr))
			{
				for (const TSharedPtr<FJsonValue>& V : *Arr)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					if (!O.IsValid()) { continue; }
					FVoiceFillRule R;
					O->TryGetStringField(TEXT("Match"), R.Match);
					FString KeyName;
					O->TryGetStringField(TEXT("Key"), KeyName);
					R.Key = RuleTag(KeyName, F + TEXT(" ") + R.Match, Errors);
					if (R.Match.IsEmpty() || !R.Key.IsValid() || !R.Key.MatchesTag(ERTags::Pres_Voice))
					{
						UE_LOG(LogEternalReturn, Error, TEXT("[연출 규칙] %s 음성 '%s' — Match 가 비었거나 Key 가 Pres.Voice.* 가 아니다"), *F, *R.Match);
						++Errors;
						continue;
					}
					double Num = 0.0;
					if (O->TryGetNumberField(TEXT("Chance"), Num)) { R.Rule.Chance = FMath::Clamp(static_cast<float>(Num), 0.f, 1.f); }
					if (O->TryGetNumberField(TEXT("Cooldown"), Num)) { R.Rule.Cooldown = FMath::Max(0.f, static_cast<float>(Num)); }
					O->TryGetBoolField(TEXT("bInterrupt"), R.Rule.bInterrupt);
					O->TryGetBoolField(TEXT("bWeaponFromMatch"), R.bWeaponFromMatch);
					GVoiceRules.Add(R);
				}
			}
			const TSharedPtr<FJsonObject>* AliasJ = nullptr;
			if (Root->TryGetObjectField(TEXT("VoiceWeaponAlias"), AliasJ))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : (*AliasJ)->Values)
				{
					if (!KV.Key.StartsWith(TEXT("_"))) { GVoiceWeaponAlias.Add(KV.Key.ToLower(), KV.Value->AsString()); }
				}
			}
		}
		UE_LOG(LogEternalReturn, Log, TEXT("[연출 규칙] %s — 파일 %d · 애니 규칙 %d · 소리 %d · 음성 %d%s"), *Dir, Files.Num(), GModeRules.Num(), GCharSounds.Num(), GVoiceRules.Num(),
			Errors > 0 ? *FString::Printf(TEXT(" · ⚠ 에러 %d"), Errors) : TEXT(""));
		return Errors;
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

	/** 메시 이름 → 애셋. 경로(/Game/…)면 그대로. 이름이면 그 스킨 폴더에서 먼저 · 없으면 전체에서 — 같은 이름이 여럿이면 실패 (경로로 적는다). */
	UObject* FindAttachMesh(const FString& Name, const FString& PreferFolder, FString& OutWhy)
	{
		if (Name.StartsWith(TEXT("/")))
		{
			UObject* Obj = FSoftObjectPath(Name).TryLoad();
			OutWhy = Obj ? FString() : FString(TEXT("경로에 애셋이 없다"));
			return Obj;
		}
		TArray<FAssetData> Hits;
		for (UClass* C : { USkeletalMesh::StaticClass(), UStaticMesh::StaticClass() })
		{
			TArray<FAssetData> All;
			Registry().GetAssetsByClass(C->GetClassPathName(), All);
			Hits.Append(All.FilterByPredicate([&Name](const FAssetData& A) { return A.AssetName.ToString() == Name; }));
		}
		const TArray<FAssetData> Local = Hits.FilterByPredicate([&PreferFolder](const FAssetData& A)
			{ return !PreferFolder.IsEmpty() && A.PackagePath.ToString().StartsWith(PreferFolder); });
		const TArray<FAssetData>& Pick = Local.IsEmpty() ? Hits : Local;
		if (Pick.Num() == 1)
		{
			return Pick[0].GetAsset();
		}
		// ⚠ 같은 이름 다른 애셋 — 조용히 하나를 고르지 않는다 (메모리 "같은 이름 다른 내용")
		OutWhy = Pick.IsEmpty() ? FString(TEXT("이 이름의 메시가 없다"))
			: FString::Printf(TEXT("같은 이름 %d개 (%s · %s …) — 경로로 적는다"), Pick.Num(), *Pick[0].PackageName.ToString(), *Pick[1].PackageName.ToString());
		return nullptr;
	}

	/** {"Mesh","Socket","Location":[x,y,z],"Rotation":[pitch,yaw,roll],"Scale":n} 하나 */
	bool ParseAttachPiece(const TSharedPtr<FJsonObject>& J, const FString& PreferFolder, const FString& Who, FERAttachPiece& Out)
	{
		FString MeshName;
		if (!J.IsValid() || !J->TryGetStringField(TEXT("Mesh"), MeshName) || MeshName.IsEmpty())
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[연출 채우기] 부착 %s — Mesh 칸이 없다"), *Who);
			return false;
		}
		FString Why;
		Out.Mesh = FindAttachMesh(MeshName, PreferFolder, Why);
		if (!Out.Mesh)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[연출 채우기] 부착 %s — 메시 %s: %s"), *Who, *MeshName, *Why);
			return false;
		}
		FString Socket;
		Out.Socket = J->TryGetStringField(TEXT("Socket"), Socket) && !Socket.IsEmpty() ? FName(*Socket) : FName(NAME_None);
		auto Vec3 = [&J](const TCHAR* Field)
		{
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			return J->TryGetArrayField(Field, Arr) && Arr->Num() == 3
				? FVector((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber()) : FVector::ZeroVector;
		};
		const FVector Loc = Vec3(TEXT("Location"));
		const FVector Rot = Vec3(TEXT("Rotation"));
		double Scale = 1.0;
		J->TryGetNumberField(TEXT("Scale"), Scale);
		Out.Offset = FTransform(FRotator(Rot.X, Rot.Y, Rot.Z), Loc, FVector(Scale));
		return true;
	}

	/** 조각 배열 칸 → Out (실패한 조각은 빼고 에러 수를 센다) */
	void ParseAttachPieces(const TArray<TSharedPtr<FJsonValue>>& Arr, const FString& PreferFolder, const FString& Who, TArray<FERAttachPiece>& Out, int32& Errors)
	{
		for (const TSharedPtr<FJsonValue>& PJ : Arr)
		{
			FERAttachPiece Piece;
			if (ParseAttachPiece(PJ->AsObject(), PreferFolder, Who, Piece))
			{
				Out.Add(Piece);
			}
			else
			{
				++Errors;
			}
		}
	}

	/**
	 * Attach.json 의 이 캐릭터 칸 → 스킨 DA 의 WeaponMeshes · Props (Argument 64). JSON 이 원본이라 **적힌 스킨은 통째로 덮는다** (Force 무관).
	 * 형식: { "<캐릭터>": { "S0nn": { "Weapons": { "<무기 종류>": [조각…] }, "Props": { "Pres.Prop.*": { "ShowWhile": "<태그>", "Pieces": [조각…] } } } } }
	 */
	void FillAttach(const FString& Char, const TArray<TPair<FString, UERSkinData*>>& SkinAssets, const TMap<FString, FString>& SkinFolderOf)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Docs/3_EditorTasks/Data/Attach.json"));
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[연출 채우기] 부착 — %s 를 못 읽었다 (없거나 JSON 오류) · 건너뜀"), *Path);
			return;
		}
		const TSharedPtr<FJsonObject>* CharJ = nullptr;
		if (!Root->TryGetObjectField(Char, CharJ))
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기] 부착 — Attach.json 에 %s 칸 없음 · 건너뜀"), *Char);
			return;
		}
		const UEnum* WeaponEnum = StaticEnum<EERWeaponType>();
		for (const TPair<FString, TSharedPtr<FJsonValue>>& SkinKV : (*CharJ)->Values)
		{
			if (SkinKV.Key.StartsWith(TEXT("_")))
			{
				continue;   // 메모 칸
			}
			const TPair<FString, UERSkinData*>* Skin = SkinAssets.FindByPredicate([&SkinKV](const TPair<FString, UERSkinData*>& S) { return S.Key == SkinKV.Key; });
			const TSharedPtr<FJsonObject> SkinJ = SkinKV.Value->AsObject();
			if (!Skin || !SkinJ.IsValid())
			{
				UE_LOG(LogEternalReturn, Error, TEXT("[연출 채우기] 부착 %s.%s — 스킨 DA 가 없다 (Skins/ 폴더 이름에 %s)"), *Char, *SkinKV.Key, *SkinKV.Key);
				continue;
			}
			UERSkinData* DA = Skin->Value;
			const FString Folder = SkinFolderOf.FindRef(SkinKV.Key);
			DA->Modify();
			DA->WeaponMeshes.Reset();
			DA->Props.Reset();
			int32 Errors = 0;
			const TSharedPtr<FJsonObject>* WeaponsJ = nullptr;
			if (SkinJ->TryGetObjectField(TEXT("Weapons"), WeaponsJ))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& W : (*WeaponsJ)->Values)
				{
					if (W.Key.StartsWith(TEXT("_")))
					{
						continue;
					}
					const int64 Value = WeaponEnum->GetValueByNameString(W.Key);
					if (Value == INDEX_NONE)
					{
						UE_LOG(LogEternalReturn, Error, TEXT("[연출 채우기] 부착 %s — 무기 종류 %s 가 없다 (EERWeaponType 이름)"), *DA->GetName(), *W.Key);
						++Errors;
						continue;
					}
					ParseAttachPieces(W.Value->AsArray(), Folder, DA->GetName() + TEXT(".") + W.Key,
						DA->WeaponMeshes.Add(static_cast<EERWeaponType>(Value)).Pieces, Errors);
				}
			}
			const TSharedPtr<FJsonObject>* PropsJ = nullptr;
			if (SkinJ->TryGetObjectField(TEXT("Props"), PropsJ))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& P : (*PropsJ)->Values)
				{
					if (P.Key.StartsWith(TEXT("_")))
					{
						continue;
					}
					const FGameplayTag Key = FGameplayTag::RequestGameplayTag(FName(*P.Key), /*ErrorIfNotFound=*/false);
					const TSharedPtr<FJsonObject> PropJ = P.Value->AsObject();
					if (!Key.IsValid() || !PropJ.IsValid())
					{
						UE_LOG(LogEternalReturn, Error, TEXT("[연출 채우기] 부착 %s — 소품 키 %s 가 태그가 아니다 (ERGameplayTags 에 Pres.Prop.*)"), *DA->GetName(), *P.Key);
						++Errors;
						continue;
					}
					FERAttachProp& Prop = DA->Props.Add(Key);
					FString Show;
					if (PropJ->TryGetStringField(TEXT("ShowWhile"), Show) && !Show.IsEmpty())
					{
						Prop.ShowWhile = FGameplayTag::RequestGameplayTag(FName(*Show), false);
						if (!Prop.ShowWhile.IsValid())
						{
							UE_LOG(LogEternalReturn, Error, TEXT("[연출 채우기] 부착 %s.%s — ShowWhile %s 가 태그가 아니다"), *DA->GetName(), *P.Key, *Show);
							++Errors;
						}
					}
					const TArray<TSharedPtr<FJsonValue>>* PiecesJ = nullptr;
					if (PropJ->TryGetArrayField(TEXT("Pieces"), PiecesJ))
					{
						ParseAttachPieces(*PiecesJ, Folder, DA->GetName() + TEXT(".") + P.Key, Prop.Pieces, Errors);
					}
				}
			}
			DA->MarkPackageDirty();
			UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기] %s 부착 — 무기 %d · 소품 %d · 에러 %d"), *DA->GetName(), DA->WeaponMeshes.Num(), DA->Props.Num(), Errors);
		}
	}

	/** 음성 스킨 표 (Data/Presentation/VoiceSkin.csv — ER_Asset/03_Log/보이스_스킨.csv 사본): 애셋 이름(소문자) → S0nn */
	TMap<FString, FString> LoadVoiceSkins()
	{
		TMap<FString, FString> Out;
		const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Docs/3_EditorTasks/Data/Presentation/VoiceSkin.csv"));
		TArray<FString> Lines;
		if (!FFileHelper::LoadFileToStringArray(Lines, *Path))
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[연출 채우기] 음성 — %s 를 못 읽었다 · 음성 건너뜀"), *Path);
			return Out;
		}
		for (int32 i = 1; i < Lines.Num(); ++i)   // 0 = 머리줄 (캐릭터,파일,스킨,원래경로,길이차초)
		{
			TArray<FString> Cols;
			Lines[i].ParseIntoArray(Cols, TEXT(","), /*InCullEmpty=*/false);
			if (Cols.Num() >= 3)
			{
				Out.Add(FPaths::GetBaseFilename(Cols[1]).ToLower(), Cols[2].TrimStartAndEnd().ToUpper());
			}
		}
		return Out;
	}

	/**
	 * 음성 (Argument 73) — `/Game/ER/Audio/Voice/ko/<캐릭터>/` → 규칙에 맞는 상황만. S000 = 기본 DA (무기 세트) · S00x = 스킨 DA Overrides.
	 * 재생 규칙(확률 · 간격 · 끊기)은 기본 DA 의 VoiceRules 에 **통째로** (JSON 이 원본). 반환: 쓴 줄 수
	 */
	int32 FillVoice(const FString& Char, UERPresentationData& Base, const FString& Folder, const TArray<TPair<FString, UERSkinData*>>& SkinAssets, bool bForce, FFillStats& Stats)
	{
		const TArray<FAssetData> Voices = FindAssets(TEXT("/Game/ER/Audio/Voice/ko/") + Char, USoundBase::StaticClass());
		if (Voices.IsEmpty() || GVoiceRules.IsEmpty())
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기] %s 음성 — 파일 %d · 규칙 %d · 건너뜀"), *Char, Voices.Num(), GVoiceRules.Num());
			return 0;
		}
		const TMap<FString, FString> SkinOf = LoadVoiceSkins();
		using FGroupKey = TTuple<FString, EERWeaponType, FGameplayTag>;   // 스킨 · 무기 · 음성 키
		TMap<FGroupKey, TArray<UObject*>> Groups;
		TMap<FString, int32> UnusedSitu;   // 규칙 밖 상황 → 파일 수 (지역 · 로비 · 결과 …)
		int32 NoSkin = 0;
		for (const FAssetData& A : Voices)
		{
			const FString Name = A.AssetName.ToString();
			const FString* Skin = SkinOf.Find(Name.ToLower());
			TArray<FString> T;
			Name.ParseIntoArray(T, TEXT("_"));
			int32 Ko = INDEX_NONE;
			for (int32 i = T.Num() - 1; i >= 0; --i)
			{
				if (T[i].Equals(TEXT("ko"), ESearchCase::IgnoreCase)) { Ko = i; break; }
			}
			// <캐릭터> _ <상황키 (밑줄 있을 수 있음)> [_ <번호>] _ ko [_k] — 번호 없는 것도 있다 (VictoryCutscene · hacking_lost — 한 개짜리)
			if (!Skin || Ko < 2 || !T[0].Equals(Char, ESearchCase::IgnoreCase))
			{
				++NoSkin;
				Stats.Unmatched.Add(FString::Printf(TEXT("음성 %s (%s)"), *Name, Skin ? TEXT("이름 형식") : TEXT("스킨 표에 없음")));
				continue;
			}
			const int32 SituEnd = (Ko >= 3 && T[Ko - 1].IsNumeric()) ? Ko - 1 : Ko;   // 번호 토큰 앞까지
			const FString Situ = FString::Join(TArray<FString>(T.GetData() + 1, SituEnd - 1), TEXT("_"));
			const FVoiceFillRule* Hit = nullptr;
			FString Group1;
			for (const FVoiceFillRule& R : GVoiceRules)
			{
				FRegexMatcher M(FRegexPattern(R.Match), Situ);
				if (M.FindNext() && M.GetMatchBeginning() == 0 && M.GetMatchEnding() == Situ.Len())
				{
					Hit = &R;
					Group1 = M.GetCaptureGroup(1);
					break;
				}
			}
			if (!Hit)
			{
				++UnusedSitu.FindOrAdd(Situ);
				continue;
			}
			EERWeaponType Weapon = EERWeaponType::None;
			if (Hit->bWeaponFromMatch)
			{
				const FString* Alias = GVoiceWeaponAlias.Find(Group1.ToLower());
				Weapon = TokenToWeapon(Alias ? *Alias : Group1);
				if (Weapon == EERWeaponType::None)
				{
					Stats.Unmatched.Add(FString::Printf(TEXT("음성 %s (무기 '%s' 를 모른다 — VoiceWeaponAlias)"), *Name, *Group1));
					continue;
				}
			}
			Groups.FindOrAdd(FGroupKey(*Skin, Weapon, Hit->Key)).Add(A.GetAsset());
		}

		int32 Rows = 0;
		TMap<FString, int32> MissingSkinDA;
		for (const TPair<FGroupKey, TArray<UObject*>>& G : Groups)
		{
			const FString& GSkin = G.Key.Get<0>();
			const EERWeaponType GWeapon = G.Key.Get<1>();
			const FGameplayTag& GKey = G.Key.Get<2>();
			if (GSkin == TEXT("S000"))
			{
				UERPresentationData* Target = &Base;
				if (GWeapon != EERWeaponType::None)
				{
					const FString WeaponName = StaticEnum<EERWeaponType>()->GetNameStringByValue(static_cast<int64>(GWeapon));
					Target = FindOrCreate<UERPresentationData>(Folder, FString::Printf(TEXT("DA_Pres_%s_%s"), *Char, *WeaponName), Stats);
					if (Target && Base.WeaponSets.FindOrAdd(GWeapon).IsNull())
					{
						Base.WeaponSets[GWeapon] = Target;
					}
				}
				if (Target)
				{
					Target->Modify();
					Rows += WriteEntry(*Target, EERWeaponType::None, GKey, G.Value, bForce, Stats) ? 1 : 0;
					Target->MarkPackageDirty();
				}
				continue;
			}
			const TPair<FString, UERSkinData*>* SkinRow = SkinAssets.FindByPredicate([&GSkin](const TPair<FString, UERSkinData*>& S) { return S.Key == GSkin; });
			if (!SkinRow)
			{
				MissingSkinDA.FindOrAdd(GSkin) += G.Value.Num();
				continue;
			}
			SkinRow->Value->Modify();
			Rows += WriteEntry(*SkinRow->Value, GWeapon, GKey, G.Value, bForce, Stats) ? 1 : 0;
			SkinRow->Value->MarkPackageDirty();
		}

		// 재생 규칙 — JSON 이 원본이라 통째로 (Force 무관)
		Base.Modify();
		Base.VoiceRules.Reset();
		for (const FVoiceFillRule& R : GVoiceRules)
		{
			Base.VoiceRules.FindOrAdd(R.Key) = R.Rule;   // 같은 키 여러 줄(사망 묶음)이면 마지막 줄
		}
		Base.MarkPackageDirty();

		for (const TPair<FString, int32>& M : MissingSkinDA)
		{
			Stats.Unmatched.Add(FString::Printf(TEXT("음성 스킨 %s %d개 — 스킨 DA 가 없다 (Skins/ 에 %s 폴더 없음) · 그 스킨에선 기본 대사가 나온다"), *M.Key, M.Value, *M.Key));
		}
		TArray<FString> Unused;
		for (const TPair<FString, int32>& U : UnusedSitu) { Unused.Add(FString::Printf(TEXT("%s ×%d"), *U.Key, U.Value)); }
		Unused.Sort();
		UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기] %s 음성 — 파일 %d · 묶음 %d · 줄 채움 %d · 재생 규칙 %d · 이름/스킨 표 문제 %d · 규칙 밖 상황 %d종"),
			*Char, Voices.Num(), Groups.Num(), Rows, Base.VoiceRules.Num(), NoSkin, UnusedSitu.Num());
		UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기]   음성 규칙 밖 (그 기능 때): %s"), *FString::Join(Unused, TEXT(" · ")));
		return Rows;
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
		FModeGroups BaseModeGroups;                     // 기본 표에 들어갈 규칙 줄 (재키 전기톱 · 학살 · 도끼 Q)
		TSet<UObject*> ExcludedAnims;                   // 규칙이 뺀 애니 — 앞 Fill 이 넣어 둔 줄에서 지운다 (사람이 줄을 안 지워도 되게)
		for (const FAssetData& A : Anims)
		{
			const FString Name = A.AssetName.ToString();
			TArray<FString> Tokens;
			// 에디터 몽타주 AM_<시퀀스 이름> — 같은 규칙으로 읽고, 같은 키의 시퀀스를 대신한다 (아래 몽타주 우선)
			(Name.StartsWith(TEXT("AM_")) ? Name.Mid(3) : Name).ParseIntoArray(Tokens, TEXT("_"));
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
			const FString Act = FString::Join(TArray<FString>(Tokens.GetData() + 2, Tokens.Num() - 2), TEXT("_")).ToLower();
			// 규칙이 먼저 (저격 Sniperrifle_Skill_* · 재키 Saw_* …) — 모드 토큰(Saw)은 무기가 아니라 여기서만 받는다
			const FString TokenLower = Tokens[1].ToLower();
			const FModeRule* Rule = SkinId.IsEmpty() ? ModeRules().FindByPredicate([&](const FModeRule& R)
			{
				const bool bToken = R.Token == TEXT("*") ? Weapon != EERWeaponType::None : TokenLower == R.Token;
				return bToken && Act == R.Act && (R.Char.IsEmpty() || Char.Equals(R.Char, ESearchCase::IgnoreCase));
			}) : nullptr;
			if (Rule && Rule->Keys.IsEmpty())
			{
				ExcludedAnims.Add(A.GetAsset());
				Stats.Unmatched.Add(FString::Printf(TEXT("%s (규칙에서 뺌 — 쓰지 않는 애니)"), *Name));
				continue;
			}
			if (Rule)
			{
				for (const FGameplayTag& K : Rule->Keys)
				{
					if (Rule->bToBase || Weapon == EERWeaponType::None)
					{
						BaseModeGroups.FindOrAdd({ Rule->Mode, K }).Add(A.GetAsset());
					}
					else
					{
						WeaponGroups.FindOrAdd(Weapon);   // 모드 줄만 있는 무기도 세트가 생기게
						ModeGroups.FindOrAdd(Weapon).FindOrAdd({ Rule->Mode, K }).Add(A.GetAsset());
					}
				}
				if (!Rule->bAlsoNormal)
				{
					continue;
				}
			}
			if (!bCommon && Weapon == EERWeaponType::None)
			{
				Stats.Unmatched.Add(FString::Printf(TEXT("%s (토큰 %s — 모드 · 로비 · 미지원)"), *Name, *Tokens[1]));
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

		// 같은 키에 에디터 몽타주가 있으면 몽타주만 (시퀀스와 번갈아 틀지 않는다 — 야생동물 Fill 과 같은 규칙 · Argument 53 L2)
		auto PreferMontage = [](FKeyGroups& Groups)
		{
			for (TPair<TPair<EERWeaponType, FGameplayTag>, TArray<UObject*>>& G : Groups)
			{
				if (G.Value.ContainsByPredicate([](const UObject* O) { return O && O->IsA<UAnimMontage>(); }))
				{
					G.Value.RemoveAll([](const UObject* O) { return !O || !O->IsA<UAnimMontage>(); });
				}
				// 스킬 통 몽타주 (Execute 섹션 · K8) 가 있으면 그것만 — 단계별 몽타주(AM_…_Start)와 번갈아 틀지 않는다
				auto IsWhole = [](const UObject* O) { const UAnimMontage* M = Cast<UAnimMontage>(O); return M && M->IsValidSectionName(ERPresSection::Execute); };
				if (G.Value.ContainsByPredicate(IsWhole))
				{
					G.Value.RemoveAll([&IsWhole](const UObject* O) { return !IsWhole(O); });
				}
			}
		};
		PreferMontage(BaseGroups);
		// 규칙 줄도 같은 규칙 — 몽타주가 있으면 몽타주만
		auto PreferMontageMode = [](FModeGroups& Groups)
		{
			for (TPair<TPair<FGameplayTag, FGameplayTag>, TArray<UObject*>>& G : Groups)
			{
				if (G.Value.ContainsByPredicate([](const UObject* O) { return O && O->IsA<UAnimMontage>(); }))
				{
					G.Value.RemoveAll([](const UObject* O) { return !O || !O->IsA<UAnimMontage>(); });
				}
			}
		};
		PreferMontageMode(BaseModeGroups);
		for (TPair<EERWeaponType, FModeGroups>& M : ModeGroups) { PreferMontageMode(M.Value); }
		for (TPair<EERWeaponType, FKeyGroups>& W : WeaponGroups) { PreferMontage(W.Value); }
		for (TPair<FString, FKeyGroups>& S : SkinGroups) { PreferMontage(S.Value); }

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
			WriteEntry(*Base, G.Key.Key, G.Key.Value, G.Value, bForce, Stats);
		}
		// 뺀 애니가 든 옛 줄 정리 — 그 애니만 빼고, 비면 줄째 (재키 W 시전 · 옛 도끼 Q · skill03_start …)
		auto PruneExcluded = [&ExcludedAnims](TArray<FERPresentationEntry>& Entries, const UObject* Owner)
		{
			if (ExcludedAnims.IsEmpty())
			{
				return;
			}
			for (int32 i = Entries.Num() - 1; i >= 0; --i)
			{
				const int32 Removed = Entries[i].Assets.RemoveAll([&ExcludedAnims](const TObjectPtr<UObject>& O) { return ExcludedAnims.Contains(O.Get()); });
				if (Removed > 0)
				{
					UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기] %s — %s 줄에서 뺀 애니 %d개 지움%s"), *GetNameSafe(Owner), *Entries[i].Key.ToString(), Removed,
						Entries[i].Assets.IsEmpty() ? TEXT(" · 빈 줄 삭제") : TEXT(""));
					if (Entries[i].Assets.IsEmpty())
					{
						Entries.RemoveAt(i);
					}
				}
			}
		};
		for (const TPair<TPair<FGameplayTag, FGameplayTag>, TArray<UObject*>>& G : BaseModeGroups)
		{
			WriteEntry(*Base, EERWeaponType::None, G.Key.Value, G.Value, bForce, Stats, G.Key.Key);
		}
		PruneExcluded(Base->Entries, Base);
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
				WriteEntry(*Set, EERWeaponType::None, G.Key.Value, G.Value, bForce, Stats);
			}
			if (const FModeGroups* MG = ModeGroups.Find(WG.Key))
			{
				for (const TPair<TPair<FGameplayTag, FGameplayTag>, TArray<UObject*>>& G : *MG)
				{
					WriteEntry(*Set, EERWeaponType::None, G.Key.Value, G.Value, bForce, Stats, G.Key.Key);
				}
			}
			PruneExcluded(Set->Entries, Set);
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
		TMap<FString, FString> SkinFolderOf;   // S0nn → 폴더 (부착 메시를 그 스킨 폴더에서 먼저 찾는다)
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
					WriteEntry(*SkinDA, G.Key.Key, G.Key.Value, G.Value, bForce, Stats);
				}
			}
			SkinDA->MarkPackageDirty();
			SkinAssets.Add({ SkinId, SkinDA });
			SkinFolderOf.Add(SkinId, SkinFolder);
		}
		SkinAssets.Sort([](const TPair<FString, UERSkinData*>& A, const TPair<FString, UERSkinData*>& B) { return A.Key < B.Key; });   // S000 이 0 번

		// 3.2) 부착 — 손 무기 · 소품 (Argument 64 W1 · B1) · 원본 Docs/3_EditorTasks/Data/Attach.json
		FillAttach(Char, SkinAssets, SkinFolderOf);

		// 3.5) 소리 (F12.5-05 · Argument 49) — Character_FX/<캐릭터>/s000 = 이 캐릭터 전용 → 무기 세트 · s00x = 스킨 전용 → 스킨 DA (무기 칸 채움)
		//   원작 3층: 무기 기본(SFX/Attack … — FillWeapon) < 캐릭터(s000) < 스킨(s00x). 스킨은 무기 기본과 **같은 파일명 · 다른 내용** (사운드_분류.md)
		int32 SoundRows = 0;
		{
			TArray<FString> FxFolders;
			Registry().GetSubPaths(TEXT("/Game/ER/Audio/SFX/Character_FX/") + Char.ToLower(), FxFolders, /*bRecurse=*/false);
			// 모션 소리 바꿈표의 기준 — S000 소리를 이름으로 (Argument 59 N2)
			TMap<FName, FAssetData> BaseSounds;
			for (const FString& FxFolder : FxFolders)
			{
				if (AsSkinId(FPaths::GetCleanFilename(FxFolder)) == TEXT("S000"))
				{
					for (const FAssetData& A : FindAssets(FxFolder, USoundBase::StaticClass())) { BaseSounds.Add(A.AssetName, A); }
				}
			}
			for (const FString& FxFolder : FxFolders)
			{
				const FString SkinId = AsSkinId(FPaths::GetCleanFilename(FxFolder));
				if (SkinId.IsEmpty())
				{
					continue;
				}
				const TArray<FAssetData> FolderSounds = FindAssets(FxFolder, USoundBase::StaticClass());
				// 스킨 폴더 — 같은 파일명의 S000 소리와 짝 → 스킨 DA SoundSwaps (노티파이 소리를 스킨이 바꾼다 · Argument 59 N2). 키 규칙과 무관하게 전부
				if (SkinId != TEXT("S000"))
				{
					if (const TPair<FString, UERSkinData*>* SwapSkin = SkinAssets.FindByPredicate([&SkinId](const TPair<FString, UERSkinData*>& S) { return S.Key == SkinId; }))
					{
						UERSkinData* SD = SwapSkin->Value;
						SD->Modify();
						if (bForce) { SD->SoundSwaps.Reset(); }
						int32 Pairs = 0, NoPair = 0;
						for (const FAssetData& A : FolderSounds)
						{
							const FAssetData* BaseA = BaseSounds.Find(A.AssetName);
							USoundBase* From = BaseA ? Cast<USoundBase>(BaseA->GetAsset()) : nullptr;
							USoundBase* To = Cast<USoundBase>(A.GetAsset());
							if (From && To && From != To) { SD->SoundSwaps.Add(From, To); ++Pairs; } else { ++NoPair; }
						}
						SD->MarkPackageDirty();
						UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기] %s 소리 바꿈 %d쌍 (S000 에 같은 이름 없음 %d)"), *SD->GetName(), Pairs, NoPair);
					}
				}
				// 명시 표 먼저 (배치표의 거울) — 나머지만 이름 규칙으로
				struct FExplicitRow { EERWeaponType Weapon; FGameplayTag Mode; FGameplayTag Key; TArray<UObject*> Assets; };
				TArray<FExplicitRow> Explicit;
				TArray<FAssetData> RuleSounds;
				for (const FAssetData& A : FolderSounds)
				{
					const FString Lower = A.AssetName.ToString().ToLower();
					const FCharSound* CS = CharSoundRules().FindByPredicate([&Lower](const FCharSound& C) { return Lower == C.Name; });
					if (!CS)
					{
						RuleSounds.Add(A);
						continue;
					}
					if (!CS->Key.IsValid())
					{
						Stats.Unmatched.Add(FString::Printf(TEXT("소리 %s (%s · 배치표: 안 씀 · 노티파이)"), *A.AssetName.ToString(), *SkinId));
						continue;
					}
					FExplicitRow* Row = Explicit.FindByPredicate([CS](const FExplicitRow& E) { return E.Weapon == CS->Weapon && E.Mode == CS->Mode && E.Key == CS->Key; });
					if (!Row)
					{
						Row = &Explicit.Add_GetRef({ CS->Weapon, CS->Mode, CS->Key, {} });
					}
					Row->Assets.Add(A.GetAsset());
				}
				for (const FExplicitRow& E : Explicit)
				{
					if (SkinId == TEXT("S000"))
					{
						UERPresentationData* Target = Base;
						if (E.Weapon != EERWeaponType::None)
						{
							const FString WeaponName = StaticEnum<EERWeaponType>()->GetNameStringByValue(static_cast<int64>(E.Weapon));
							Target = FindOrCreate<UERPresentationData>(Folder, FString::Printf(TEXT("DA_Pres_%s_%s"), *Char, *WeaponName), Stats);
							if (Target && Base->WeaponSets.FindOrAdd(E.Weapon).IsNull())
							{
								Base->WeaponSets[E.Weapon] = Target;
							}
						}
						if (Target)
						{
							Target->Modify();
							SoundRows += WriteEntry(*Target, EERWeaponType::None, E.Key, E.Assets, bForce, Stats, E.Mode) ? 1 : 0;
							Target->MarkPackageDirty();
						}
					}
					else if (const TPair<FString, UERSkinData*>* SkinRow = SkinAssets.FindByPredicate([&SkinId](const TPair<FString, UERSkinData*>& S) { return S.Key == SkinId; }))
					{
						SkinRow->Value->Modify();
						SoundRows += WriteEntry(*SkinRow->Value, E.Weapon, E.Key, E.Assets, bForce, Stats, E.Mode) ? 1 : 0;
						SkinRow->Value->MarkPackageDirty();
					}
				}
				const FSoundGroups Groups = GroupSounds(RuleSounds, Stats, *SkinId);
				if (Groups.IsEmpty())
				{
					continue;
				}
				if (SkinId == TEXT("S000"))
				{
					for (const TPair<TPair<EERWeaponType, FGameplayTag>, TArray<UObject*>>& G : Groups)
					{
						if (G.Key.Key == EERWeaponType::None)
						{
							// 무기 무관 (캐릭터 스킬 소리 · F19-02) → 캐릭터 기본 표
							Base->Modify();
							SoundRows += WriteEntry(*Base, EERWeaponType::None, G.Key.Value, G.Value, bForce, Stats) ? 1 : 0;
							Base->MarkPackageDirty();
							continue;
						}
						const FString WeaponName = StaticEnum<EERWeaponType>()->GetNameStringByValue(static_cast<int64>(G.Key.Key));
						UERPresentationData* Set = FindOrCreate<UERPresentationData>(Folder, FString::Printf(TEXT("DA_Pres_%s_%s"), *Char, *WeaponName), Stats);
						if (!Set)
						{
							continue;
						}
						Set->Modify();
						SoundRows += WriteEntry(*Set, EERWeaponType::None, G.Key.Value, G.Value, bForce, Stats) ? 1 : 0;
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
					SoundRows += WriteEntry(*Skin->Value, G.Key.Key, G.Key.Value, G.Value, bForce, Stats) ? 1 : 0;
				}
				Skin->Value->MarkPackageDirty();
			}
		}

		// 3.7) 음성 (Argument 73) — Voice/ko/<캐릭터> · 규칙 Data/Presentation/_Voice.json · 스킨 VoiceSkin.csv
		const int32 VoiceRows = FillVoice(Char, *Base, Folder, SkinAssets, bForce, Stats);

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

		TidyAll(Stats);   // 애니 · 소리 · 음성 칸 정리 · 정렬 (Argument 74)
		for (const FString& U : Stats.Unmatched)
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기]   규칙 밖: %s"), *U);
		}
		UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기] %s — 애니 %d · 새 애셋 %d · 줄 채움 %d (소리 %d · 음성 %d) · 유지 %d · 무기 세트 %d · 스킨 %d · 규칙 밖 %d. **Save All**"),
			*Char, Anims.Num(), Stats.Created, Stats.Written, SoundRows, VoiceRows, Stats.Kept, WeaponGroups.Num(), SkinAssets.Num(), Stats.Unmatched.Num());
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
				// F12.6-04 동물 스킬 = Q 슬롯 (DA SlotTag Ability.Slot.Q). 차징 → 돌진 두 단계면 두 번째가 판정 순간(Execute)
				const FGameplayTag Key = (N.EndsWith(TEXT("_skill01")) || N.EndsWith(TEXT("_skill01_ready"))) ? ERTags::Ability_Slot_Q
					: (N.EndsWith(TEXT("_skill01_assault")) || N.EndsWith(TEXT("_skill01_atk"))) ? ERTags::Ability_Slot_Q_Execute   // 알파 · 오메가 아크 블레이드 휘두르기 (F12.6-05)
					: N.EndsWith(TEXT("_skill02")) ? ERTags::Ability_Slot_W                                                          // 오메가 VF 방출
					// F12.6-06a 위클라인 (사용자 확인 2026-10-01) — 01 신경 가스 · 02 트리플렛 (start → shot) · 04 격리 (start → end). 통제 · 유해 물질은 애니 없음
					//   04 충전 반복은 에디터 몽타주 AM_Wickline_01_ActiveSkill04_start (Start → Loop 섹션) 가 같은 키로 잡혀 _start 를 대신한다 (Argument 53 L2)
					: N.EndsWith(TEXT("_activeskill01")) ? ERTags::Ability_Slot_Q
					: N.EndsWith(TEXT("_activeskill02_start")) ? ERTags::Ability_Slot_W
					: N.EndsWith(TEXT("_activeskill02_shot")) ? ERTags::Ability_Slot_W_Execute
					: N.EndsWith(TEXT("_activeskill04_start")) ? ERTags::Ability_Slot_R
					: N.EndsWith(TEXT("_activeskill04_end")) ? ERTags::Ability_Slot_R_Execute
					: (N.EndsWith(TEXT("_atk01")) || N.EndsWith(TEXT("_atk02"))) ? ERTags::Ability_Slot_Attack
					: N.EndsWith(TEXT("_death")) ? ERTags::Pres_Anim_Death
					: N.EndsWith(TEXT("_appear")) ? ERTags::Pres_Anim_Appear          // F12.6-01 (appear_idle 은 아니다)
					: N.EndsWith(TEXT("_endbattle")) ? ERTags::Pres_Anim_EndBattle
					// F12.6-02 경계 · 잠 (beware_loop_wait 는 아니다 — 들개만 있고 뜻 미확인)
					: N.EndsWith(TEXT("_beware_start")) ? ERTags::Pres_Anim_BewareStart
					: N.EndsWith(TEXT("_beware_loop")) ? ERTags::Pres_Anim_BewareLoop
					: N.EndsWith(TEXT("_beware_end")) ? ERTags::Pres_Anim_BewareEnd
					: N.EndsWith(TEXT("_sleep_start")) ? ERTags::Pres_Anim_SleepStart
					: N.EndsWith(TEXT("_sleep")) ? ERTags::Pres_Anim_SleepLoop
					: N.EndsWith(TEXT("_wake")) ? ERTags::Pres_Anim_Wake : FGameplayTag();
				if (Key.IsValid())
				{
					Groups.FindOrAdd({ EERWeaponType::None, Key }).Add(Anim.GetAsset());
				}
			}
			// Argument 53 L2 — 같은 키에 에디터 몽타주가 있으면 몽타주만 (시퀀스와 번갈아 틀지 않는다)
			for (TPair<TPair<EERWeaponType, FGameplayTag>, TArray<UObject*>>& G : Groups)
			{
				if (G.Value.ContainsByPredicate([](const UObject* O) { return O && O->IsA<UAnimMontage>(); }))
				{
					G.Value.RemoveAll([](const UObject* O) { return !O || !O->IsA<UAnimMontage>(); });
				}
				// 스킬 통 몽타주 (Execute 섹션 · K8) 가 있으면 그것만 — 단계별 몽타주(AM_…_Start)와 번갈아 틀지 않는다
				auto IsWhole = [](const UObject* O) { const UAnimMontage* M = Cast<UAnimMontage>(O); return M && M->IsValidSectionName(ERPresSection::Execute); };
				if (G.Value.ContainsByPredicate(IsWhole))
				{
					G.Value.RemoveAll([&IsWhole](const UObject* O) { return !IsWhole(O); });
				}
			}
			if (Groups.IsEmpty())
			{
				Stats.Unmatched.Add(FString::Printf(TEXT("%s (%s 에 atk · death 애니 없음 — 소리만 채운다)"), *A.AssetName.ToString(), *WildFolder));
			}
			// 소리 (F12.5-05 · Argument 49) — 원작 이름이 종마다 제각각이라 **종 표**로. 변이 · 잠식도 같은 종 소리. 접두어로 찾는다 (atk01 · atk02 · r1 · r2 가 변형)
			//   ⚠ "<종>Hit" 이 "그 종의 공격이 맞은 소리" 인지 "그 종이 맞은 소리" 인지 (미확인) — 무기 hit<무기> 와 같은 뜻으로 보고 **공격 타격음**에 둔다
			{
				struct FWildSfx { const TCHAR* Attack; const TCHAR* Hit; const TCHAR* Die; const TCHAR* Discover = nullptr; const TCHAR* Appear = nullptr; const TCHAR* Beware = nullptr; };
					// Beware (F12.6-02 경계 들어갈 때) = <종>WakeUp_Ing — 뜻 (미확인) · 사용자가 들어보고 정한다. 멧돼지 없음
					// Discover (F12.6-01 발견음) = <종>WakeUp_Start — 닭은 _Ing 뿐 · 멧돼지 없음. ready_bear · ready_wolf 는 뜻 (미확인) → 02 에서. Appear = 등장음 (보스만 원본에 있다)
				static const TMap<EERWildlifeType, FWildSfx> SfxOf = {
					{ EERWildlifeType::Chicken,   { TEXT("chickenAttack"),   TEXT("chickenHit"),          TEXT("chickenDie"), nullptr, nullptr, TEXT("chickenWakeUp_Ing") } },
					{ EERWildlifeType::Bat,       { TEXT("batAttack"),       TEXT("batHit"),              TEXT("batDie"), TEXT("batWakeUp_Start"), nullptr, TEXT("batWakeUp_Ing") } },
					{ EERWildlifeType::Boar,      { TEXT("boar_attack"),     TEXT("boarHit"),             TEXT("boarDie") } },   // boarAttack 아님 — 사용자가 들어보고 고름 (2026-09-29)
					{ EERWildlifeType::WildDog,   { TEXT("wildDogAttack"),   TEXT("wildDogHit"),          TEXT("wildDogDie"), TEXT("wildDogWakeUp_Start"), nullptr, TEXT("wildDogWakeUp_Ing") } },
					{ EERWildlifeType::Wolf,      { TEXT("wolfAttack"),      TEXT("wolfHit"),             TEXT("wolfDie"), TEXT("wolfWakeUp_Start"), nullptr, TEXT("wolfWakeUp_Ing") } },
					{ EERWildlifeType::Bear,      { TEXT("bearAttack"),      TEXT("bearHit"),             TEXT("bearDie"), TEXT("bearWakeUp_Start"), nullptr, TEXT("bearWakeUp_Ing") } },
					{ EERWildlifeType::Alpha,     { TEXT("AlphaOmega_atk0"), TEXT("AlphaOmega_atk_hit"),  TEXT("AlphaOmega_dead"), nullptr, TEXT("AlphaOmega_appear") } },
					{ EERWildlifeType::Omega,     { TEXT("AlphaOmega_atk0"), TEXT("AlphaOmega_atk_hit"),  TEXT("AlphaOmega_dead"), nullptr, TEXT("AlphaOmega_appear") } },
					{ EERWildlifeType::Wickeline, { TEXT("wicklineAttack"),  nullptr,                     TEXT("wicklineDie"), TEXT("Wickline_TrackingStart") } },   // 발견음 자리 = 추적 시작 (F12.6-06 · 추적은 연출상 전투)
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
					AddSound(Sfx->Discover, ERTags::Pres_Sfx_Discover);
					AddSound(Sfx->Appear, ERTags::Pres_Sfx_Appear);
					AddSound(Sfx->Beware, ERTags::Pres_Sfx_Beware);
				}
				// 동물 스킬 소리 (F12.6-04) — 시전음 · 타격음. 들개는 원본에 없다. 멧돼지 boar_Skill_Attack · 곰 bear_Skill_Impact 는 뜻 (미확인) → 빼둔다
				{
					struct FSkillSfx { const TCHAR* Cast; const TCHAR* Hit; };
					static const TMap<EERWildlifeType, FSkillSfx> SkillSfxOf = {
						{ EERWildlifeType::Boar, { TEXT("boar_Skill_Activation"), TEXT("boar_Skill_Hit") } },
						{ EERWildlifeType::Bear, { TEXT("bear_Skill_Activation"), TEXT("bear_Skill_Hit") } },
						{ EERWildlifeType::Wolf, { TEXT("wolf_Skill_Activation"), nullptr } },
					};
					if (const FSkillSfx* SS = SkillSfxOf.Find(D->Type))
					{
						AddSound(SS->Cast, ERTags::Pres_Sfx_SkillCast);
						AddSound(SS->Hit, ERTags::Pres_Sfx_SkillHit);
					}
					// 알파 · 오메가 (F12.6-05) — 스킬이 둘이라 **슬롯별** 키. 알파는 Q 만 쓰지만 같은 소리 파일이라 W 줄이 있어도 무해
					//   시전음 큐는 **판정 순간**에 온다 (Argument 49) → 휘두르기 `_skill01_Swing` · 분출 `_skill02_Spout` 가 맞다.
					//   충전 소리 `_skill01_Start` · `_skill02_Ready` 는 선딜 시작 시점 큐가 없어 빼둔다 (미사용)
					// 위클라인 (F12.6-06) — 슬롯 = Q 가스 · W 트리플렛 · E 통제 · R 격리. 파일 이름으로 **추정** 연결 (뜻 미확인 · 들어보고 바꾼다)
					if (D->Type == EERWildlifeType::Wickeline)
					{
						AddSound(TEXT("Wickline_GasDispersionStart"), ERTags::Pres_Sfx_SkillCast_Q);
						AddSound(TEXT("wickline_Skill02_Activation"), ERTags::Pres_Sfx_SkillCast_W);
						AddSound(TEXT("wickline_Skill02_Hit"), ERTags::Pres_Sfx_SkillHit_W);
						AddSound(TEXT("Wickline_QuickMove"), ERTags::Pres_Sfx_SkillCast_E);
						AddSound(TEXT("Wickline_KnockbackStart"), ERTags::Pres_Sfx_SkillCast_R);
						AddSound(TEXT("Wickline_KnockbackHit"), ERTags::Pres_Sfx_SkillHit_R);
					}
					if (D->Type == EERWildlifeType::Alpha || D->Type == EERWildlifeType::Omega)
					{
						AddSound(TEXT("AlphaOmega_skill01_Swing"), ERTags::Pres_Sfx_SkillCast_Q);
						AddSound(TEXT("AlphaOmega_skill01_Hit"), ERTags::Pres_Sfx_SkillHit_Q);
						AddSound(TEXT("AlphaOmega_skill02_Spout"), ERTags::Pres_Sfx_SkillCast_W);
						AddSound(TEXT("AlphaOmega_skill02_Hit"), ERTags::Pres_Sfx_SkillHit_W);
					}
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
				WriteEntry(*Pres, EERWeaponType::None, G.Key.Value, G.Value, bForce, Stats);
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
		TidyAll(Stats);   // 애니 · 소리 · 음성 칸 정리 · 정렬 (Argument 74)
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
			if (WriteEntry(*DA, EERWeaponType::None, P.Key.Value, P.Value, bForce, Stats))
			{
				DA->MarkPackageDirty();
			}
			Made.FindOrAdd(P.Key.Key).Add(FString::Printf(TEXT("%s %d"), *P.Key.Value.ToString().RightChop(9), P.Value.Num()));   // "Pres.Sfx." 떼고
		}
		for (const TPair<EERWeaponType, TArray<FString>>& M : Made)
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[연출 채우기]   무기 공통 %s — %s"), *StaticEnum<EERWeaponType>()->GetNameStringByValue(static_cast<int64>(M.Key)), *FString::Join(M.Value, TEXT(" · ")));
		}
		TidyAll(Stats);   // 애니 · 소리 · 음성 칸 정리 · 정렬 (Argument 74)
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
		LoadFillRules();   // 규칙 데이터 (Argument 72 A1) — 매번 다시 읽는다
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
