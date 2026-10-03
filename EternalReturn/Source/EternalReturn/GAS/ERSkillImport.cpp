// Copyright Epic Games, Inc. All Rights Reserved.
//
// [에디터] F19 — 스킬 DA 를 JSON 에서 채운다 (Argument 55 J1 · 사용자 2026-10-01 "스킬 데이터들도 입력해주면 안 됨??").
//   원본: Docs/3_EditorTasks/Data/Skills/<캐릭터>.json · 명령: ER.Skill.ImportJson <캐릭터|All> → Save All.
//   값은 **엔진 텍스트 형식** (에디터 칸 복사 · 붙여넣기와 같다) — FProperty::ImportText_InContainer 가 읽는다. 그래서 어떤 칸 · 어떤 조각이든 코드 수정 없이 들어간다.
//   - JSON 에 적은 칸만 덮어쓴다 (AbilityClass · SlotTag 를 안 적으면 그대로)
//   - "Fragments" 를 적으면 조각을 **통째로 새로** 만든다 (안 적으면 손대지 않음)
//   - 애셋은 `@이름` 으로 — 레지스트리에서 이름으로 찾아 경로로 바꾼다 (블루프린트면 생성 클래스 `_C`)
//   - 틀린 칸 · 값 · 이름은 Error 로그 (GAS 처럼 조용히 건너뛰지 않는다 — CLAUDE.md §8)

#if WITH_EDITOR

#include "Animation/AnimSequenceBase.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Character/ERCharacterData.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "FileHelpers.h"
#include "EternalReturn.h"
#include "GAS/ERSkillData.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "GAS/Delivery/ERSkillDelivery.h"
#include "GAS/Shape/ERSkillShape.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DataValidation.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Presentation/ERAnimNotify_HitMarker.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"
#include "UObject/Package.h"

namespace
{
	/** 파싱 에러를 모으는 출력 장치 — ImportText 가 왜 실패했는지 로그로 */
	class FImportErrors : public FOutputDevice
	{
	public:
		FString Text;
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type, const FName&) override { Text += V; Text += TEXT(" "); }
	};

	/** 이름별 개수 — 중복 경고용 (BuildNameIndex 가 채운다) */
	TMap<FName, int32> GNameCounts;

	/** 이름 → 애셋 (한 번만 모은다). 같은 이름이 둘이면 첫 번째. */
	TMap<FName, FAssetData> BuildNameIndex()
	{
		GNameCounts.Reset();
		TArray<FAssetData> All;
		FAssetRegistryModule::GetRegistry().GetAssetsByPath(TEXT("/Game"), All, /*bRecursive=*/true);
		TMap<FName, FAssetData> Index;
		for (const FAssetData& A : All)
		{
			// 같은 이름이 여럿이면 첫 번째 — 경고는 JSON 이 실제로 그 이름을 쓸 때만 (ResolveTokens). 텍스처 · 애니 중복까지 다 찍으면 로그가 묻힌다 (2026-10-01)
			if (A.IsRedirector())
			{
				continue;   // 폴더를 옮기면 남는 빈 껍데기 — 진짜 애셋이 아니다 (2026-10-01 /Game/Wildlife/DA_Wild_* 를 중복으로 셌다)
			}
			Index.FindOrAdd(A.AssetName, A);
			++GNameCounts.FindOrAdd(A.AssetName);
		}
		return Index;
	}

	/** `@이름` → 경로. 블루프린트면 생성 클래스. 못 찾으면 bOk = false. */
	FString ResolveTokens(const FString& In, const TMap<FName, FAssetData>& Index, const FString& Who, bool& bOk)
	{
		FString Out;
		for (int32 i = 0; i < In.Len(); ++i)
		{
			if (In[i] != TEXT('@'))
			{
				Out.AppendChar(In[i]);
				continue;
			}
			int32 j = i + 1;
			while (j < In.Len() && (FChar::IsAlnum(In[j]) || In[j] == TEXT('_'))) { ++j; }
			const FString Name = In.Mid(i + 1, j - i - 1);
			const FAssetData* A = Index.Find(*Name);
			if (A)
			{
				const int32 Count = GNameCounts.FindRef(*Name);
				if (Count > 1)
				{
					UE_LOG(LogEternalReturn, Warning, TEXT("[스킬 임포트] %s — @%s 가 %d곳에 있다 · %s 를 쓴다 (중복을 지운다)"), *Who, *Name, Count, *A->PackageName.ToString());
				}
			}
			if (!A)
			{
				UE_LOG(LogEternalReturn, Error, TEXT("[스킬 임포트] %s — 애셋 @%s 를 못 찾았다"), *Who, *Name);
				bOk = false;
			}
			else if (A->IsInstanceOf(UBlueprint::StaticClass()))
			{
				const UBlueprint* BP = Cast<UBlueprint>(A->GetAsset());
				Out += BP && BP->GeneratedClass ? BP->GeneratedClass->GetPathName() : FString();
			}
			else
			{
				Out += A->GetObjectPathString();
			}
			i = j - 1;
		}
		return Out;
	}

	FString JsonValueToText(const TSharedPtr<FJsonValue>& V)
	{
		if (!V.IsValid()) { return FString(); }
		FString S;
		if (V->TryGetString(S)) { return S; }
		double N = 0.0;
		if (V->TryGetNumber(N)) { return FString::SanitizeFloat(N); }
		bool B = false;
		if (V->TryGetBool(B)) { return B ? TEXT("True") : TEXT("False"); }
		return FString();
	}

	/** Obj 의 칸들을 텍스트로. 반환: 에러 수. */
	int32 ApplyProps(UObject* Obj, const TSharedPtr<FJsonObject>& Props, const TMap<FName, FAssetData>& Index, const FString& Who)
	{
		int32 Errors = 0;
		if (!Props.IsValid()) { return 0; }
		for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : Props->Values)
		{
			FProperty* P = FindFProperty<FProperty>(Obj->GetClass(), *KV.Key);
			if (!P)
			{
				UE_LOG(LogEternalReturn, Error, TEXT("[스킬 임포트] %s — 칸 '%s' 가 %s 에 없다"), *Who, *KV.Key, *Obj->GetClass()->GetName());
				++Errors;
				continue;
			}
			bool bOk = true;
			FString Raw = JsonValueToText(KV.Value);
			// `@Marker(애니)` — 그 애니의 "ER 타격 지점" 노티파이 시각(초)을 값으로 (선딜 = 사용자가 찍은 쏘는 순간 · 2026-10-01 "스킬 타이밍은 내가 노티파이 해놓을께").
			//   노티파이를 옮기면 임포트만 다시. 값은 DA 에 숫자로 들어가 서버는 애니를 읽지 않는다 (Argument 52 T2 그대로)
			if (Raw.StartsWith(TEXT("@Marker(")) && Raw.EndsWith(TEXT(")")))
			{
				const FString AnimName = Raw.Mid(8, Raw.Len() - 9).TrimStartAndEnd();
				const FAssetData* AA = Index.Find(*AnimName);
				const UAnimSequenceBase* Anim = AA ? Cast<UAnimSequenceBase>(AA->GetAsset()) : nullptr;
				float Sec = -1.f;
				if (Anim)
				{
					for (const FAnimNotifyEvent& E : Anim->Notifies)
					{
						if (Cast<UERAnimNotify_HitMarker>(E.Notify))
						{
							const float Scale = FMath::IsNearlyZero(Anim->RateScale) ? 1.f : FMath::Abs(Anim->RateScale);
							Sec = E.GetTriggerTime() / Scale;
							break;
						}
					}
				}
				if (Sec < 0.f)
				{
					UE_LOG(LogEternalReturn, Error, TEXT("[스킬 임포트] %s — 칸 %s: %s (애니 %s · \"ER 타격 지점\" 노티파이 %s) — 이 칸은 그대로 둔다"),
						*Who, *KV.Key, *Raw, Anim ? TEXT("있음") : TEXT("없음"), Anim ? TEXT("없음") : TEXT("-"));
					++Errors;
					continue;
				}
				UE_LOG(LogEternalReturn, Log, TEXT("[스킬 임포트] %s — %s ← %s 타격 지점 %.3f초"), *Who, *KV.Key, *AnimName, Sec);
				Raw = FString::SanitizeFloat(Sec);
			}
			// `@Length(애니)` — 그 애니의 실제 재생 길이(초 · RateScale 반영). 구조체 칸 안에서도 (예: Shape 의 ShotInterval=@Length(…))
			//   카티야 R 발 간격 = Fire 애니 길이 (사용자 2026-10-02 "shot 애니 속도를 조절해 볼게" — 애니를 바꾸면 임포트만 다시)
			for (int32 At = Raw.Find(TEXT("@Length(")); At != INDEX_NONE; At = Raw.Find(TEXT("@Length(")))
			{
				const int32 Close = Raw.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, At);
				if (Close == INDEX_NONE) { break; }
				const FString AnimName = Raw.Mid(At + 8, Close - At - 8).TrimStartAndEnd();
				const FAssetData* AA = Index.Find(*AnimName);
				UObject* LenAsset = AA ? AA->GetAsset() : nullptr;
				const UAnimSequenceBase* Anim = Cast<UAnimSequenceBase>(LenAsset);
				const USoundBase* Sound = Cast<USoundBase>(LenAsset);   // 소리 길이도 (카티야 R 발 사이 조준 = Aiming 소리 길이 · 사용자 2026-10-02)
				if (!Anim && !Sound)
				{
					UE_LOG(LogEternalReturn, Error, TEXT("[스킬 임포트] %s — 칸 %s: @Length(%s) 애니 · 소리를 못 찾았다"), *Who, *KV.Key, *AnimName);
					bOk = false;
					break;
				}
				const float Len = Anim ? Anim->GetPlayLength() / (FMath::IsNearlyZero(Anim->RateScale) ? 1.f : FMath::Abs(Anim->RateScale))
					: Sound->GetDuration();
				UE_LOG(LogEternalReturn, Log, TEXT("[스킬 임포트] %s — %s 안 @Length(%s) = %.3f초"), *Who, *KV.Key, *AnimName, Len);
				Raw = Raw.Left(At) + FString::SanitizeFloat(Len) + Raw.Mid(Close + 1);
			}
			if (!bOk) { ++Errors; continue; }
			const FString Text = ResolveTokens(Raw, Index, Who + TEXT(".") + KV.Key, bOk);
			if (!bOk) { ++Errors; continue; }
			FImportErrors Err;
			const TCHAR* End = P->ImportText_InContainer(*Text, Obj, Obj, PPF_None, &Err);
			if (!End || !Err.Text.IsEmpty())
			{
				UE_LOG(LogEternalReturn, Error, TEXT("[스킬 임포트] %s — 칸 %s 값 '%s' 를 못 읽었다 %s"), *Who, *KV.Key, *Text, *Err.Text);
				++Errors;
			}
		}
		return Errors;
	}

	/**
	 * `{"Type": "Trapezoid", "Props": {…}}` → Outer 안의 새 객체 (S3.1 ⑦). 논리 이름 → 클래스 `<Prefix><Type>` (ERShape_Trapezoid · ERDelivery_Projectile).
	 * C++ 클래스 이름이 바뀌어도 JSON 은 그대로 — 여기 한 곳만 고친다. 못 만들면 nullptr.
	 */
	template <class TBase>
	TBase* MakeTyped(UObject* Outer, const TSharedPtr<FJsonObject>& O, const TCHAR* Prefix, const TMap<FName, FAssetData>& Index, const FString& Who, int32& Errors)
	{
		FString Type;
		if (!O.IsValid() || !O->TryGetStringField(TEXT("Type"), Type))
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[스킬 임포트] %s — \"Type\" 이 없다"), *Who);
			++Errors;
			return nullptr;
		}
		const FString ClassName = FString(Prefix) + Type;
		UClass* Cls = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::NativeFirst);
		if (!Cls || !Cls->IsChildOf(TBase::StaticClass()) || Cls->HasAnyClassFlags(CLASS_Abstract))
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[스킬 임포트] %s — Type '%s' 가 없다 (클래스 %s)"), *Who, *Type, *ClassName);
			++Errors;
			return nullptr;
		}
		TBase* Obj = NewObject<TBase>(Outer, Cls, NAME_None, RF_Transactional);
		const TSharedPtr<FJsonObject>* Props = nullptr;
		if (O->TryGetObjectField(TEXT("Props"), Props))
		{
			Errors += ApplyProps(Obj, *Props, Index, Who + TEXT(".") + Type);
		}
		return Obj;
	}

	/** From = 새로 만들 때 복제할 원본 DA 이름 (`@DA_BasicAttack` · 비면 빈 DA). 이미 있으면 무시 — 적힌 칸만 덮는다. */
	UERSkillData* FindOrCreateSkill(const FString& AssetName, const FString& Folder, TMap<FName, FAssetData>& Index, bool& bCreated, const FString& From = FString())
	{
		bCreated = false;
		if (const FAssetData* A = Index.Find(*AssetName))
		{
			UERSkillData* D = Cast<UERSkillData>(A->GetAsset());
			if (!D)
			{
				UE_LOG(LogEternalReturn, Error, TEXT("[스킬 임포트] %s 가 있지만 스킬 DA(UERSkillData) 가 아니다 — %s"), *AssetName, *A->AssetClassPath.ToString());
			}
			return D;
		}
		UPackage* Package = CreatePackage(*(Folder / AssetName));
		// "From" — 원본 DA 를 통째로 복제 (슬롯 · 어빌리티 클래스 · 조각 · 판정 칸) 후 적힌 칸만 덮는다 (원거리 평타 · Argument 60 D2)
		const FString FromName = From.StartsWith(TEXT("@")) ? From.Mid(1) : From;
		const FAssetData* FromAsset = FromName.IsEmpty() ? nullptr : Index.Find(*FromName);
		UERSkillData* Source = FromAsset ? Cast<UERSkillData>(FromAsset->GetAsset()) : nullptr;
		if (!FromName.IsEmpty() && !Source)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[스킬 임포트] %s — From %s 를 못 찾았다 (스킬 DA 가 아니다) · 빈 DA 로 만든다"), *AssetName, *From);
		}
		UERSkillData* D = Source
			? DuplicateObject<UERSkillData>(Source, Package, *AssetName)
			: NewObject<UERSkillData>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
		D->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
		if (Source)
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬 임포트] %s ← %s 복제"), *AssetName, *Source->GetName());
		}
		FAssetRegistryModule::AssetCreated(D);
		Index.Add(*AssetName, FAssetData(D));   // 같은 임포트의 뒤 스킬이 @이름 으로 바로 참조한다 (카티야 Q · E → @DA_Skill_Katja_P)
		bCreated = true;
		return D;
	}

	void ImportFile(const FString& Path, TMap<FName, FAssetData>& Index, int32& OutSkills, int32& OutErrors, TArray<UPackage*>& OutTouched)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[스킬 임포트] 파일을 못 읽었다: %s"), *Path);
			++OutErrors;
			return;
		}
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[스킬 임포트] JSON 형식이 틀렸다: %s"), *Path);
			++OutErrors;
			return;
		}
		const FString CharName = FPaths::GetBaseFilename(Path);
		FString DefaultFolder = FString::Printf(TEXT("/Game/ERCharacter/CharData/Skill/%s"), *CharName);
		Root->TryGetStringField(TEXT("Folder"), DefaultFolder);

		// "Character" 를 적으면 그 실험체 DA 의 Skills 에서 **같은 슬롯**을 이 스킬로 바꿔 끼운다 (없으면 더한다) — 손으로 안 넣게
		UERCharacterData* CharData = nullptr;
		FString CharAsset;
		if (Root->TryGetStringField(TEXT("Character"), CharAsset))
		{
			const FAssetData* CA = Index.Find(*CharAsset);
			CharData = CA ? Cast<UERCharacterData>(CA->GetAsset()) : nullptr;
			if (!CharData)
			{
				UE_LOG(LogEternalReturn, Error, TEXT("[스킬 임포트] 실험체 DA %s 를 못 찾았다 — 스킬 목록은 안 바꾼다"), *CharAsset);
				++OutErrors;
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* Skills = nullptr;
		if (!Root->TryGetArrayField(TEXT("Skills"), Skills))
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[스킬 임포트] %s — \"Skills\" 배열이 없다"), *Path);
			++OutErrors;
			return;
		}
		for (const TSharedPtr<FJsonValue>& SV : *Skills)
		{
			const TSharedPtr<FJsonObject> S = SV->AsObject();
			FString AssetName;
			if (!S.IsValid() || !S->TryGetStringField(TEXT("Asset"), AssetName))
			{
				UE_LOG(LogEternalReturn, Error, TEXT("[스킬 임포트] %s — \"Asset\" 이 없는 항목"), *CharName);
				++OutErrors;
				continue;
			}
			FString Folder = DefaultFolder;
			S->TryGetStringField(TEXT("Folder"), Folder);
			bool bCreated = false;
			FString From;
			S->TryGetStringField(TEXT("From"), From);
			UERSkillData* D = FindOrCreateSkill(AssetName, Folder, Index, bCreated, From);
			if (!D) { ++OutErrors; continue; }

			D->Modify();
			const TSharedPtr<FJsonObject> SkillProps = S->GetObjectField(TEXT("Props"));
			int32 Errors = 0;
			if (SkillProps.IsValid() && SkillProps->HasField(TEXT("Shape")))
			{
				// 옛 칸 — 써도 판정은 안 바뀐다 (S3.1). 조용히 무시하면 "고쳤는데 그대로" 가 된다
				UE_LOG(LogEternalReturn, Error, TEXT("[스킬 임포트] %s — \"Shape\" 는 옛 칸이다 · \"Area\" · \"Targets\" · \"Delivery\" 로 (Argument 57 S3.1) — 무시"), *AssetName);
				SkillProps->RemoveField(TEXT("Shape"));
				++Errors;
			}
			Errors += ApplyProps(D, SkillProps, Index, AssetName);

			// 어디를 · 어떻게 — 적었을 때만 통째로 바꾼다 (조각과 같다)
			const TSharedPtr<FJsonObject>* AreaJson = nullptr;
			if (S->TryGetObjectField(TEXT("Area"), AreaJson))
			{
				if (UERSkillShapeBase* A = MakeTyped<UERSkillShapeBase>(D, *AreaJson, TEXT("ERShape_"), Index, AssetName + TEXT(".Area"), Errors))
				{
					D->Area = A;
				}
			}
			const TSharedPtr<FJsonObject>* DeliveryJson = nullptr;
			if (S->TryGetObjectField(TEXT("Delivery"), DeliveryJson))
			{
				if (UERSkillDelivery* Dl = MakeTyped<UERSkillDelivery>(D, *DeliveryJson, TEXT("ERDelivery_"), Index, AssetName + TEXT(".Delivery"), Errors))
				{
					D->Delivery = Dl;
				}
			}

			const TArray<TSharedPtr<FJsonValue>>* Frags = nullptr;
			int32 FragCount = -1;
			if (S->TryGetArrayField(TEXT("Fragments"), Frags))
			{
				D->Fragments.Reset();
				for (const TSharedPtr<FJsonValue>& FV : *Frags)
				{
					const TSharedPtr<FJsonObject> FO = FV->AsObject();
					FString ClassName;
					if (!FO.IsValid() || !FO->TryGetStringField(TEXT("Class"), ClassName))
					{
						UE_LOG(LogEternalReturn, Error, TEXT("[스킬 임포트] %s — 조각에 \"Class\" 가 없다"), *AssetName);
						++Errors;
						continue;
					}
					UClass* Cls = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::NativeFirst);
					if (!Cls || !Cls->IsChildOf(UERSkillFragment::StaticClass()) || Cls->HasAnyClassFlags(CLASS_Abstract))
					{
						UE_LOG(LogEternalReturn, Error, TEXT("[스킬 임포트] %s — 조각 클래스 '%s' 가 없다 (예: ERSkillFragment_Damage)"), *AssetName, *ClassName);
						++Errors;
						continue;
					}
					UERSkillFragment* Frag = NewObject<UERSkillFragment>(D, Cls, NAME_None, RF_Transactional);
					const TSharedPtr<FJsonObject>* FProps = nullptr;
					if (FO->TryGetObjectField(TEXT("Props"), FProps))
					{
						Errors += ApplyProps(Frag, *FProps, Index, AssetName + TEXT(".") + ClassName);
					}
					D->Fragments.Add(Frag);
				}
				FragCount = D->Fragments.Num();
			}
			D->MarkPackageDirty();
			OutTouched.AddUnique(D->GetPackage());
			++OutSkills;
			if (CharData && D->SlotTag.IsValid())
			{
				CharData->Modify();
				const int32 Slot = CharData->Skills.IndexOfByPredicate([D](const TObjectPtr<UERSkillData>& X) { return X && X->SlotTag == D->SlotTag; });
				const FString Prev = Slot != INDEX_NONE ? GetNameSafe(CharData->Skills[Slot]) : FString(TEXT("없음"));
				if (Slot != INDEX_NONE) { CharData->Skills[Slot] = D; } else { CharData->Skills.Add(D); }
				CharData->MarkPackageDirty();
				OutTouched.AddUnique(CharData->GetPackage());
				UE_LOG(LogEternalReturn, Log, TEXT("[스킬 임포트] %s · %s 자리 %s → %s"), *CharData->GetName(), *D->SlotTag.ToString(), *Prev, *AssetName);
			}
			OutErrors += Errors;
			UE_LOG(LogEternalReturn, Log, TEXT("[스킬 임포트] %s %s — 쿨 %d칸 · 선딜 %.2f · 모양 %s · 사거리 %.1f · 조각 %s%s"),
				*AssetName, bCreated ? TEXT("새로 만듦") : TEXT("덮어씀"), D->Cooldowns.Num(), D->CastTime,
				D->Area ? *D->Area->Describe() : TEXT("없음"), D->GetMaxReach(),
				FragCount < 0 ? TEXT("(손대지 않음)") : *FString::FromInt(FragCount),
				Errors > 0 ? *FString::Printf(TEXT(" · ⚠ 에러 %d"), Errors) : TEXT(""));
		}
	}

	void ImportSkillJson(const TArray<FString>& Args)
	{
		const FString Dir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Docs/3_EditorTasks/Data/Skills"));
		const FString Which = Args.IsEmpty() ? FString(TEXT("All")) : Args[0];
		TArray<FString> Files;
		if (Which.Equals(TEXT("All"), ESearchCase::IgnoreCase))
		{
			IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.json")), /*Files=*/true, /*Directories=*/false);
		}
		else
		{
			Files.Add(Which + TEXT(".json"));
		}
		if (Files.IsEmpty())
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[스킬 임포트] %s 에 JSON 이 없다"), *Dir);
			return;
		}
		TMap<FName, FAssetData> Index = BuildNameIndex();
		int32 Skills = 0, Errors = 0;
		TArray<UPackage*> Touched;
		for (const FString& F : Files)
		{
			ImportFile(Dir / F, Index, Skills, Errors, Touched);
		}
		// ⭐ 바꾼 DA 를 바로 저장 — 새로 만든 DA 를 저장 전에 PIE 하면 클라로 못 보낸다 (`NOT Supported` · `GA_Test 에 SkillData 가 없다` — 2026-10-01 두 번)
		const bool bSaved = Touched.IsEmpty() || UEditorLoadingAndSavingUtils::SavePackages(Touched, /*bOnlyDirty=*/true);
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬 임포트] 저장 %d개 %s"), Touched.Num(), bSaved ? TEXT("✅") : TEXT("⚠ 실패 — Save All 로"));
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬 임포트] 끝 — 스킬 %d · 에러 %d%s"), Skills, Errors,
			Errors > 0 ? TEXT(" · ⚠ 에러 줄을 먼저 본다 (그 칸만 안 들어갔다)") : TEXT(""));
	}

	FAutoConsoleCommand GImportSkillJsonCmd(
		TEXT("ER.Skill.ImportJson"),
		TEXT("[에디터] Docs/3_EditorTasks/Data/Skills/<캐릭터>.json → 스킬 DA (적힌 칸만 · Fragments 는 통째로). ER.Skill.ImportJson <캐릭터|All>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ImportSkillJson));

	/** 모든 스킬 DA 를 불러 이관(PostLoad) → 검사 → 이관된 것만 저장 (Argument 57 S3.1). */
	void ResaveSkills()
	{
		TArray<FAssetData> Assets;
		FAssetRegistryModule::GetRegistry().GetAssetsByClass(UERSkillData::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);
		TArray<UPackage*> Touched;
		int32 Invalid = 0;
		for (const FAssetData& A : Assets)
		{
			UERSkillData* D = Cast<UERSkillData>(A.GetAsset());   // 불러오는 순간 PostLoad 가 이관 · 로그
			if (!D)
			{
				continue;
			}
			FDataValidationContext Ctx;
			if (D->IsDataValid(Ctx) == EDataValidationResult::Invalid)
			{
				++Invalid;
				TArray<FText> Warnings, Errors;
				Ctx.SplitIssues(Warnings, Errors);
				for (const FText& E : Errors) { UE_LOG(LogEternalReturn, Error, TEXT("[스킬 이관] 검사 실패 %s"), *E.ToString()); }
			}
			if (D->bShapeMigratedOnLoad)
			{
				D->MarkPackageDirty();
				Touched.Add(D->GetPackage());
			}
		}
		const bool bSaved = Touched.IsEmpty() || UEditorLoadingAndSavingUtils::SavePackages(Touched, /*bOnlyDirty=*/true);
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬 이관] 끝 — 스킬 DA %d · 이관 · 저장 %d %s · 검사 실패 %d"),
			Assets.Num(), Touched.Num(), bSaved ? TEXT("✅") : TEXT("⚠ 저장 실패 — Save All 로"), Invalid);
	}

	FAutoConsoleCommand GResaveSkillsCmd(
		TEXT("ER.Skill.Resave"),
		TEXT("[에디터] 모든 스킬 DA: 옛 Shape → Area · Targets · Delivery 이관 · 검사 · 저장 (S3.1 · 한 번)"),
		FConsoleCommandDelegate::CreateStatic(&ResaveSkills));
}

#endif // WITH_EDITOR
