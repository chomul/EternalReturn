// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/ERPresentationComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "EternalReturn.h"
#include "GameFramework/Character.h"
#include "Presentation/ERPresentationData.h"
#include "Core/ERPlayerState.h"
#include "GAS/ERGameplayTags.h"
#include "Presentation/ERAnimInstance.h"
#include "Presentation/ERAnimNotify_HitMarker.h"
#include "GameplayEffectTypes.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Sound/SoundBase.h"
#include "Weapon/ERWeaponLibrary.h"
#include "Weapon/ERWeaponTypes.h"

namespace
{
	/** 동적 재생 슬롯 · 블렌드 [자체] (Argument 39 ⑤) — 상체 분리가 필요해지면 F12.5-03 에서 나눈다. */
	const FName AnimSlotName(TEXT("DefaultSlot"));
	constexpr float BlendIn = 0.1f;
	constexpr float BlendOut = 0.2f;
	/** 채집 끝 — 웅크림에서 서기까지 [자체] (Argument 47 F1). collect 에 일어서는 구간이 없어 이 블렌드가 일어서기다. */
	constexpr float GatherBlendOut = 0.5f;

	/** 애니 길이가 선딜+후딜 × 이 값보다 길면 경고 — 판정과 모션이 어긋나는 걸 데이터 단계에서 잡는다. */
	constexpr float LengthWarnRatio = 1.5f;

	/** 반복 소리 (F19-02 · Audio/Magnus.md) — 이 태그가 있는 동안 Loop 키를 반복 · 붙을 때 Start 키 한 번 */
	struct FLoopSfx { FGameplayTag Tag; FGameplayTag Loop; FGameplayTag Start; };
	TArray<FLoopSfx> LoopSfxTable()
	{
		return {
			{ ERTags::State_AnimHold, ERTags::Pres_Sfx_SkillLoop_W, FGameplayTag() },               // W 도는 동안 (장판 동안 모션 유지 태그)
			{ ERTags::State_Riding, ERTags::Pres_Sfx_SkillLoop_R, ERTags::Pres_Sfx_SkillLoopStart_R }, // R 탄 동안 · 시동
		};
	}

	const TCHAR* NetTag(const AActor* Owner)
	{
		return Owner && Owner->HasAuthority() ? TEXT("서버") : TEXT("클라");
	}
}

UERPresentationComponent::UERPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UERPresentationComponent::SetBase(UERPresentationData* InBase)
{
	if (Base == InBase)
	{
		return;
	}
	Base = InBase;
	Rebuild();
	// 무기가 먼저 정해졌으면 세트 목록이 방금 생겼다 — 다시 로드 시도.
	if (Weapon != EERWeaponType::None && !LoadedWeaponSets.Contains(Weapon))
	{
		const EERWeaponType W = Weapon;
		Weapon = EERWeaponType::None;
		SetWeapon(W);
	}
}

void UERPresentationComponent::SetSkin(const TSoftObjectPtr<UERSkinData>& SkinRef)
{
	UERSkinData* NewSkin = SkinRef.IsNull() ? nullptr : SkinRef.LoadSynchronous();
	if (!SkinRef.IsNull() && !NewSkin)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[연출] %s 스킨 %s 로드 실패 — 기본으로"), *GetNameSafe(GetOwner()), *SkinRef.ToString());
	}
	Skin = NewSkin;

	// 몸 교체 — 각 머신이 로컬로 (SkinIndex 복제 → OnRep → 여기). 서버도 한다: GAS 몽타주 재생에 서버의 AnimInstance 가 필요하다 (Argument 39 점검).
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()); Character && Skin)
	{
		if (USkeletalMeshComponent* MeshComp = Character->GetMesh())
		{
			if (Skin->Mesh && MeshComp->GetSkeletalMeshAsset() != Skin->Mesh)
			{
				MeshComp->SetSkeletalMesh(Skin->Mesh);
				LinkedLayer = nullptr;   // 애님 인스턴스가 다시 초기화된다 — 레이어를 다시 붙인다
			}
			if (Skin->AnimClass && MeshComp->GetAnimClass() != Skin->AnimClass)
			{
				MeshComp->SetAnimInstanceClass(Skin->AnimClass);
				LinkedLayer = nullptr;   // 새 인스턴스에는 아무것도 안 붙어 있다
			}
		}
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 스킨 %s (%s)"), *GetNameSafe(GetOwner()), Skin ? *Skin->GetName() : TEXT("없음"), NetTag(GetOwner()));
	Rebuild();
	// 부착 (Argument 64) — 스킨이 무기 · 소품 모습을 정한다
	RefreshWeaponAttach();
	BindPropTags();
}

void UERPresentationComponent::SetWeapon(EERWeaponType InWeapon)
{
	if (Weapon == InWeapon)
	{
		return;
	}
	Weapon = InWeapon;

	// 두 DA 를 같이 — 캐릭터의 무기 세트 (Base->WeaponSets) · 무기 공통 (DT_WeaponClass.Presentation · 49)
	TArray<FSoftObjectPath> ToLoad;
	auto Hold = [this, &ToLoad](const TSoftObjectPtr<UERPresentationData>& Ref, TMap<EERWeaponType, TObjectPtr<UERPresentationData>>& Held, const TCHAR* What)
	{
		if (Ref.IsNull() || Held.Contains(Weapon))
		{
			return;   // 없음(맨손 · 표에 없는 무기) 또는 이미 붙잡음
		}
		if (UERPresentationData* Already = Ref.Get())
		{
			// 이미 메모리에 있다 (같은 판 다른 플레이어 · PIE 는 서버 · 클라가 한 프로세스) — 붙잡기만
			Held.Add(Weapon, Already);
			UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s %s %s (이미 로드됨 · %s)"), *GetNameSafe(GetOwner()), What, *Already->GetName(), NetTag(GetOwner()));
			return;
		}
		ToLoad.Add(Ref.ToSoftObjectPath());
	};
	if (Weapon != EERWeaponType::None)
	{
		if (const TSoftObjectPtr<UERPresentationData>* SetRef = Base ? Base->WeaponSets.Find(Weapon) : nullptr)
		{
			Hold(*SetRef, LoadedWeaponSets, TEXT("무기 세트"));
		}
		if (const FERWeaponClassRow* Row = ERWeapon::Find(Weapon))
		{
			Hold(Row->Presentation, LoadedWeaponCommon, TEXT("무기 공통"));
		}
	}
	// ⭐ 비동기 — 로드 전 몇 프레임은 비어 "판정만" (Argument 39 로드 시점). 서버 · 각 클라가 각자.
	Rebuild();
	RefreshWeaponAttach();   // 손 무기 (Argument 64 W1) — 스킨 DA 에 있어 세트 로드를 기다리지 않는다
	if (ToLoad.IsEmpty())
	{
		return;
	}
	const EERWeaponType Requested = Weapon;
	PendingWeaponLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(ToLoad,
		FStreamableDelegate::CreateUObject(this, &UERPresentationComponent::OnWeaponSetLoaded, Requested));
	UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 무기 세트 로드 요청 %s (%d개 · %s)"), *GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Requested), ToLoad.Num(), NetTag(GetOwner()));
}

void UERPresentationComponent::OnWeaponSetLoaded(EERWeaponType LoadedWeapon)
{
	auto Take = [this, LoadedWeapon](const TSoftObjectPtr<UERPresentationData>& Ref, TMap<EERWeaponType, TObjectPtr<UERPresentationData>>& Held, const TCHAR* What)
	{
		if (Ref.IsNull() || Held.Contains(LoadedWeapon))
		{
			return;
		}
		UERPresentationData* Loaded = Ref.Get();
		if (!Loaded)
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[연출] %s %s %s 로드 실패 — %s"), *GetNameSafe(GetOwner()), What, *UEnum::GetValueAsString(LoadedWeapon), *Ref.ToString());
			return;
		}
		Held.Add(LoadedWeapon, Loaded);   // P2 — 붙잡는다
		UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s %s 로드 %s (%s)"), *GetNameSafe(GetOwner()), What, *Loaded->GetName(), NetTag(GetOwner()));
	};
	if (const TSoftObjectPtr<UERPresentationData>* SetRef = Base ? Base->WeaponSets.Find(LoadedWeapon) : nullptr)
	{
		Take(*SetRef, LoadedWeaponSets, TEXT("무기 세트"));
	}
	if (const FERWeaponClassRow* Row = ERWeapon::Find(LoadedWeapon))
	{
		Take(Row->Presentation, LoadedWeaponCommon, TEXT("무기 공통"));
	}
	if (LoadedWeapon == Weapon)
	{
		Rebuild();
	}
}

void UERPresentationComponent::AddLayer(const TArray<FERPresentationEntry>& Entries, const TCHAR* Source, bool bFilterWeapon, bool bRequireWeaponMatch, bool bModePass)
{
	for (const FERPresentationEntry& E : Entries)
	{
		if (!E.Key.IsValid())
		{
			continue;
		}
		// 모드 칸 (Argument 42 ⑤): 평소 층에는 모드 없는 줄만 · 모드 층에는 지금 모드와 같은 줄만
		if (bModePass ? (!E.Mode.IsValid() || E.Mode != ActiveMode) : E.Mode.IsValid())
		{
			continue;
		}
		if (bFilterWeapon)
		{
			// bRequireWeaponMatch: 이 무기 전용 줄만 · 아니면 무기 무관 줄만
			if (bRequireWeaponMatch ? (E.Weapon == EERWeaponType::None || E.Weapon != Weapon) : (E.Weapon != EERWeaponType::None))
			{
				continue;
			}
		}
		Cache.Add(E.Key, FResolved{ &E, Source });   // 나중 층이 덮는다
	}
}

void UERPresentationComponent::Rebuild()
{
	Cache.Reset();
	// 넓은 것부터 쌓고 좁은 것이 덮는다: 무기 공통 → 기본 → 기본의 무기 전용 → 무기 세트 → (모드 줄) → 스킨 → 스킨 무기 전용 → (스킨 모드 줄)
	const TObjectPtr<UERPresentationData>* Set = LoadedWeaponSets.Find(Weapon);
	const bool bMode = ActiveMode.IsValid();
	if (const TObjectPtr<UERPresentationData>* Common = LoadedWeaponCommon.Find(Weapon))
	{
		AddLayer((*Common)->Entries, TEXT("무기공통"), false, false, false);   // 캐릭터와 무관한 무기 소리 (Argument 39 ④ · 49)
	}
	if (Base)
	{
		AddLayer(Base->Entries, TEXT("기본"), true, false, false);
		AddLayer(Base->Entries, TEXT("기본·무기"), true, true, false);
	}
	if (Set)
	{
		AddLayer((*Set)->Entries, TEXT("무기세트"), false, false, false);
	}
	// 모드 줄 (Argument 42 ⑤) — 캐릭터 모드(전기톱)는 기본 DA 에, 무기 모드(저격)는 무기 세트에 적는다
	if (bMode && Base)
	{
		AddLayer(Base->Entries, TEXT("기본·모드"), true, false, true);
		AddLayer(Base->Entries, TEXT("기본·모드·무기"), true, true, true);
	}
	if (bMode && Set)
	{
		AddLayer((*Set)->Entries, TEXT("무기세트·모드"), false, false, true);
	}
	if (Skin)
	{
		AddLayer(Skin->Overrides, TEXT("스킨"), true, false, false);
		AddLayer(Skin->Overrides, TEXT("스킨·무기"), true, true, false);
		if (bMode)
		{
			AddLayer(Skin->Overrides, TEXT("스킨·모드"), true, false, true);
			AddLayer(Skin->Overrides, TEXT("스킨·모드·무기"), true, true, true);
		}
	}
	UE_LOG(LogEternalReturn, Verbose, TEXT("[연출] %s 해석 %d키 (무기 %s · 스킨 %s)"), *GetNameSafe(GetOwner()), Cache.Num(),
		*UEnum::GetValueAsString(Weapon), Skin ? *Skin->GetName() : TEXT("없음"));
	ApplyWeaponLayer();
	PushModeToAnim();
	PushStateToAnim();
}

UAnimMontage* UERPresentationComponent::FindSkillMontage(FGameplayTag Key, FName Section) const
{
	UAnimMontage* Montage = Cast<UAnimMontage>(FindFirstAnim(Key));
	return Montage && Montage->IsValidSectionName(Section) ? Montage : nullptr;
}

bool UERPresentationComponent::JumpSkillSection(FGameplayTag Key, FName Section)
{
	UAnimMontage* Montage = FindSkillMontage(Key, Section);
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* AnimInst = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!Montage || !ASC || !AnimInst)
	{
		return false;
	}
	bool bJumped = false;
	if (GetOwner()->HasAuthority())
	{
		// 서버 — ASC 현재 몽타주여야 복제된다 (어빌리티가 ASC->PlayMontage 로 틀었다 · 어빌리티가 끝나도 몽타주는 계속)
		if (ASC->GetCurrentMontage() == Montage)
		{
			ASC->CurrentMontageJumpToSection(Section);
			bJumped = true;
		}
	}
	else if (AnimInst->Montage_IsPlaying(Montage))
	{
		AnimInst->Montage_JumpToSection(Section, Montage);
		bJumped = true;
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s %s 섹션 %s %s (%s)"), *GetNameSafe(GetOwner()), *Montage->GetName(), *Section.ToString(),
		bJumped ? TEXT("→") : TEXT("— 몽타주가 재생 중이 아니다 (건너뜀)"), NetTag(GetOwner()));
	return bJumped;
}

USoundBase* UERPresentationComponent::ResolveSound(USoundBase* Default) const
{
	const TObjectPtr<USoundBase>* Swap = Skin && Default ? Skin->SoundSwaps.Find(Default) : nullptr;
	return Swap && *Swap ? Swap->Get() : Default;
}

USoundBase* UERPresentationComponent::PickSound(FGameplayTag Key, int32 ShotNumber) const
{
	const FResolved* R = Cache.Find(Key);
	if (!R)
	{
		return nullptr;
	}
	TArray<USoundBase*, TInlineAllocator<4>> Sounds;
	for (const TObjectPtr<UObject>& A : R->Entry->Assets)
	{
		if (USoundBase* S = Cast<USoundBase>(A)) { Sounds.Add(S); }
	}
	if (Sounds.IsEmpty())
	{
		return nullptr;
	}
	// 순차 사격 — 발마다 정해진 소리 (Skill04_Shot → _02 → _03 · 이름순으로 들어 있다 · K8)
	if (ShotNumber > 0)
	{
		return Sounds[FMath::Min(ShotNumber, Sounds.Num()) - 1];
	}
	// r1 · r2 · r3 중 하나 — 각 클라가 따로 고른다 (의미 없는 변형이라 복제 안 함 · Argument 49)
	return Sounds[FMath::RandRange(0, Sounds.Num() - 1)];
}

void UERPresentationComponent::HandlePresCue(FGameplayTag CueTag, const FGameplayCueParameters& Params)
{
	const bool bAttackCue = CueTag == ERTags::GameplayCue_Pres_Attack;
	const bool bAimCue = CueTag == ERTags::GameplayCue_Pres_Aim;
	const bool bReadyCue = CueTag == ERTags::GameplayCue_Pres_Ready;
	if (!bAttackCue && !bAimCue && !bReadyCue && CueTag != ERTags::GameplayCue_Pres_Hit)
	{
		return;
	}
	// ⭐ 소리는 **시전자**의 연출에서 찾는다 (무기 · 스킨이 시전자 것) — 타격음도 맞은 쪽이 아니라 때린 쪽 무기 소리
	AActor* Instigator = Params.Instigator.Get();
	const UERPresentationComponent* Source = Instigator ? Instigator->FindComponentByClass<UERPresentationComponent>() : nullptr;
	if (!Source)
	{
		return;   // 시전자가 이미 사라졌다 · 연출 없는 액터
	}
	const bool bBasic = Params.AggregatedSourceTags.HasTagExact(ERTags::Ability_Slot_Attack);
	// 스킬 통 몽타주 (K8) — 이 사건의 슬롯 키 (Q~R)
	FGameplayTag EventSlot;
	const FGameplayTag SkillSlots[] = { ERTags::Ability_Slot_Q, ERTags::Ability_Slot_W, ERTags::Ability_Slot_E, ERTags::Ability_Slot_R };
	for (const FGameplayTag& S : SkillSlots)
	{
		if (Params.AggregatedSourceTags.HasTagExact(S)) { EventSlot = S; break; }
	}
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const bool bOwnerClient = Instigator == GetOwner() && OwnerPawn && OwnerPawn->IsLocallyControlled() && !GetOwner()->HasAuthority();
	const bool bSectionMontage = EventSlot.IsValid() && FindSkillMontage(EventSlot, ERPresSection::Execute) != nullptr;
	if ((bAttackCue && Params.RawMagnitude >= 1.f) || bAimCue)
	{
		if (bSectionMontage)
		{
			// 섹션은 서버가 넘기고 복제된다 — 엔진이 본인에게만 안 주니 **소유 클라만** 여기서 (발 = Execute · 조준 N>0 = Loop · 조준 0 = 더 쏠 발 없음 → End)
			if (bOwnerClient)
			{
				const FName Section = bAttackCue ? ERPresSection::Execute : Params.RawMagnitude >= 1.f ? ERPresSection::Loop : ERPresSection::End;
				JumpSkillSection(EventSlot, Section);
			}
		}
		else if (bAttackCue && Instigator == GetOwner())
		{
			// 몽타주가 없으면 — 발마다 판정 순간 애니(`<슬롯>.Execute`)를 이 머신에서 (순차 사격은 어빌리티가 판정 순간에 안 튼다)
			static const TPair<FGameplayTag, FGameplayTag> ExecKeys[] = {
				{ ERTags::Ability_Slot_Q, ERTags::Ability_Slot_Q_Execute }, { ERTags::Ability_Slot_W, ERTags::Ability_Slot_W_Execute },
				{ ERTags::Ability_Slot_E, ERTags::Ability_Slot_E_Execute }, { ERTags::Ability_Slot_R, ERTags::Ability_Slot_R_Execute },
			};
			for (const TPair<FGameplayTag, FGameplayTag>& E : ExecKeys)
			{
				if (E.Key == EventSlot)
				{
					const FString Played = PlayEventPres(E.Value, FGameplayTag());
					UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s %.0f번째 발 — %s (%s)"), *GetNameSafe(GetOwner()), Params.RawMagnitude, *Played, NetTag(GetOwner()));
					break;
				}
			}
		}
	}
	// 순차 사격 — 쏘는 대상 쪽으로 몸 (서버가 이미 돌렸다 · 엔진이 소유 클라 회전은 클라 쪽 값을 쓰니 본인 화면도 같이 · Yaw 만)
	if (bAttackCue && bOwnerClient && !Params.Normal.IsNearlyZero())
	{
		GetOwner()->SetActorRotation(FRotator(0.f, FVector(Params.Normal).Rotation().Yaw, 0.f));
	}
	if (bAimCue && Params.RawMagnitude < 1.f)
	{
		return;   // 조준 0 = 끝 신호 — 소리 없음
	}
	FGameplayTag Key = bAimCue ? ERTags::Pres_Sfx_SkillAim_R
		: bReadyCue ? ERTags::Pres_Sfx_EnhanceReady
		: bAttackCue ? (bBasic ? ERTags::Pres_Sfx_Attack : ERTags::Pres_Sfx_SkillCast)
		: (bBasic ? ERTags::Pres_Sfx_Hit : ERTags::Pres_Sfx_SkillHit);
	// 강화를 소비하는 평타 — 강화 소리 줄이 있으면 평소 소리 **대신** (카티야 P Reinforce_Shot · _Hit · Docs/3_EditorTasks/Audio/Katja.md)
	if (bBasic && !bAimCue && !bReadyCue && Params.AggregatedSourceTags.HasTagExact(ERTags::State_NextAttackBuff))
	{
		const FGameplayTag Enhanced = bAttackCue ? ERTags::Pres_Sfx_AttackEnhanced : ERTags::Pres_Sfx_HitEnhanced;
		if (Source->HasKey(Enhanced))
		{
			Key = Enhanced;
		}
	}
	// 재사용의 공격음 (F19-02 매그너스 R 바이크 발사) — 줄이 있으면 슬롯 소리 대신
	const bool bRecastSfx = bAttackCue && Params.AggregatedSourceTags.HasTagExact(ERTags::Ability_Slot_R_Recast) && Source->HasKey(ERTags::Pres_Sfx_SkillRecast_R);
	if (bRecastSfx)
	{
		Key = ERTags::Pres_Sfx_SkillRecast_R;
	}
	// 스킬마다 다른 소리 (F12.6-05 오메가 Q · W · F19 실험체) — `<SkillCast|SkillHit>.<슬롯>` 줄이 있으면 그것 · 없으면 공통 키
	if (!bBasic && !bAimCue && !bReadyCue && !bRecastSfx)
	{
		struct FSlotSfx { FGameplayTag Slot; FGameplayTag Cast; FGameplayTag Hit; };
		static const FSlotSfx SlotSfx[] = {
			{ ERTags::Ability_Slot_Q, ERTags::Pres_Sfx_SkillCast_Q, ERTags::Pres_Sfx_SkillHit_Q },
			{ ERTags::Ability_Slot_W, ERTags::Pres_Sfx_SkillCast_W, ERTags::Pres_Sfx_SkillHit_W },
			{ ERTags::Ability_Slot_E, ERTags::Pres_Sfx_SkillCast_E, ERTags::Pres_Sfx_SkillHit_E },
			{ ERTags::Ability_Slot_R, ERTags::Pres_Sfx_SkillCast_R, ERTags::Pres_Sfx_SkillHit_R },
			{ ERTags::Ability_Slot_D, ERTags::Pres_Sfx_SkillCast_D, ERTags::Pres_Sfx_SkillHit_D },
		};
		for (const FSlotSfx& S : SlotSfx)
		{
			const FGameplayTag SlotKey = bAttackCue ? S.Cast : S.Hit;
			if (Params.AggregatedSourceTags.HasTagExact(S.Slot) && Source->HasKey(SlotKey))
			{
				Key = SlotKey;
				break;
			}
		}
	}
	USoundBase* Sound = Source->PickSound(Key, FMath::RoundToInt(Params.RawMagnitude));   // 발 번호 (순차 사격) · 그 외 0
	// 공격음 = 시전자 위치 · 타격음 = 서버가 준 타격 지점 (Argument 49 — 이펙트가 생기면 같은 지점을 쓴다)
	const FVector Location = Params.Location.IsNearlyZero() ? GetOwner()->GetActorLocation() : FVector(Params.Location);
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location);
	}
	// 어느 층에서 왔나 — 스킨 소리는 무기 기본과 **파일명이 같아서** 이름만으론 구분이 안 된다 (Argument 49 3층)
	const FResolved* From = Source->Cache.Find(Key);
	UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 소리 %s [%s] ← %s 의 %s (%s)"), *GetNameSafe(GetOwner()),
		Sound ? *Sound->GetName() : TEXT("없음"), From ? From->Source : TEXT("줄 없음"), *GetNameSafe(Instigator), *Key.ToString(), NetTag(GetOwner()));

	// 타격 뒤 조금 늦게 한 번 더 (F19-02 매그너스 Q Impact · 사용자 2026-10-04 "Hit 보다 조금 느리게") — 같은 자리
	if (!bAttackCue && !bAimCue && !bReadyCue && !bBasic && EventSlot == ERTags::Ability_Slot_Q && Source->HasKey(ERTags::Pres_Sfx_SkillHitLate_Q))
	{
		constexpr float LateSeconds = 0.15f;   // [자체] — 들어보고 조절
		TWeakObjectPtr<USoundBase> Late = Source->PickSound(ERTags::Pres_Sfx_SkillHitLate_Q);
		FTimerHandle Unused;
		GetWorld()->GetTimerManager().SetTimer(Unused, FTimerDelegate::CreateWeakLambda(this, [this, Late, Location]()
		{
			if (USoundBase* S = Late.Get())
			{
				UGameplayStatics::PlaySoundAtLocation(this, S, Location);
				UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 소리 %s (타격 뒤 늦게 · %s)"), *GetNameSafe(GetOwner()), *S->GetName(), NetTag(GetOwner()));
			}
		}), LateSeconds, false);
	}
}

UAnimSequenceBase* UERPresentationComponent::FindFirstAnim(FGameplayTag Key) const
{
	if (const FResolved* R = Cache.Find(Key))
	{
		for (const TObjectPtr<UObject>& A : R->Entry->Assets)
		{
			if (UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(A)) { return Seq; }
		}
	}
	return nullptr;
}

void UERPresentationComponent::PushStateToAnim()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UERAnimInstance* Anim = Character && Character->GetMesh() ? Cast<UERAnimInstance>(Character->GetMesh()->GetAnimInstance()) : nullptr;
	if (!Anim)
	{
		return;
	}
	if (bDead)
	{
		UAnimSequenceBase* Death = FindFirstAnim(ERTags::Pres_Anim_Death);
		if (!Death && !Cache.IsEmpty())   // 비어 있으면 아직 SetBase 전 (OnRep 순서) — SetBase 의 Rebuild 가 다시 부른다
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[연출] %s 사망 애니가 없다 — 동작표에 Pres.Anim.Death 줄"), *GetNameSafe(GetOwner()));
		}
		Anim->SetDead(Death, bDeathSkipToEnd);
	}
	// 쉬는 자세 (F12.6-02) — 애니는 동작표에서 (없으면 null → 상태머신이 그 단계를 건너뛴다)
	Anim->SetRest(RestPose == EERRestPose::Beware, RestPose == EERRestPose::Sleep, bRestSkipIntro,
		FindFirstAnim(ERTags::Pres_Anim_BewareStart), FindFirstAnim(ERTags::Pres_Anim_BewareLoop), FindFirstAnim(ERTags::Pres_Anim_BewareEnd),
		FindFirstAnim(ERTags::Pres_Anim_SleepStart), FindFirstAnim(ERTags::Pres_Anim_SleepLoop), FindFirstAnim(ERTags::Pres_Anim_Wake));
}

void UERPresentationComponent::SetRestPose(EERRestPose Pose, bool bSkipIntro)
{
	if (RestPose == Pose)
	{
		return;
	}
	RestPose = Pose;
	bRestSkipIntro = bSkipIntro;
	PushStateToAnim();
	UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 쉬는 자세 %s%s (%s)"), *GetNameSafe(GetOwner()),
		Pose == EERRestPose::Beware ? TEXT("경계") : Pose == EERRestPose::Sleep ? TEXT("잠") : TEXT("없음"),
		bSkipIntro && Pose != EERRestPose::None ? TEXT(" · 늦게 받음 — 반복부터") : TEXT(""), NetTag(GetOwner()));
}

void UERPresentationComponent::SetDead(bool bSkipToEnd)
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	bDeathSkipToEnd = bSkipToEnd;
	StopActionAnim();   // 공격 중에 죽으면 그 모션을 끊고 쓰러진다
	PushStateToAnim();
	// 사망음 (F12.5-05) — 쓰러지는 걸 보는 머신만. 늦게 relevant 된 클라(이미 누워 있음)는 안 튼다. 신호는 복제 상태 bDead 라 큐가 필요 없다
	if (!bSkipToEnd && GetNetMode() != NM_DedicatedServer)
	{
		if (USoundBase* Die = PickSound(ERTags::Pres_Sfx_Die))
		{
			UGameplayStatics::PlaySoundAtLocation(this, Die, GetOwner()->GetActorLocation());
			UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 사망음 %s (%s)"), *GetNameSafe(GetOwner()), *Die->GetName(), NetTag(GetOwner()));
		}
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 사망 포즈 %s (%s)"), *GetNameSafe(GetOwner()),
		bSkipToEnd ? TEXT("누운 채로 (늦은 relevant)") : TEXT("쓰러짐"), NetTag(GetOwner()));
}

FString UERPresentationComponent::PlayEventPres(FGameplayTag AnimKey, FGameplayTag SfxKey)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return TEXT("데디 서버 · 안 틂");
	}
	FString Played;
	if (AnimKey.IsValid())
	{
		const ACharacter* Character = Cast<ACharacter>(GetOwner());
		UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
		UAnimSequenceBase* Seq = FindFirstAnim(AnimKey);
		// 에디터 몽타주(여러 섹션 — R Fire → End)는 그대로 · 시퀀스는 동적 몽타주로 (K8 · Argument 53 L2)
		if (UAnimMontage* Montage = Cast<UAnimMontage>(Seq))
		{
			if (Anim && Anim->Montage_Play(Montage) > 0.f)
			{
				Played = Montage->GetName();
			}
		}
		else if (Anim && Seq && Anim->PlaySlotAnimationAsDynamicMontage(Seq, AnimSlotName, BlendIn, BlendOut))
		{
			Played = Seq->GetName();
		}
	}
	if (SfxKey.IsValid())
	{
		if (USoundBase* Sound = PickSound(SfxKey))
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, GetOwner()->GetActorLocation());
			Played += (Played.IsEmpty() ? TEXT("") : TEXT(" · ")) + Sound->GetName();
		}
	}
	return Played.IsEmpty() ? FString(TEXT("동작표에 없음")) : Played;
}

void UERPresentationComponent::PushModeToAnim()
{
	// 스킨이 AnimBP 를 바꾸면 새 인스턴스에도 다시 알려야 해서 Rebuild 끝에서 매번 (값만 넘긴다 — 싸다)
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UERAnimInstance* Anim = Character && Character->GetMesh() ? Cast<UERAnimInstance>(Character->GetMesh()->GetAnimInstance()) : nullptr;
	if (!Anim)
	{
		return;
	}
	Anim->SetInMode(ActiveMode.IsValid());
	// 모드 애니 (Argument 42 ⑥ A2) — 모드 중일 때만 넘긴다. 해제되면 그대로 둔다: 상태머신의 End 가 아직 그 애니를 튼다
	if (ActiveMode.IsValid())
	{
		UAnimSequenceBase* Start = FindFirstAnim(ERTags::Pres_Anim_ModeStart);
		UAnimSequenceBase* Idle = FindFirstAnim(ERTags::Pres_Anim_ModeIdle);
		UAnimSequenceBase* Run = FindFirstAnim(ERTags::Pres_Anim_ModeRun);
		UAnimSequenceBase* End = FindFirstAnim(ERTags::Pres_Anim_ModeEnd);
		if (!Start || !Idle || !Run || !End)
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[연출] %s 모드 %s 애니가 비었다 — Start %s · Idle %s · Run %s · End %s (동작표에 모드 줄 Pres.Anim.Mode*)"),
				*GetNameSafe(GetOwner()), *ActiveMode.ToString(), *GetNameSafe(Start), *GetNameSafe(Idle), *GetNameSafe(Run), *GetNameSafe(End));
		}
		Anim->SetModeAnims(Start, Idle, Run, End);
	}
}

void UERPresentationComponent::ApplyWeaponLayer()
{
	// 데디 서버는 그리지 않고 그래프도 안 돈다 (P1) — 레이어 인스턴스를 만들 이유가 없다. 몽타주는 메인 인스턴스로 충분.
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* MeshComp = Character ? Character->GetMesh() : nullptr;
	const TObjectPtr<UERPresentationData>* Set = LoadedWeaponSets.Find(Weapon);
	const TSubclassOf<UAnimInstance> SetLayer = Set ? (*Set)->AnimLayer : nullptr;
	// 무기 없음 · 세트 로딩 중 · 세트에 레이어 없음 → 캐릭터 기본 표의 레이어 (맨손 대기 · 달리기 — 사용자 2026-09-28)
	// 모드 자세는 레이어 교체가 아니다 — 같은 무기 레이어의 ModeStart/Idle/Run/End 를 상태머신이 고른다 (Argument 42 ④ MB)
	const TSubclassOf<UAnimInstance> Desired = SetLayer ? SetLayer : (Base ? Base->AnimLayer : nullptr);
	if (Desired == LinkedLayer)
	{
		// 세트는 있는데 레이어가 비었다 — 조용히 넘어가면 "자세가 안 바뀐다" 의 원인을 못 찾는다 (2026-09-28)
		if (Set && !SetLayer)
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[연출] %s 무기 세트 %s 에 AnimLayer 가 없다 — ER.Pres.Fill 로 ABPL_<Char>_<무기> 연결 (%s)"),
				*GetNameSafe(GetOwner()), *GetNameSafe(*Set), NetTag(GetOwner()));
		}
		return;
	}
	if (!MeshComp || !MeshComp->GetSkeletalMeshAsset())
	{
		return;   // 스킨이 아직 메시를 안 넣었다 — SetSkin 뒤 Rebuild 에서 다시 온다 (2026-09-28 스폰마다 잡음 Warning)
	}
	if (!MeshComp->GetAnimInstance())
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[연출] %s 무기 레이어 %s 를 못 붙인다 — AnimInstance 없음 (메시 %s · AnimClass %s) (%s)"),
			*GetNameSafe(GetOwner()), *GetNameSafe(Desired.Get()), MeshComp ? *GetNameSafe(MeshComp->GetSkeletalMeshAsset()) : TEXT("없음"),
			MeshComp ? *GetNameSafe(MeshComp->GetAnimClass()) : TEXT("-"), NetTag(GetOwner()));
		return;
	}
	if (LinkedLayer)
	{
		MeshComp->UnlinkAnimClassLayers(LinkedLayer);
	}
	if (Desired)
	{
		MeshComp->LinkAnimClassLayers(Desired);
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 무기 레이어 %s → %s (%s)"),
		*GetNameSafe(GetOwner()), *GetNameSafe(LinkedLayer.Get()), *GetNameSafe(Desired.Get()), NetTag(GetOwner()));
	LinkedLayer = Desired;
}

namespace
{
	/**
	 * 선딜 구간 재생 속도 (Argument 52 T2) — **자르지 않는다** (사용자 2026-10-01 "0.25배 해도 됨"): 맞는 순간과 판정 일치가 목적이라
	 * 이 범위 밖이면 **경고만** [자체]. 안전 범위(아래 Safe*)는 마커를 엉뚱한 프레임에 찍은 사고만 막는다.
	 */
	constexpr float WarnMinCastRate = 0.5f;
	constexpr float WarnMaxCastRate = 2.0f;
	constexpr float SafeMinCastRate = 0.1f;
	constexpr float SafeMaxCastRate = 10.0f;

	/** "ER 타격 지점" 마커의 **실제 재생 시각** (초) — 시퀀스 RateScale 반영. 없으면 −1. */
	float FindHitMarkerSeconds(const UAnimSequenceBase* Anim)
	{
		for (const FAnimNotifyEvent& E : Anim->Notifies)
		{
			if (Cast<UERAnimNotify_HitMarker>(E.Notify))
			{
				const float Scale = FMath::IsNearlyZero(Anim->RateScale) ? 1.f : FMath::Abs(Anim->RateScale);
				return E.GetTriggerTime() / Scale;
			}
		}
		// 에디터 몽타주 (K8 — E `AM_Katja_Snipe_Skill03` = Skill03 → End) — 마커는 안의 시퀀스에 있다 → 몽타주 시각으로 옮긴다
		if (const UAnimMontage* Montage = Cast<UAnimMontage>(Anim); Montage && !Montage->SlotAnimTracks.IsEmpty())
		{
			for (const FAnimSegment& Seg : Montage->SlotAnimTracks[0].AnimTrack.AnimSegments)
			{
				const UAnimSequenceBase* Inner = Seg.GetAnimReference();
				const float InnerSec = Inner && Inner != Anim ? FindHitMarkerSeconds(Inner) : -1.f;
				if (InnerSec >= Seg.AnimStartTime && InnerSec <= Seg.AnimEndTime)
				{
					const float Rate = FMath::IsNearlyZero(Seg.AnimPlayRate) ? 1.f : FMath::Abs(Seg.AnimPlayRate);
					return Seg.StartPos + (InnerSec - Seg.AnimStartTime) / Rate;
				}
			}
		}
		return -1.f;
	}
}

void UERPresentationComponent::RestoreSkillAnimRate(UGameplayAbility* Ability)
{
	UAbilitySystemComponent* ASC = Ability ? Ability->GetAbilitySystemComponentFromActorInfo() : nullptr;
	UAnimMontage* Current = ASC ? ASC->GetCurrentMontage() : nullptr;
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!Current || !Anim || ASC->GetAnimatingAbility() != Ability || FMath::IsNearlyEqual(Anim->Montage_GetPlayRate(Current), 1.f))
	{
		return;
	}
	if (GetOwner()->HasAuthority())
	{
		ASC->CurrentMontageSetPlayRate(1.f);   // 복제 원천 — RepAnimMontageInfo 의 재생 속도가 다른 클라로 간다
	}
	else
	{
		Anim->Montage_SetPlayRate(Current, 1.f);   // 소유 클라 — 자기 것만 (ASC 판은 서버로 RPC 를 또 보낸다)
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 판정 순간 — 재생 속도 1.00 으로 (%s)"), *GetNameSafe(GetOwner()), NetTag(GetOwner()));
}

void UERPresentationComponent::PlayAbilityAnim(UGameplayAbility* Ability, const FGameplayAbilityActivationInfo& ActivationInfo, FGameplayTag Key, float AttackSpeed, float ExpectedSeconds, float CastTime)
{
	const FResolved* R = Cache.Find(Key);
	if (!R)
	{
		UE_LOG(LogEternalReturn, Verbose, TEXT("[연출] %s %s — 애니 없음, 판정만"), *GetNameSafe(GetOwner()), *Key.ToString());
		return;
	}

	// 변형 번갈아 — 애니가 아닌 애셋은 건너뛴다.
	TArray<UAnimSequenceBase*, TInlineAllocator<4>> Anims;
	for (const TObjectPtr<UObject>& A : R->Entry->Assets)
	{
		if (UAnimSequenceBase* Anim = Cast<UAnimSequenceBase>(A))
		{
			Anims.Add(Anim);
		}
	}
	if (Anims.IsEmpty())
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[연출] %s %s — 항목(%s)에 애니가 없다"), *GetNameSafe(GetOwner()), *Key.ToString(), R->Source);
		return;
	}
	int32& Next = NextVariant.FindOrAdd(Key);
	UAnimSequenceBase* Anim = Anims[Next % Anims.Num()];
	Next = (Next + 1) % Anims.Num();

	const float Length = Anim->GetPlayLength();
	// 평타 재생 속도 [자체] — 간격(1/공속)보다 긴 애니만 빨라진다. 느려지지는 않는다.
	float Rate = AttackSpeed > 0.f ? FMath::Max(1.f, Length * AttackSpeed) : 1.f;

	// ⭐ Argument 52 T2 — "ER 타격 지점" 마커가 있으면 선딜 구간을 **마커 시각 ÷ CastTime** 으로 튼다 → 맞는 순간이 CastTime 에 온다.
	//   판정 순간에 1배속으로 돌아간다 (RestoreSkillAnimRate — 어빌리티가 부른다). 마커가 없으면 지금처럼 1배속.
	FString MarkerNote;
	if (AttackSpeed <= 0.f && CastTime > 0.f)
	{
		const float MarkerSec = FindHitMarkerSeconds(Anim);
		if (MarkerSec > 0.f)
		{
			const float Raw = MarkerSec / CastTime;
			Rate = FMath::Clamp(Raw, SafeMinCastRate, SafeMaxCastRate);   // 사고 방지만 — 보통은 Raw 그대로
			const bool bUnusual = Raw < WarnMinCastRate || Raw > WarnMaxCastRate;
			MarkerNote = FString::Printf(TEXT(" · 타격 지점 %.2f초 → 선딜 %.2f초%s"), MarkerSec, CastTime,
				Rate != Raw ? *FString::Printf(TEXT(" (⚠ %.2f배 → 안전 범위 %.1f~%.0f 로 자름 — 마커 위치를 본다)"), Raw, SafeMinCastRate, SafeMaxCastRate)
				: bUnusual ? TEXT(" (⚠ 속도 큼 — 의도가 아니면 CastTime · 마커를 본다)") : TEXT(""));
			if (bUnusual)
			{
				static TSet<const UObject*> WarnedRate;   // 애니당 한 번
				if (!WarnedRate.Contains(Anim))
				{
					WarnedRate.Add(Anim);
					UE_LOG(LogEternalReturn, Warning, TEXT("[연출] %s 타격 지점 %.2f초 ÷ 선딜 %.2f초 = %.2f배 — 보통 범위(%.1f~%.1f) 밖. 의도가 아니면 CastTime 또는 마커를 본다"),
						*Anim->GetName(), MarkerSec, CastTime, Raw, WarnMinCastRate, WarnMaxCastRate);
				}
			}
		}
	}

	else if (AttackSpeed <= 0.f && CastTime <= 0.f)
	{
		// 선딜 0 인데 타격 지점 마커가 있다 — 판정(이동 · 발사)이 누르자마자 나가고 애니는 마커까지 걸린다 (2026-10-01 카티야 E "그 전에부터 이동").
		//   T2 는 선딜이 있을 때만 맞춘다 → 스킬 데이터 CastTime 을 이 마커 시각으로 (1배속)
		const float MarkerSec = FindHitMarkerSeconds(Anim);
		static TSet<const UObject*> WarnedNoCast;
		if (MarkerSec > 0.f && !WarnedNoCast.Contains(Anim))
		{
			WarnedNoCast.Add(Anim);
			UE_LOG(LogEternalReturn, Warning, TEXT("[연출] %s 타격 지점 %.2f초가 있는데 스킬 선딜(CastTime)이 0 — 판정이 애니보다 먼저 나간다. CastTime 을 %.2f 로"),
				*Anim->GetName(), MarkerSec, MarkerSec);
		}
	}

	// 여러 섹션 몽타주(채널 Loop · 발마다 Execute · K8)는 전체 길이가 스킬 시간과 무관 — 경고하지 않는다
	const UAnimMontage* AsMontage = Cast<UAnimMontage>(Anim);
	const bool bMultiSection = AsMontage && AsMontage->CompositeSections.Num() > 1;
	if (!bMultiSection && ExpectedSeconds > 0.f && Length / Rate > ExpectedSeconds * LengthWarnRatio)
	{
		static TSet<const UObject*> Warned;
		if (!Warned.Contains(Anim))
		{
			Warned.Add(Anim);
			UE_LOG(LogEternalReturn, Warning, TEXT("[연출] %s 길이 %.2f초가 스킬 시간(선딜+후딜) %.2f초의 %.1f배를 넘는다 — 판정과 모션이 어긋난다"),
				*Anim->GetName(), Length / Rate, ExpectedSeconds, LengthWarnRatio);
		}
	}

	UAbilitySystemComponent* ASC = Ability ? Ability->GetAbilitySystemComponentFromActorInfo() : nullptr;
	if (!ASC)
	{
		return;
	}
	// Argument 53 L2 — 에디터 몽타주(여러 단계 · 섹션 반복)는 그대로 튼다. 복제는 ASC 가 같은 길(RepAnimMontageInfo)로
	UAnimMontage* Montage = nullptr;
	if (UAnimMontage* Authored = Cast<UAnimMontage>(Anim))
	{
		if (!Authored->IsValidSlot(AnimSlotName))
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[연출] %s 몽타주 %s 에 슬롯 %s 트랙이 없다 — 재생해도 안 보인다 (몽타주 슬롯을 맞춘다)"),
				*GetNameSafe(GetOwner()), *Authored->GetName(), *AnimSlotName.ToString());
		}
		if (ASC->PlayMontage(Ability, ActivationInfo, Authored, Rate) > 0.f)
		{
			Montage = Authored;
		}
	}
	else
	{
		Montage = ASC->PlaySlotAnimationAsDynamicMontage(Ability, ActivationInfo, Anim, AnimSlotName, BlendIn, BlendOut, Rate);
	}
	// 재생 성공이면 ASC 의 현재 몽타주가 된다 (AbilitySystemComponent_Abilities.cpp PlayMontage — AnimInstance 가 없거나 스켈레톤이 안 맞으면 실패).
	if (!Montage || ASC->GetCurrentMontage() != Montage)
	{
		const ACharacter* Character = Cast<ACharacter>(GetOwner());
		const USkeletalMeshComponent* MeshComp = Character ? Character->GetMesh() : nullptr;
		const USkeletalMesh* MeshAsset = MeshComp ? MeshComp->GetSkeletalMeshAsset() : nullptr;
		// 액터당 한 번만 Warning — 원인(AnimBP 없음 · 스켈레톤)은 고칠 때까지 같아서 평타마다 찍으면 로그가 묻힌다.
		const FString Msg = FString::Printf(TEXT("[연출] %s %s 재생 실패 — AnimInstance %s · 메시 스켈레톤 %s · 애니 스켈레톤 %s (%s)"),
			*GetNameSafe(GetOwner()), *Anim->GetName(), MeshComp && MeshComp->GetAnimInstance() ? TEXT("있음") : TEXT("없음"),
			MeshAsset ? *GetNameSafe(MeshAsset->GetSkeleton()) : TEXT("없음"), *GetNameSafe(Anim->GetSkeleton()), NetTag(GetOwner()));
		if (bWarnedPlayFail)
		{
			UE_LOG(LogEternalReturn, Verbose, TEXT("%s"), *Msg);
		}
		else
		{
			bWarnedPlayFail = true;
			UE_LOG(LogEternalReturn, Warning, TEXT("%s (이 액터는 이후 Verbose)"), *Msg);
		}
		return;
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s %s 재생 %s (%s) ×%.2f%s (%s)"),
		*GetNameSafe(GetOwner()), *Key.ToString(), *Anim->GetName(), R->Source, Rate, *MarkerNote, NetTag(GetOwner()));
}

void UERPresentationComponent::StopActionAnim()
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	UAnimMontage* Current = ASC ? ASC->GetCurrentMontage() : nullptr;
	if (!Current)
	{
		return;
	}
	// 장판 동안 도는 모션(매그너스 W)은 걸어도 유지 — 끝은 태그가 빠질 때 End 섹션 (Argument 63 M1). 죽으면 그래도 끊는다
	if (!bDead && ASC->HasMatchingGameplayTag(ERTags::State_AnimHold))
	{
		UE_LOG(LogEternalReturn, Verbose, TEXT("[연출] %s 이동 — 모션 유지 중이라 안 끊음 (%s)"), *GetNameSafe(GetOwner()), NetTag(GetOwner()));
		return;
	}
	ASC->CurrentMontageStop(BlendOut);
	// Log — 재생 중일 때만 찍힌다 (위에서 조기 반환). 검증 증거로 남긴다 (2026-09-27 Verbose 라 확인 못 함).
	UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 이동 — 모션 끊음 %s (%s)"),
		*GetNameSafe(GetOwner()), *GetNameSafe(Current->IsDynamicMontage() ? Current->GetFirstAnimReference() : Current), NetTag(GetOwner()));
}

void UERPresentationComponent::BindModeTags(UAbilitySystemComponent* InASC)
{
	if (!InASC || ModeASC.Get() == InASC)
	{
		return;
	}
	if (UAbilitySystemComponent* Old = ModeASC.Get())
	{
		Old->RegisterGameplayTagEvent(ERTags::Mode, EGameplayTagEventType::AnyCountChange).Remove(ModeTagHandle);
		Old->RegisterGameplayTagEvent(ERTags::State_Gathering, EGameplayTagEventType::NewOrRemoved).Remove(GatherTagHandle);
		Old->RegisterGameplayTagEvent(ERTags::State_AnimHold, EGameplayTagEventType::NewOrRemoved).Remove(AnimHoldTagHandle);
		UnbindPropTags(Old);
		for (const TPair<FGameplayTag, FDelegateHandle>& H : LoopSfxTagHandles)
		{
			Old->RegisterGameplayTagEvent(H.Key, EGameplayTagEventType::NewOrRemoved).Remove(H.Value);
		}
		LoopSfxTagHandles.Reset();
	}
	ModeASC = InASC;
	// 부모 태그(Mode)로 구독 — 자식(Mode.Sniper …)이 늘고 줄 때도 부모 개수가 바뀌어 불린다
	ModeTagHandle = InASC->RegisterGameplayTagEvent(ERTags::Mode, EGameplayTagEventType::AnyCountChange)
		.AddUObject(this, &UERPresentationComponent::OnModeTagChanged);
	// 채집 (04 · Argument 46 · 47) — 모드와 같은 길: 서버가 복제 loose 태그 → 각 머신이 자기 화면에 몽타주
	GatherTagHandle = InASC->RegisterGameplayTagEvent(ERTags::State_Gathering, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UERPresentationComponent::OnGatherTagChanged);
	AnimHoldTagHandle = InASC->RegisterGameplayTagEvent(ERTags::State_AnimHold, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UERPresentationComponent::OnAnimHoldTagChanged);
	BindPropTags();   // 소품 켜는 태그 (Argument 64 B1) — 이미 타고 있으면 바로 붙는다
	for (const FLoopSfx& L : LoopSfxTable())
	{
		LoopSfxTagHandles.Add(L.Tag, InASC->RegisterGameplayTagEvent(L.Tag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UERPresentationComponent::OnLoopSfxTagChanged));
	}
	RefreshLoopSfx(/*bPlayStart=*/false);   // 늦게 들어온 클라 — 이미 도는 중이면 반복만
	RefreshMode();   // 이미 모드 중에 붙었을 수 있다 (늦은 relevant · 부활)
}

void UERPresentationComponent::OnAnimHoldTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	UAbilitySystemComponent* ASC = ModeASC.Get();
	UAnimMontage* Current = ASC ? ASC->GetCurrentMontage() : nullptr;
	if (NewCount > 0)
	{
		// 진단 — 반복하려면 Loop 섹션의 다음이 Loop 여야 한다 (몽타주 Sections 패널 연결)
		const int32 LoopIdx = Current ? Current->GetSectionIndex(ERPresSection::Loop) : INDEX_NONE;
		UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 모션 유지 시작 — 몽타주 %s · Loop 다음 = %s · End %s (%s)"), *GetNameSafe(GetOwner()),
			Current ? *Current->GetName() : TEXT("없음"),
			LoopIdx != INDEX_NONE ? *Current->CompositeSections[LoopIdx].NextSectionName.ToString() : TEXT("Loop 섹션 없음"),
			Current && Current->IsValidSectionName(ERPresSection::End) ? TEXT("있음") : TEXT("없음"), NetTag(GetOwner()));
		return;
	}
	if (!Current && Pawn && (Pawn->HasAuthority() || Pawn->IsLocallyControlled()))
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 모션 유지 끝 — 몽타주가 이미 끝나 있다 (Loop → Loop 연결 확인) (%s)"), *GetNameSafe(GetOwner()), NetTag(GetOwner()));
	}
	// 다른 클라(시뮬레이티드)는 서버 섹션 복제를 따른다 · 그 사이 다른 스킬이 덮었으면 End 가 없거나 이미 끝났다
	if (!Pawn || !(Pawn->HasAuthority() || Pawn->IsLocallyControlled()) || !Current || !Current->IsValidSectionName(ERPresSection::End))
	{
		return;
	}
	if (Pawn->HasAuthority())
	{
		ASC->CurrentMontageJumpToSection(ERPresSection::End);
	}
	else if (const ACharacter* Character = Cast<ACharacter>(Pawn); Character && Character->GetMesh() && Character->GetMesh()->GetAnimInstance())
	{
		Character->GetMesh()->GetAnimInstance()->Montage_JumpToSection(ERPresSection::End, Current);
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 모션 유지 끝 — %s 섹션 End → (%s)"), *GetNameSafe(GetOwner()), *Current->GetName(), NetTag(GetOwner()));
}

void UERPresentationComponent::OnGatherTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UERAnimInstance* Anim = Character && Character->GetMesh() ? Cast<UERAnimInstance>(Character->GetMesh()->GetAnimInstance()) : nullptr;
	if (NewCount <= 0)
	{
		// ⭐ collect 에는 일어서는 구간이 없다 (전 구간 웅크림 — Argument 47 재측정). 일어서기 = 이 블렌드 아웃.
		//   0.2초면 한 번에 서 버린다 (사용자 2026-09-29) → 길고 부드럽게.
		if (Anim && GatherMontage.IsValid() && Anim->Montage_IsPlaying(GatherMontage.Get()))
		{
			FAlphaBlendArgs StandUp(GatherBlendOut);
			StandUp.BlendOption = EAlphaBlendOption::HermiteCubic;
			Anim->Montage_StopWithBlendOut(StandUp, GatherMontage.Get());
		}
		GatherMontage.Reset();
		UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 채집 끝 — %.1f초 동안 일어선다 (%s)"), *GetNameSafe(GetOwner()), GatherBlendOut, NetTag(GetOwner()));
		return;
	}
	UAnimSequenceBase* Seq = FindFirstAnim(ERTags::Pres_Anim_Gather);
	if (!Anim || !Seq)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[연출] %s 채집 모션 없음 — AnimInstance %s · 애니 %s (동작표 Pres.Anim.Gather · ER.Pres.Fill)"),
			*GetNameSafe(GetOwner()), *GetNameSafe(Anim), *GetNameSafe(Seq));
		return;
	}
	// 반복 = 올림(채집 시간 ÷ 길이) — 채집이 끝나기 전에 모션이 먼저 끝나지 않게. 끝은 태그 제거가 멈춘다. 속도는 원래 그대로 (사용자 2026-09-29 "재생 속도 건드리는건 아닌거 같아")
	const AERPlayerState* PS = ModeASC.IsValid() ? Cast<AERPlayerState>(ModeASC->GetOwnerActor()) : nullptr;
	const float Seconds = PS ? PS->GetGatherSeconds() : 0.f;
	const float Length = Seq->GetPlayLength();
	const int32 Loops = (Seconds > 0.f && Length > 0.f) ? FMath::Max(1, FMath::CeilToInt(Seconds / Length)) : 1;
	UAnimMontage* Montage = Anim->PlaySlotAnimationAsDynamicMontage(Seq, AnimSlotName, BlendIn, BlendOut, 1.f, Loops);
	GatherMontage = Montage;   // 움직이면 AnimInstance 가 끊는다 (Argument 48 ③ M1)
	UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 채집 시작 — %s %.2f초 ×%d (채집 %.2f초) (%s)"), *GetNameSafe(GetOwner()),
		*Seq->GetName(), Length, Loops, Seconds, NetTag(GetOwner()));
}

void UERPresentationComponent::OnModeTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	RefreshMode();
}

void UERPresentationComponent::RefreshMode()
{
	const UAbilitySystemComponent* ASC = ModeASC.Get();
	// 붙어 있는 모드 태그 중 첫 번째 (모드는 한 번에 하나 — 둘이면 경고)
	FGameplayTag NewMode;
	if (ASC)
	{
		FGameplayTagContainer Owned;
		ASC->GetOwnedGameplayTags(Owned);
		for (const FGameplayTag& T : Owned)
		{
			if (T.MatchesTag(ERTags::Mode) && T != ERTags::Mode)
			{
				if (NewMode.IsValid())
				{
					UE_LOG(LogEternalReturn, Warning, TEXT("[연출] %s 모드가 둘 — %s · %s (앞의 것을 쓴다)"), *GetNameSafe(GetOwner()), *NewMode.ToString(), *T.ToString());
					break;
				}
				NewMode = T;
			}
		}
	}
	if (NewMode == ActiveMode)
	{
		return;
	}
	const FGameplayTag OldMode = ActiveMode;
	ActiveMode = NewMode;
	Rebuild();   // 모드 층(사건 덮어쓰기) + AnimInstance bInMode — 진입 · 해제 포즈는 상태머신이 (Argument 42 ④ MB)
	UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 모드 %s → %s (%s)"), *GetNameSafe(GetOwner()),
		OldMode.IsValid() ? *OldMode.ToString() : TEXT("없음"), NewMode.IsValid() ? *NewMode.ToString() : TEXT("없음"), NetTag(GetOwner()));
}

void UERPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* ASC = ModeASC.Get())
	{
		ASC->RegisterGameplayTagEvent(ERTags::Mode, EGameplayTagEventType::AnyCountChange).Remove(ModeTagHandle);
		ASC->RegisterGameplayTagEvent(ERTags::State_Gathering, EGameplayTagEventType::NewOrRemoved).Remove(GatherTagHandle);
		ASC->RegisterGameplayTagEvent(ERTags::State_AnimHold, EGameplayTagEventType::NewOrRemoved).Remove(AnimHoldTagHandle);
		UnbindPropTags(ASC);
		for (const TPair<FGameplayTag, FDelegateHandle>& H : LoopSfxTagHandles)
		{
			ASC->RegisterGameplayTagEvent(H.Key, EGameplayTagEventType::NewOrRemoved).Remove(H.Value);
		}
	}
	LoopSfxTagHandles.Reset();
	ModeASC.Reset();
	ModeTagHandle.Reset();
	GatherTagHandle.Reset();
	AnimHoldTagHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void UERPresentationComponent::OnLoopSfxTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	RefreshLoopSfx(/*bPlayStart=*/NewCount > 0);
}

void UERPresentationComponent::RefreshLoopSfx(bool bPlayStart)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;   // 소리를 안 튼다
	}
	const UAbilitySystemComponent* ASC = ModeASC.Get();
	for (const FLoopSfx& L : LoopSfxTable())
	{
		const bool bWant = ASC && ASC->HasMatchingGameplayTag(L.Tag) && HasKey(L.Loop);
		TObjectPtr<UAudioComponent>* Playing = LoopAudio.Find(L.Loop);
		if (bWant && !Playing)
		{
			if (bPlayStart && L.Start.IsValid())
			{
				if (USoundBase* Start = PickSound(L.Start))
				{
					UGameplayStatics::SpawnSoundAttached(Start, GetOwner()->GetRootComponent());
				}
			}
			UAudioComponent* Audio = UGameplayStatics::SpawnSoundAttached(PickSound(L.Loop), GetOwner()->GetRootComponent(),
				NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, /*bStopWhenAttachedToDestroyed=*/true,
				1.f, 1.f, 0.f, nullptr, nullptr, /*bAutoDestroy=*/false);
			if (Audio)
			{
				Audio->OnAudioFinished.AddDynamic(this, &UERPresentationComponent::OnLoopAudioFinished);
				LoopAudio.Add(L.Loop, Audio);
				UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 반복 소리 시작 %s (%s 동안 · %s)"), *GetNameSafe(GetOwner()),
					*GetNameSafe(Audio->Sound), *L.Tag.ToString(), NetTag(GetOwner()));
			}
		}
		else if (!bWant && Playing)
		{
			if (UAudioComponent* Audio = Playing->Get())
			{
				Audio->OnAudioFinished.RemoveAll(this);
				Audio->Stop();
				Audio->DestroyComponent();
			}
			LoopAudio.Remove(L.Loop);
			UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 반복 소리 멈춤 (%s 빠짐 · %s)"), *GetNameSafe(GetOwner()), *L.Tag.ToString(), NetTag(GetOwner()));
		}
	}
}

void UERPresentationComponent::OnLoopAudioFinished()
{
	// 한 번 다 돌았다 — 태그가 아직 있으면 다시 (소리 애셋이 반복 설정이 아니어도 이어진다)
	for (const TPair<FGameplayTag, TObjectPtr<UAudioComponent>>& P : LoopAudio)
	{
		if (UAudioComponent* Audio = P.Value.Get(); Audio && !Audio->IsPlaying())
		{
			Audio->Play();
		}
	}
}

const FERAttachProp* UERPresentationComponent::FindProp(FGameplayTag Key) const
{
	return Skin ? Skin->Props.Find(Key) : nullptr;
}

void UERPresentationComponent::SpawnPieces(AActor* Owner, USceneComponent* Parent, const TArray<FERAttachPiece>& Pieces, TArray<TObjectPtr<USceneComponent>>& OutComps)
{
	if (!Owner || !Parent || Owner->GetNetMode() == NM_DedicatedServer)
	{
		return;   // 그리지 않는 머신 — 판정과 무관
	}
	for (const FERAttachPiece& P : Pieces)
	{
		UMeshComponent* Comp = nullptr;
		if (USkeletalMesh* SK = Cast<USkeletalMesh>(P.Mesh))
		{
			USkeletalMeshComponent* C = NewObject<USkeletalMeshComponent>(Owner);
			C->SetSkeletalMesh(SK);
			Comp = C;
		}
		else if (UStaticMesh* SM = Cast<UStaticMesh>(P.Mesh))
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
			C->SetStaticMesh(SM);
			Comp = C;
		}
		if (!Comp)
		{
			continue;   // 빈 칸 — Fill 이 못 찾았으면 그때 에러를 냈다
		}
		if (!P.Socket.IsNone() && !Parent->DoesSocketExist(P.Socket))
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[부착] %s — %s 에 소켓 %s 가 없다 → 원점에 붙인다 (스켈레톤에 소켓을 만든다)"),
				*GetNameSafe(Owner), *GetNameSafe(Parent), *P.Socket.ToString());
		}
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetupAttachment(Parent, P.Socket);
		Comp->SetRelativeTransform(P.Offset);
		Comp->RegisterComponent();
		OutComps.Add(Comp);
	}
}

void UERPresentationComponent::RefreshWeaponAttach()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	for (USceneComponent* C : WeaponComps)
	{
		if (C) { C->DestroyComponent(); }
	}
	WeaponComps.Reset();
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const FERAttachPieces* Want = Skin && Weapon != EERWeaponType::None ? Skin->WeaponMeshes.Find(Weapon) : nullptr;
	if (!Character || !Want)
	{
		return;
	}
	SpawnPieces(GetOwner(), Character->GetMesh(), Want->Pieces, WeaponComps);
	if (!WeaponComps.IsEmpty())
	{
		const FERAttachPiece& First = Want->Pieces[0];
		UE_LOG(LogEternalReturn, Log, TEXT("[부착] %s 무기 %s ← %s @%s (%d조각 · %s)"), *GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Weapon),
			*GetNameSafe(First.Mesh), *First.Socket.ToString(), WeaponComps.Num(), NetTag(GetOwner()));
	}
}

void UERPresentationComponent::BindPropTags()
{
	UAbilitySystemComponent* ASC = ModeASC.Get();
	UnbindPropTags(ASC);
	if (ASC && Skin)
	{
		for (const TPair<FGameplayTag, FERAttachProp>& P : Skin->Props)
		{
			const FGameplayTag Show = P.Value.ShowWhile;
			if (Show.IsValid() && !PropTagHandles.Contains(Show))
			{
				PropTagHandles.Add(Show, ASC->RegisterGameplayTagEvent(Show, EGameplayTagEventType::NewOrRemoved)
					.AddUObject(this, &UERPresentationComponent::OnPropTagChanged));
			}
		}
	}
	RefreshProps();
}

void UERPresentationComponent::UnbindPropTags(UAbilitySystemComponent* ASC)
{
	if (ASC)
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& H : PropTagHandles)
		{
			ASC->RegisterGameplayTagEvent(H.Key, EGameplayTagEventType::NewOrRemoved).Remove(H.Value);
		}
	}
	PropTagHandles.Reset();
}

void UERPresentationComponent::OnPropTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	RefreshProps();
}

void UERPresentationComponent::RefreshProps()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;   // 그리지 않는다 — 판정과 무관 (Argument 64 네트워크)
	}
	const UAbilitySystemComponent* ASC = ModeASC.Get();
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	// 켜야 할 것 — 스킨에 있고 켜는 태그가 붙은 소품
	TSet<FGameplayTag> Want;
	if (Skin && ASC)
	{
		for (const TPair<FGameplayTag, FERAttachProp>& P : Skin->Props)
		{
			if (P.Value.ShowWhile.IsValid() && ASC->HasMatchingGameplayTag(P.Value.ShowWhile))
			{
				Want.Add(P.Key);
			}
		}
	}
	// 끄기 — 태그가 빠졌거나 스킨이 바뀌어 없어진 소품
	for (auto It = ActiveProps.CreateIterator(); It; ++It)
	{
		if (!Want.Contains(It.Key()))
		{
			for (USceneComponent* C : It.Value().Comps)
			{
				if (C) { C->DestroyComponent(); }
			}
			UE_LOG(LogEternalReturn, Log, TEXT("[부착] %s 소품 %s 꺼짐 (%s)"), *GetNameSafe(GetOwner()), *It.Key().ToString(), NetTag(GetOwner()));
			It.RemoveCurrent();
		}
	}
	// 켜기
	for (const FGameplayTag& Key : Want)
	{
		if (ActiveProps.Contains(Key) || !Character)
		{
			continue;
		}
		FERSpawnedAttach& Spawned = ActiveProps.Add(Key);
		SpawnPieces(GetOwner(), Character->GetMesh(), Skin->Props[Key].Pieces, Spawned.Comps);
		UE_LOG(LogEternalReturn, Log, TEXT("[부착] %s 소품 %s 켜짐 %d조각 (%s)"), *GetNameSafe(GetOwner()), *Key.ToString(), Spawned.Comps.Num(), NetTag(GetOwner()));
	}
}

void UERPresentationComponent::DumpToLog() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const USkeletalMeshComponent* MeshComp = Character ? Character->GetMesh() : nullptr;
	const USkeletalMesh* MeshAsset = MeshComp ? MeshComp->GetSkeletalMeshAsset() : nullptr;
	UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s (%s) — 기본 %s · 스킨 %s · 무기 %s (세트 %s) · 로드된 세트 %d · 메시 %s (스켈레톤 %s · 회전 %s) · 틱 옵션 %s · %d키"),
		*GetNameSafe(GetOwner()), NetTag(GetOwner()), *GetNameSafe(Base), *GetNameSafe(Skin), *UEnum::GetValueAsString(Weapon),
		LoadedWeaponSets.Contains(Weapon) ? TEXT("로드됨") : TEXT("없음/로딩"), LoadedWeaponSets.Num(),
		*GetPathNameSafe(MeshAsset), MeshAsset ? *GetNameSafe(MeshAsset->GetSkeleton()) : TEXT("-"),
		MeshComp ? *MeshComp->GetRelativeRotation().ToString() : TEXT("-"),
		MeshComp ? *UEnum::GetValueAsString(MeshComp->VisibilityBasedAnimTickOption) : TEXT("-"), Cache.Num());
	const TObjectPtr<UERPresentationData>* Set = LoadedWeaponSets.Find(Weapon);
	UE_LOG(LogEternalReturn, Log, TEXT("[연출]   AnimInstance %s · 붙은 레이어 %s · 세트의 레이어 %s · 모드 %s"),
		MeshComp ? *GetNameSafe(MeshComp->GetAnimInstance()) : TEXT("-"), *GetNameSafe(LinkedLayer.Get()),
		Set ? *GetNameSafe((*Set)->AnimLayer.Get()) : TEXT("(세트 없음)"),
		ActiveMode.IsValid() ? *ActiveMode.ToString() : TEXT("없음"));
	for (const TPair<FGameplayTag, FResolved>& P : Cache)
	{
		FString Names;
		for (const TObjectPtr<UObject>& A : P.Value.Entry->Assets)
		{
			Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + GetNameSafe(A);
		}
		UE_LOG(LogEternalReturn, Log, TEXT("[연출]   %-24s ← %-8s %s"), *P.Key.ToString(), P.Value.Source, *Names);
	}
}
