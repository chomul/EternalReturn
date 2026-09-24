// Copyright Epic Games, Inc. All Rights Reserved.
//
// [임시 · 에디터] F12-01 — Docs/3_EditorTasks/Data/DT_Wildlife.csv 의 행을 DA_Wild_<이름> 애셋으로 만든다 (Argument 30 B).
//   17개 애셋을 손으로 찍는 대신 명령 한 번. 사용자가 에디터에서 실행 · Save All. 검증 뒤 지운다 (F11.5 이관 명령과 같은 방식).
//   ⚠ 이미 있는 애셋은 건너뛴다 (값을 손으로 고쳤을 수 있다). `Force` 를 붙이면 **CSV 가 가진 필드만** 덮어쓴다 (연출 · 스킬 유지).
// [임시 · 에디터] ER.Wild.FitHitBox [이름] — DA.Mesh 의 레퍼런스 포즈 바운드로 DA.HitBoxExtent 를 채운다 (사용자 2026-09-22 "B"). 메시를 바꾸면 다시 실행.

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "EternalReturn.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Wildlife/ERWildlifeCharacter.h"
#include "Wildlife/ERWildlifeData.h"
#include "Wildlife/ERWildlifeSettings.h"

namespace
{
	/** 따옴표 안의 쉼표를 지키는 CSV 한 줄 분리. "" 는 " 로. */
	TArray<FString> SplitCsvLine(const FString& Line)
	{
		TArray<FString> Out;
		FString Cur;
		bool bQuoted = false;
		for (int32 i = 0; i < Line.Len(); ++i)
		{
			const TCHAR C = Line[i];
			if (bQuoted)
			{
				if (C == TEXT('"'))
				{
					if (i + 1 < Line.Len() && Line[i + 1] == TEXT('"')) { Cur.AppendChar(TEXT('"')); ++i; }
					else { bQuoted = false; }
				}
				else { Cur.AppendChar(C); }
			}
			else if (C == TEXT('"')) { bQuoted = true; }
			else if (C == TEXT(',')) { Out.Add(Cur); Cur.Reset(); }
			else { Cur.AppendChar(C); }
		}
		Out.Add(Cur);
		return Out;
	}

	/** 애셋 레지스트리에서 이름이 같은 UERWildlifeData 를 폴더와 무관하게 찾는다. 둘 이상이면 경고하고 첫 번째. */
	UERWildlifeData* FindExistingByName(const FString& AssetName)
	{
		TArray<FAssetData> Assets;
		FAssetRegistryModule::GetRegistry().GetAssetsByClass(UERWildlifeData::StaticClass()->GetClassPathName(), Assets);
		TArray<FAssetData> Matches = Assets.FilterByPredicate([&AssetName](const FAssetData& A) { return A.AssetName.ToString() == AssetName; });
		if (Matches.Num() > 1)
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[야생동물 임포트] %s 가 %d곳에 있다 — %s 를 쓴다. 중복을 지워라."), *AssetName, Matches.Num(), *Matches[0].PackageName.ToString());
		}
		return Matches.IsEmpty() ? nullptr : Cast<UERWildlifeData>(Matches[0].GetAsset());
	}

	/** CSV 가 가진 필드만 DA 에 쓴다 (연출 · 스킬은 손대지 않는다 — Force 로 덮어쓸 때 날아가면 안 된다). */
	void ApplyCsvRow(UERWildlifeData* D, const TArray<FString>& F, int32 CType, int32 CMut, int32 CBoss, int32 CLoot, int32 CExp, int32 CTier, int32 CStats, int32 CGrow, int32 CExpLv, int32 CCredit,
		int32 CVar, int32 CBaseLv, int32 CDay, int32 CNight, int32 CTimer, int32 CResp, int32 CRespSec, int32 CResist, int32 CHeal, int32 CSpawnAggro, int32 CTeamCredit)
	{
		const int64 TypeValue = StaticEnum<EERWildlifeType>()->GetValueByNameString(F[CType].TrimStartAndEnd());
		D->Type = TypeValue == INDEX_NONE ? EERWildlifeType::None : static_cast<EERWildlifeType>(TypeValue);
		// Variant 열 (Normal · Mutant · Corrupted). 예전 CSV 의 bMutant 열도 받는다.
		if (CVar >= 0)
		{
			const int64 V = StaticEnum<EERWildlifeVariant>()->GetValueByNameString(F[CVar].TrimStartAndEnd());
			D->Variant = V == INDEX_NONE ? EERWildlifeVariant::Normal : static_cast<EERWildlifeVariant>(V);
		}
		else
		{
			D->Variant = (CMut >= 0 && F[CMut].TrimStartAndEnd().ToBool()) ? EERWildlifeVariant::Mutant : EERWildlifeVariant::Normal;
		}
		auto Num = [&F](int32 C, float Def) { return C >= 0 ? FCString::Atof(*F[C]) : Def; };
		D->BaseLevel = FMath::Max(1, static_cast<int32>(Num(CBaseLv, 1.f)));
		D->FirstSpawnDay = FMath::Max(1, static_cast<int32>(Num(CDay, 1.f)));
		D->bFirstSpawnNight = CNight >= 0 && F[CNight].TrimStartAndEnd().ToBool();
		D->FirstSpawnTimer = Num(CTimer, 999.f);
		if (CResp >= 0)
		{
			const int64 R = StaticEnum<EERWildlifeRespawn>()->GetValueByNameString(F[CResp].TrimStartAndEnd());
			D->RespawnMode = R == INDEX_NONE ? EERWildlifeRespawn::Interval : static_cast<EERWildlifeRespawn>(R);
		}
		D->RespawnSeconds = Num(CRespSec, 120.f);
		// F12-05 보스 열 (없으면 기본값)
		D->ProportionalDamageResist = FMath::Clamp(Num(CResist, 0.f), 0.f, 1.f);
		D->bHealOnReturn = CHeal < 0 || F[CHeal].TrimStartAndEnd().ToBool();
		D->SpawnAggroRadius = Num(CSpawnAggro, 0.f);
		D->bBoss = CBoss >= 0 && F[CBoss].TrimStartAndEnd().ToBool();
		D->LootRow = CLoot >= 0 ? FName(*F[CLoot].TrimStartAndEnd()) : NAME_None;
		D->HuntExpBase = CExp >= 0 ? FCString::Atof(*F[CExp]) : 0.f;
		D->HuntExpPerLevel = CExpLv >= 0 ? FCString::Atof(*F[CExpLv]) : 0.f;
		D->Credit = CCredit >= 0 ? FCString::Atoi(*F[CCredit]) : 0;
		D->TeamCredit = CTeamCredit >= 0 ? FCString::Atoi(*F[CTeamCredit]) : 0;
		D->Tier = CTier >= 0 ? FCString::Atoi(*F[CTier]) : 1;
		FERCharStats::StaticStruct()->ImportText(*F[CStats], &D->BaseStats, nullptr, PPF_None, GLog, TEXT("DT_Wildlife.BaseStats"));
		FERCharStatGrowth::StaticStruct()->ImportText(*F[CGrow], &D->PerLevel, nullptr, PPF_None, GLog, TEXT("DT_Wildlife.PerLevel"));
	}

	void ImportCsv(const TArray<FString>& Args)
	{
		// 인자: [csv 경로] [Force]. Force = 이미 있는 애셋의 **CSV 필드만** 덮어쓴다 (Mesh · AnimClass · MeshScale · HitBox · Skills 는 유지).
		bool bForce = false;
		FString Path;
		for (const FString& A : Args)
		{
			if (A.Equals(TEXT("Force"), ESearchCase::IgnoreCase)) { bForce = true; }
			else if (Path.IsEmpty()) { Path = A; }
		}
		if (Path.IsEmpty()) { Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Docs/3_EditorTasks/Data/DT_Wildlife.csv")); }
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[야생동물 임포트] CSV 를 못 읽었다: %s"), *Path);
			return;
		}
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		if (Lines.Num() < 2)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[야생동물 임포트] 행이 없다."));
			return;
		}
		const TArray<FString> Header = SplitCsvLine(Lines[0].TrimStartAndEnd());
		auto Col = [&Header](const TCHAR* Name) { return Header.IndexOfByPredicate([Name](const FString& H) { return H.TrimStartAndEnd().Equals(Name, ESearchCase::IgnoreCase); }); };
		const int32 CType = Col(TEXT("Type")), CMut = Col(TEXT("bMutant")), CBoss = Col(TEXT("bBoss")), CStats = Col(TEXT("BaseStats")),
			CGrow = Col(TEXT("PerLevel")), CLoot = Col(TEXT("LootRow")), CExp = Col(TEXT("HuntExpBase")), CTier = Col(TEXT("Tier")),
			CExpLv = Col(TEXT("HuntExpPerLevel")), CCredit = Col(TEXT("Credit")),
			CVar = Col(TEXT("Variant")), CBaseLv = Col(TEXT("BaseLevel")), CDay = Col(TEXT("FirstSpawnDay")), CNight = Col(TEXT("bFirstSpawnNight")),
			CTimer = Col(TEXT("FirstSpawnTimer")), CResp = Col(TEXT("RespawnMode")), CRespSec = Col(TEXT("RespawnSeconds")),
			CResist = Col(TEXT("ProportionalDamageResist")), CHeal = Col(TEXT("bHealOnReturn")), CSpawnAggro = Col(TEXT("SpawnAggroRadius")), CTeamCredit = Col(TEXT("TeamCredit"));
		if (CType < 0 || CStats < 0 || CGrow < 0)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[야생동물 임포트] 헤더에 Type · BaseStats · PerLevel 이 있어야 한다: %s"), *Lines[0]);
			return;
		}

		const FString Dir = UERWildlifeSettings::Get().DataPath;
		int32 Made = 0, Updated = 0, Skipped = 0;
		for (int32 L = 1; L < Lines.Num(); ++L)
		{
			const TArray<FString> F = SplitCsvLine(Lines[L].TrimStartAndEnd());
			if (F.Num() < Header.Num() || F[0].IsEmpty()) { continue; }
			const FString Name = F[0].TrimStartAndEnd();
			const FString AssetName = TEXT("DA_Wild_") + Name;
			const FString PackagePath = Dir / AssetName;

			// ⭐ 이름으로 **어디에 있든** 찾는다 — 폴더를 옮기면 경로 조회가 실패해서 같은 이름의 빈 DA 를 또 만들었다 (2026-09-24 WildData 이동 사고).
			UERWildlifeData* Found = FindExistingByName(AssetName);
			if (!Found)
			{
				Found = LoadObject<UERWildlifeData>(nullptr, *(PackagePath + TEXT(".") + AssetName), nullptr, LOAD_NoWarn);
			}
			if (UERWildlifeData* Exist = Found)
			{
				if (!bForce)
				{
					UE_LOG(LogEternalReturn, Log, TEXT("[야생동물 임포트] %s — 이미 있음, 건너뜀 (덮어쓰려면 ER.Wild.ImportCSV Force)"), *AssetName);
					++Skipped;
					continue;
				}
				Exist->Modify();
				ApplyCsvRow(Exist, F, CType, CMut, CBoss, CLoot, CExp, CTier, CStats, CGrow, CExpLv, CCredit, CVar, CBaseLv, CDay, CNight, CTimer, CResp, CRespSec, CResist, CHeal, CSpawnAggro, CTeamCredit);
				Exist->MarkPackageDirty();
				++Updated;
				UE_LOG(LogEternalReturn, Log, TEXT("[야생동물 임포트] %s 덮어씀 — 체력 %.0f · 공격력 %.0f (+%.0f/Lv) · 루트 %s · 사냥 숙련도 %.0f (연출 · 스킬 유지)"),
					*AssetName, Exist->BaseStats.MaxHP, Exist->BaseStats.AttackPower, Exist->PerLevel.AttackPowerPerLevel, *Exist->LootRow.ToString(), Exist->HuntExpBase);
				continue;
			}
			UPackage* Package = CreatePackage(*PackagePath);
			UERWildlifeData* D = NewObject<UERWildlifeData>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);

			ApplyCsvRow(D, F, CType, CMut, CBoss, CLoot, CExp, CTier, CStats, CGrow, CExpLv, CCredit, CVar, CBaseLv, CDay, CNight, CTimer, CResp, CRespSec, CResist, CHeal, CSpawnAggro, CTeamCredit);

			FAssetRegistryModule::AssetCreated(D);
			Package->MarkPackageDirty();
			++Made;
			UE_LOG(LogEternalReturn, Log, TEXT("[야생동물 임포트] %s: %s%s%s 체력 %.0f · 공격력 %.0f (+%.0f/Lv) · 루트 %s · 사냥 숙련도 %.0f"),
				*AssetName, *UEnum::GetDisplayValueAsText(D->Type).ToString(), D->Variant != EERWildlifeVariant::Normal ? TEXT(" 변이") : TEXT(""), D->bBoss ? TEXT(" 보스") : TEXT(""),
				D->BaseStats.MaxHP, D->BaseStats.AttackPower, D->PerLevel.AttackPowerPerLevel, *D->LootRow.ToString(), D->HuntExpBase);
		}
		UE_LOG(LogEternalReturn, Log, TEXT("[야생동물 임포트] 끝 — 새로 %d · 덮어씀 %d · 건너뜀 %d. **Save All (Ctrl+Shift+S)** 로 저장하라."), Made, Updated, Skipped);
	}

	/**
	 * DA 한 개의 HitBoxExtent · HitBoxOffset 을 메시 바운드로. 반환: 바꿨으면 true.
	 * ⭐ 바운드는 **메시 로컬** 이다. BP_ERWildlife 의 Mesh 컴포넌트가 yaw −90 으로 세워져 있어(언리얼 규약) 그대로 넣으면 박스가 90° 돌아간다 (2026-09-23 로그: 늑대 Y 147).
	 *   → WildlifeClass CDO 의 Mesh 상대 회전을 읽어 액터 공간으로 돌린다. 위치도 ApplyVisuals 와 같게 (메시 Z = −캡슐 반높이 · MeshScale).
	 */
	bool FitHitBox(UERWildlifeData* D, const FRotator& MeshRot, float HalfHeight)
	{
		USkeletalMesh* Mesh = D->Mesh.IsNull() ? nullptr : D->Mesh.LoadSynchronous();
		if (!Mesh)
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[히트박스 맞춤] %s — Mesh 가 비어 있다. DA 의 연출 › Mesh 를 먼저 채워라."), *D->GetName());
			return false;
		}
		// 레퍼런스 포즈 AABB (메시 로컬) → 스케일 → 회전 → 메시 상대 위치(발끝을 캡슐 바닥에).
		const FBoxSphereBounds B = Mesh->GetImportedBounds();
		const FVector LocalExtent = B.BoxExtent * D->MeshScale;
		const FVector LocalOrigin = B.Origin * D->MeshScale;
		const FMatrix Rot = FRotationMatrix(MeshRot);
		// 회전된 AABB 의 반크기 = |회전행렬| × 반크기 (축마다 절댓값 합).
		const FVector Extent(
			FMath::Abs(Rot.M[0][0]) * LocalExtent.X + FMath::Abs(Rot.M[1][0]) * LocalExtent.Y + FMath::Abs(Rot.M[2][0]) * LocalExtent.Z,
			FMath::Abs(Rot.M[0][1]) * LocalExtent.X + FMath::Abs(Rot.M[1][1]) * LocalExtent.Y + FMath::Abs(Rot.M[2][1]) * LocalExtent.Z,
			FMath::Abs(Rot.M[0][2]) * LocalExtent.X + FMath::Abs(Rot.M[1][2]) * LocalExtent.Y + FMath::Abs(Rot.M[2][2]) * LocalExtent.Z);
		const FVector Offset = MeshRot.RotateVector(LocalOrigin) - FVector(0.f, 0.f, HalfHeight);

		D->Modify();
		D->HitBoxExtent = Extent;
		D->HitBoxOffset = Offset;
		D->MarkPackageDirty();
		UE_LOG(LogEternalReturn, Log, TEXT("[히트박스 맞춤] %s: %s → 반크기 (%.0f, %.0f, %.0f) · 중심 (%.0f, %.0f, %.0f) (스케일 %.2f)"),
			*D->GetName(), *Mesh->GetName(), Extent.X, Extent.Y, Extent.Z, Offset.X, Offset.Y, Offset.Z, D->MeshScale);
		return true;
	}

	void FitHitBoxCmd(const TArray<FString>& Args)
	{
		// BP 기본값 — 메시 상대 회전 · 캡슐 반높이. 추정하지 않고 CDO 에서 읽는다 (BP 를 바꾸면 따라간다).
		const UERWildlifeSettings& S = UERWildlifeSettings::Get();
		UClass* Class = S.WildlifeClass.IsNull() ? nullptr : S.WildlifeClass.LoadSynchronous();
		const AERWildlifeCharacter* CDO = Class ? Cast<AERWildlifeCharacter>(Class->GetDefaultObject()) : nullptr;
		if (!CDO || !CDO->GetMesh() || !CDO->GetCapsuleComponent())
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[히트박스 맞춤] WildlifeClass 를 못 읽었다 (Project Settings > ER Wildlife). 설정: %s"), *S.WildlifeClass.ToString());
			return;
		}
		const FRotator MeshRot = CDO->GetMesh()->GetRelativeRotation();
		const float HalfHeight = CDO->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		UE_LOG(LogEternalReturn, Log, TEXT("[히트박스 맞춤] BP 기준 — 메시 회전 %s · 캡슐 반높이 %.0f"), *MeshRot.ToCompactString(), HalfHeight);

		const FString Dir = S.DataPath;
		TArray<UERWildlifeData*> Targets;
		if (Args.Num() >= 1)
		{
			if (UERWildlifeData* D = const_cast<UERWildlifeData*>(UERWildlifeSettings::FindData(FName(*Args[0]))))
			{
				Targets.Add(D);
			}
		}
		else
		{
			TArray<FAssetData> Assets;
			FAssetRegistryModule::GetRegistry().GetAssetsByPath(FName(*Dir), Assets, /*bRecursive=*/true);   // 하위 폴더도
			for (const FAssetData& A : Assets)
			{
				if (A.AssetClassPath == UERWildlifeData::StaticClass()->GetClassPathName())
				{
					if (UERWildlifeData* D = Cast<UERWildlifeData>(A.GetAsset())) { Targets.Add(D); }
				}
			}
		}
		int32 Done = 0;
		for (UERWildlifeData* D : Targets) { if (FitHitBox(D, MeshRot, HalfHeight)) { ++Done; } }
		UE_LOG(LogEternalReturn, Log, TEXT("[히트박스 맞춤] 끝 — %d / %d. **Save All** 로 저장하라."), Done, Targets.Num());
	}

	FAutoConsoleCommand GFitHitBoxCmd(
		TEXT("ER.Wild.FitHitBox"),
		TEXT("[임시 · 에디터] DA.Mesh 바운드 → DA.HitBoxExtent (DataPath 의 전부 또는 한 종). ER.Wild.FitHitBox [이름]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FitHitBoxCmd));

	FAutoConsoleCommand GImportCmd(
		TEXT("ER.Wild.ImportCSV"),
		TEXT("[임시 · 에디터] Docs/3_EditorTasks/Data/DT_Wildlife.csv → DA_Wild_<이름> 애셋 생성 (DataPath 폴더). ER.Wild.ImportCSV [csv 경로] [Force] — Force 면 있는 애셋의 CSV 필드만 덮어쓴다"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ImportCsv));
}

#endif // WITH_EDITOR
