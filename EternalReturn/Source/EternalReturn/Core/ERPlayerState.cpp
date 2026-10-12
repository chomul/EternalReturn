// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ERPlayerState.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GAS/ERCCLibrary.h"
#include "GAS/ERAttributeSet.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"
#include "Item/ERInventoryComponent.h"
#include "Growth/ERGrowthComponent.h"
#include "Growth/ERGrowthSettings.h"
#include "Character/ERCharacterBase.h"
#include "Character/ERCharacterData.h"
#include "Item/ERItemData.h"
#include "Weapon/ERWeaponLibrary.h"
#include "Weapon/ERWeaponTypes.h"
#include "Weapon/ERWeaponRangeEffect.h"
#include "Weapon/ERUnarmedEffect.h"
#include "Combat/ERCombatSettings.h"
#include "GAS/ERSkillPhaseEffect.h"
#include "Net/UnrealNetwork.h"
#include "Item/ERItemDropActor.h"
#include "Presentation/ERPresentationComponent.h"
#include "Presentation/ERSoundSubsystem.h"
#include "GAS/ERDownedEffects.h"
#include "GAS/ERReviveAbility.h"
#include "GAS/ERPassiveAbility.h"
#include "Core/ERTeamStatics.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

AERPlayerState::AERPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// ⭐ Mixed: 소유 클라에는 전체 정보, 나머지 클라에는 최소 정보만 보낸다.
	//   ASC 기본값은 Full 이다(AbilitySystemComponent.cpp 생성자). 24명 매치에서
	//   남의 활성 이펙트 전체 목록까지 보내는 건 낭비다.
	//   Minimal 은 쓸 수 없다 — 엔진 주석이 소유자 있는 ASC 에서 동작하지 않는다고 명시한다
	//   (AbilitySystemComponent.h:87). 야생동물(소유 컨트롤러 없음)만 Minimal 을 쓴다.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// ⚠ APlayerState 기본값은 1 이다(PlayerState.cpp:26). 그대로 두면
	//   체력바가 초당 1회 갱신되어 전투가 성립하지 않는다.
	//   ⚠ 100 은 자체 결정 초기값이며 측정하지 않았다. APlayerState 는
	//   bAlwaysRelevant = true 라 24명분이 전원에게 가므로, 봇 24명 + NET ACTIVE +
	//   stat unit 으로 재본 뒤 낮춘다.
	NetUpdateFrequency = 100.f;

	// 생성자에서 만든 AttributeSet 은 ASC 가 자동으로 SpawnedAttributes 에 등록한다.
	// 초기값은 여기서 넣지 않는다 - 실험체별 값을 데이터 테이블에서 읽어 GE 로 적용한다.
	AttributeSet = CreateDefaultSubobject<UERAttributeSet>(TEXT("AttributeSet"));

	// 인벤토리 — PlayerState (Argument 21). 컴포넌트 자체가 복제되고, 안의 장착 배열은 컴포넌트가 DOREPLIFETIME 한다.
	Inventory = CreateDefaultSubobject<UERInventoryComponent>(TEXT("Inventory"));

	// 성장 — 같은 이유로 PlayerState (Argument 22). 레벨은 전원, 경험치는 소유자만 컴포넌트가 복제한다.
	Growth = CreateDefaultSubobject<UERGrowthComponent>(TEXT("Growth"));
}

void AERPlayerState::BeginPlay()
{
	Super::BeginPlay();

	// ⚠ 서버에서만 건다. 둔화 재계산은 서버 권위이고, 결과인 MoveSpeed 는
	//   어트리뷰트라 클라에 복제된다 (CLAUDE.md §6).
	if (HasAuthority())
	{
		ERCC::BindSlowRecalculation(AbilitySystemComponent);

		// 아군 소생 (F14 · Argument 75 R1) — 스킬 DA 없는 공용 행동. ASC 가 여기라 폰이 바뀌어도 남는다
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UERReviveAbility::StaticClass(), 1));

		// 빈사 스킬 (F14 S4 · Argument 79 T1) — 모든 실험체 같은 3개 · 빈사일 때만 발동 (UERDownedAbility). 소생과 같은 자리
		{
			TArray<TObjectPtr<UERSkillData>> Downed;
			for (const TSoftObjectPtr<UERSkillData>& Soft : UERCombatSettings::Get().DownedSkills)
			{
				if (UERSkillData* Skill = Soft.LoadSynchronous())
				{
					Downed.Add(Skill);
				}
				else
				{
					// GAS 처럼 조용히 건너뛰지 않는다 — 경로가 틀렸거나 ER.Skill.ImportJson Downed 전
					UE_LOG(LogEternalReturn, Warning, TEXT("[빈사 스킬] ER Combat DownedSkills — 애셋이 없다: %s"), *Soft.ToString());
				}
			}
			ERSkill::GrantSkills(AbilitySystemComponent, Downed, DownedSkills);
			AbilitySystemComponent->RegisterGameplayTagEvent(ERTags::State_DownedSkill_Obstinacy, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AERPlayerState::OnObstinacyChanged);
		}

		// ⭐ 스킬 포인트 지급 (F10-03) — 시작 포인트 + 레벨업마다 테이블 행의 SkillPointGranted.
		//   Growth 가 스탯 GE 를 먼저 적용한 뒤 브로드캐스트한다 (F10-02). P 도 포인트로 찍는다 — 자동 강화 없음.
		AddSkillPoints(UERGrowthSettings::Get().StartingSkillPoints);

		// ⭐ 무기 교체 → 숙련도 증폭 GE 갱신 (F10-04). 장비 GE 와 별개 핸들. 제작(F09-02) → 무기 숙련도.
		if (Inventory && Growth)
		{
			Inventory->OnEquippedChanged.AddWeakLambda(this, [this](EEREquipSlot Slot)
			{
				if (Slot == EEREquipSlot::Weapon)
				{
					RefreshWeaponSkills();            // F11-02: 회수 → 부여 → 사거리
					Growth->RefreshProficiencyBonus();
				}
			});
			// 시작은 무기 없음 — State.Unarmed (평타 · 스킬 차단). 사거리는 InitDefaultStats 가 맨몸 값을 이미 넣는다.
			RefreshWeaponSkills();
			Inventory->OnItemCrafted.AddWeakLambda(this, [this](FName ResultId, bool bFirstTime)
			{
				Growth->OnItemCrafted(ResultId, bFirstTime);
				// 제작 소리 (주변 · 여러 개 이어서 — 공용 ER Sound Craft 의 Count · 사용자 2026-10-08)
				if (APawn* CraftPawn = GetPawn())
				{
					UERPresentationComponent::SendSfxCue(CraftPawn, ERTags::Pres_Sfx_Craft, CraftPawn->GetActorLocation());
				}
				// 제작 음성 (Argument 73 · 본인) — 결과 등급. 일반은 대사 없음 · 영웅 = craftEpic · 초월 = craftMystic
				if (const FERItemRow* Row = ERItem::Find(ResultId))
				{
					const FGameplayTag ByGrade[] = { FGameplayTag(), ERTags::Pres_Voice_Craft_Uncommon, ERTags::Pres_Voice_Craft_Rare,
						ERTags::Pres_Voice_Craft_Epic, ERTags::Pres_Voice_Craft_Legend, ERTags::Pres_Voice_Craft_Mystic };
					const int32 G = static_cast<int32>(Row->Grade);
					if (G >= 0 && G < static_cast<int32>(UE_ARRAY_COUNT(ByGrade)) && ByGrade[G].IsValid())
					{
						ClientPlayVoice(ByGrade[G]);
					}
				}
			});
			// ⭐ 숙련도 레벨업 → D 해금 · 강화 (F11-03). 무기 트랙 · 장착 계열의 이벤트만 — 다른 계열은 교체할 때 맞춘다.
			//   나머지 5 트랙(방어 · 사냥 · 제작 · 탐색 · 이동)의 레벨 효과는 (미확인) — F10-05 는 적립 · 레벨업까지.
			Growth->OnProficiencyLevelUp.AddWeakLambda(this, [this](const FERProficiencyKey& Key, int32 /*NewLevel*/)
			{
				if (Key.Track != EERProficiencyTrack::Weapon)
				{
					return;
				}
				const FName WeaponId = Inventory->GetEquippedItem(EEREquipSlot::Weapon);
				const FERItemRow* Item = WeaponId.IsNone() ? nullptr : ERItem::Find(WeaponId);
				if (Item && Item->WeaponType == Key.WeaponType && SyncWeaponSkillLevel())
				{
					ClientPlayVoice(ERTags::Pres_Voice_LearnWeaponSkill);   // D 처음 해금 (Argument 73 · 본인 · 무기 세트 줄)
				}
			});
			// 상자 열기 → 탐색 숙련도 (F10-05)
			Inventory->OnBoxOpened.AddWeakLambda(this, [this](FName LootRow)
			{
				Growth->OnBoxOpened(LootRow);
			});
		}
		if (Growth)
		{
			Growth->OnLevelUp.AddWeakLambda(this, [this](int32 NewLevel)
			{
				// Lv.N-1 → N 행이 이번 레벨업의 지급량이다.
				if (const FERLevelExpRow* Row = UERGrowthComponent::FindLevelRow(NewLevel - 1))
				{
					AddSkillPoints(Row->SkillPointGranted);
				}
			});
		}

		// ⭐ 사망 → 시체 드롭 (F08-05). F02-04 가 "알리기만 한다" 로 둔 델리게이트를 여기서 받는다.
		//   사망 처리 자체(폰 파괴 · 부활)는 F14 — 여기서는 시체만 만든다. 원본 인벤토리는 그대로 (원작 확인: 복사 드롭).
		if (AttributeSet)
		{
			// 체력 0 → 빈사 / 사망 / 전멸 (Argument 75) — 판단은 HandleOutOfHealth 한 곳
			AttributeSet->OnOutOfHealth.AddUObject(this, &AERPlayerState::HandleOutOfHealth);

			// ⭐ 내가 맞은 피해 → 가해자의 숙련도 (F10-04). 처치 경험치와 같은 경로 — 어트리뷰트셋은 알리기만 한다.
			//   양쪽 다 전투 상태 (F11-04).
			AttributeSet->OnDamageTaken.AddWeakLambda(this, [this](AActor* Attacker, float Damage)
			{
				EnterCombat();
				if (Growth)
				{
					Growth->OnDamageTaken(Damage);   // 방어 숙련도 (F10-05)
				}
				if (AERPlayerState* AttackerPS = Cast<AERPlayerState>(Attacker); AttackerPS && AttackerPS != this)
				{
					AttackerPS->EnterCombat();
					if (AttackerPS->GetGrowth())
					{
						AttackerPS->GetGrowth()->OnDamageDealt(GetPawn(), Damage);
					}
				}
			});
		}
	}
}

UAbilitySystemComponent* AERPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AERPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// ⭐ UPROPERTY(Replicated) 를 추가하면 여기에도 반드시 등록한다.
	//   빠뜨리면 컴파일도 되고 에러도 없는데 값만 안 온다.
	DOREPLIFETIME(AERPlayerState, TeamId);

	// 남의 포인트는 볼 일이 없다. 24명분을 전원에게 보내지 않는다.
	DOREPLIFETIME_CONDITION(AERPlayerState, SkillPoints, COND_OwnerOnly);

	// 스킨은 모두가 본다 (F12.5-01).
	DOREPLIFETIME(AERPlayerState, SkinIndex);
	DOREPLIFETIME(AERPlayerState, GatherSeconds);   // 채집 포즈 재생 속도 (F12.5-04 · Argument 47)
}

// ─────────────────────────────────────────────────────────────
// 스킨
// ─────────────────────────────────────────────────────────────

void AERPlayerState::SetSkinIndex(int32 NewIndex)
{
	if (!HasAuthority() || NewIndex < 0 || NewIndex == SkinIndex)
	{
		return;
	}
	SkinIndex = NewIndex;
	UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 스킨 인덱스 -> %d (서버)"), *GetName(), SkinIndex);
	OnSkinChanged.Broadcast();   // 서버는 OnRep 이 안 불린다
}

void AERPlayerState::OnRep_SkinIndex()
{
	OnSkinChanged.Broadcast();
}

// ─────────────────────────────────────────────────────────────
// 스킬 포인트
// ─────────────────────────────────────────────────────────────

void AERPlayerState::AddSkillPoints(int32 Amount)
{
	if (!HasAuthority() || Amount <= 0)
	{
		return;
	}

	SkillPoints += Amount;
	UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s 포인트 +%d -> %d"), *GetName(), Amount, SkillPoints);
}

bool AERPlayerState::ServerLevelUpSkill_Validate(FGameplayTag SlotTag)
{
	// 조작된 태그(빈 값·슬롯 계열이 아님)만 끊는다. "포인트 없음" 은 정상 실패라 Validate 로 끊지 않는다 —
	// 끊으면 연결이 닫힌다.
	return SlotTag.IsValid() && SlotTag.MatchesTag(ERTags::Ability_Slot);
}

void AERPlayerState::ServerLevelUpSkill_Implementation(FGameplayTag SlotTag)
{
	if (SkillPoints <= 0)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[스킬] %s 포인트가 없다 (%s 요청)."), *GetName(), *SlotTag.ToString());
		return;
	}

	// ⭐ 레벨을 올린 뒤에만 포인트를 뺀다. 실패(상한·미부여)면 포인트가 그대로다.
	if (ERSkill::LevelUpSkill(AbilitySystemComponent, SlotTag, Growth ? Growth->GetLevel() : 1))
	{
		SkillPoints -= 1;
		UE_LOG(LogEternalReturn, Log, TEXT("[스킬] %s 포인트 -1 -> %d"), *GetName(), SkillPoints);
	}
}

// ─────────────────────────────────────────────────────────────
// 무기 스킬 (F11-02)
// ─────────────────────────────────────────────────────────────

void AERPlayerState::RefreshWeaponSkills()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	// ① 회수 — 부여 전에 반드시. 같은 슬롯 스펙이 둘이면 입력이 둘 다 발동시킨다 (F07-01).
	const int32 Taken = WeaponSkills.AbilityHandles.Num();
	ERSkill::TakeSkills(AbilitySystemComponent, WeaponSkills);

	// ② 계열
	const FName WeaponId = Inventory ? Inventory->GetEquippedItem(EEREquipSlot::Weapon) : NAME_None;
	const FERItemRow* Item = WeaponId.IsNone() ? nullptr : ERItem::Find(WeaponId);
	const FERWeaponClassRow* Row = Item ? ERWeapon::Find(Item->WeaponType) : nullptr;   // 행이 없으면 Error 로그 (F11-01)

	if (!Item)
	{
		// 무기 없음 — 아무것도 부여하지 않고 전부 차단. 사거리는 맨몸 값으로.
		if (!UnarmedHandle.IsValid())
		{
			const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(UERUnarmedEffect::StaticClass(), 1.f, AbilitySystemComponent->MakeEffectContext());
			if (Spec.IsValid())
			{
				UnarmedHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data);
			}
		}
		if (bWeaponRangeApplied)   // 벗었을 때만 — 시작 때는 InitDefaultStats 가 넣는다
		{
			const AERCharacterBase* Character = Cast<AERCharacterBase>(GetPawn());
			const UERCharacterData* Data = Character ? Character->GetCharacterData() : nullptr;
			if (Data)
			{
				ApplyWeaponRange(Data->BaseStats.AttackRange);
			}
			else
			{
				UE_LOG(LogEternalReturn, Warning, TEXT("[무기] %s 폰이 없어 맨몸 사거리를 못 되돌렸다 — 부활 때 재적용 (F14)."), *GetName());
			}
			bWeaponRangeApplied = false;
		}
		UE_LOG(LogEternalReturn, Log, TEXT("[무기] %s 무기 없음 — 회수 %d · State.Unarmed (평타 · 스킬 차단)"), *GetName(), Taken);
		return;
	}

	// 무기 있음 — 잠금 해제
	if (UnarmedHandle.IsValid())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(UnarmedHandle);
		UnarmedHandle.Invalidate();
	}

	// ③ 부여 — D 는 InitialLevel 0 (잠김, 03 이 숙련도로 올린다) · 평타는 계열 것
	TArray<TObjectPtr<UERSkillData>> Skills;
	if (Row)
	{
		if (Row->DSkill)     { Skills.Add(Row->DSkill); }
		if (Row->AttackData) { Skills.Add(Row->AttackData); }
		else
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[무기] %s 계열 행에 AttackData 가 없다 — 평타 없이 동작한다."), *UEnum::GetValueAsString(Item->WeaponType));
		}
	}
	ERSkill::GrantSkills(AbilitySystemComponent, Skills, WeaponSkills);
	SyncWeaponSkillLevel();   // 교체 직후 — 그 계열의 현재 숙련도로 D 레벨 (F11-03)

	// ④ 사거리 — 계열 값으로 Base 교체 (장비 Additive 는 유지)
	if (Row)
	{
		ApplyWeaponRange(Row->AttackRange);
		bWeaponRangeApplied = true;
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[무기] %s <- %s (%s): 회수 %d · 부여 %d · 사거리 %s"), *GetName(), *WeaponId.ToString(),
		*UEnum::GetValueAsString(Item->WeaponType), Taken, Skills.Num(), Row ? *FString::Printf(TEXT("%.1fm"), Row->AttackRange) : TEXT("(행 없음 — 유지)"));
}

void AERPlayerState::SetModeAttack(UERSkillData* Data, int32 Level)
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	// ① 지금 Attack 슬롯 스펙 제거 — 계열 평타든 모드 평타든. 같은 슬롯 스펙이 둘이면 입력이 둘 다 발동시킨다 (F07-01).
	for (int32 i = WeaponSkills.AbilityHandles.Num() - 1; i >= 0; --i)
	{
		const FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(WeaponSkills.AbilityHandles[i]);
		if (Spec && Spec->DynamicAbilityTags.HasTagExact(ERTags::Ability_Slot_Attack))
		{
			AbilitySystemComponent->ClearAbility(WeaponSkills.AbilityHandles[i]);
			WeaponSkills.AbilityHandles.RemoveAt(i);
		}
	}
	ModeAttackHandle = FGameplayAbilitySpecHandle();
	if (!Data)
	{
		ModeAimHalfAngleDeg = 0.f;   // 복구 = 조준 제한도 해제
	}

	// ② 부여 — 모드 데이터(레벨 지정) 또는 계열 평타 복구
	UERSkillData* ToGrant = Data;
	if (!ToGrant)
	{
		const FName WeaponId = Inventory ? Inventory->GetEquippedItem(EEREquipSlot::Weapon) : NAME_None;
		const FERItemRow* Item = WeaponId.IsNone() ? nullptr : ERItem::Find(WeaponId);
		const FERWeaponClassRow* Row = Item ? ERWeapon::Find(Item->WeaponType) : nullptr;
		ToGrant = Row ? Row->AttackData.Get() : nullptr;
	}
	if (!ToGrant)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[모드] %s 평타 복구 실패 — 장착 무기의 AttackData 가 없다."), *GetName());
		return;
	}

	// 모드 평타는 모드 스킬(D)의 레벨을 따른다 (포인트 ✘). ⚠ 부여 **뒤** 에 스펙 레벨을 고치면 안 된다 — D 발동 안(스코프 락)에서 불리면
	//   스펙이 아직 목록에 없어 못 찾는다 (E21). GrantSkills 의 LevelOverride 로 만들 때 정한다.
	const int32 ModeLevel = Data ? FMath::Clamp(Level, 1, ToGrant->MaxLevel) : -1;
	FERGrantedSkillHandles Granted;
	ERSkill::GrantSkills(AbilitySystemComponent, { ToGrant }, Granted, ModeLevel);
	for (const FGameplayAbilitySpecHandle& H : Granted.AbilityHandles)
	{
		WeaponSkills.AbilityHandles.Add(H);
		if (Data)
		{
			ModeAttackHandle = H;
		}
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[모드] %s 평타 %s -> %s%s"), *GetName(), Data ? TEXT("교체") : TEXT("복구"), *GetNameSafe(ToGrant),
		Data ? *FString::Printf(TEXT(" Lv.%d"), ModeLevel) : TEXT(""));
}

bool AERPlayerState::SyncWeaponSkillLevel()
{
	if (!HasAuthority() || !AbilitySystemComponent || !Inventory || !Growth)
	{
		return false;
	}
	const FName WeaponId = Inventory->GetEquippedItem(EEREquipSlot::Weapon);
	const FERItemRow* Item = WeaponId.IsNone() ? nullptr : ERItem::Find(WeaponId);
	const FERWeaponClassRow* Row = Item ? ERWeapon::Find(Item->WeaponType) : nullptr;
	if (!Row || !Row->DSkill)
	{
		return false;
	}

	const int32 Proficiency = Growth->GetWeaponProficiencyLevel(Item->WeaponType);
	int32 DLevel = 0;
	if (Proficiency >= Row->UnlockLevel)
	{
		DLevel = 1;
		for (const int32 Upgrade : Row->UpgradeLevels)
		{
			if (Proficiency >= Upgrade) { ++DLevel; }
		}
	}
	const bool bChanged = ERSkill::SetSkillLevel(AbilitySystemComponent, ERTags::Ability_Slot_D, DLevel,
		*FString::Printf(TEXT("%s 숙련도 %d · 해금 %d · 강화 %d개"), *UEnum::GetValueAsString(Item->WeaponType), Proficiency, Row->UnlockLevel, Row->UpgradeLevels.Num()));
	return bChanged && DLevel == 1;
}

void AERPlayerState::ClientPlayVoice_Implementation(FGameplayTag VoiceKey)
{
	const APawn* MyPawn = GetPawn();
	if (UERPresentationComponent* Pres = MyPawn ? MyPawn->FindComponentByClass<UERPresentationComponent>() : nullptr)
	{
		Pres->PlayVoice(VoiceKey, /*bSelf=*/true);
	}
}

void AERPlayerState::EnterCombat()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}
	// 갱신 = 이전 것 제거 후 새로 (남은 시간 연장). 쿨다운 GE 와 달리 겹쳐 쌓이면 안 된다.
	if (CombatHandle.IsValid())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(CombatHandle);
		CombatHandle.Invalidate();
	}
	const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(UERSkillPhaseEffect::StaticClass(), 1.f, AbilitySystemComponent->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_PhaseDuration, UERCombatSettings::Get().CombatStateSeconds);
	Spec.Data->DynamicGrantedTags.AddTag(ERTags::State_InCombat);
	CombatHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data);
}

void AERPlayerState::ApplyWeaponRange(float RangeMeters)
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}
	const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(UERWeaponRangeEffect::StaticClass(), 1.f, AbilitySystemComponent->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_AttackRange, RangeMeters);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	UE_LOG(LogEternalReturn, Log, TEXT("[무기] %s 사거리 기본값 -> %.1fm (현재 %.1f)"), *GetName(), RangeMeters,
		AbilitySystemComponent->GetNumericAttribute(UERAttributeSet::GetAttackRangeAttribute()));
}

void AERPlayerState::OnRep_SkillPoints()
{
	// UI 가 생기면 여기서 갱신한다. 지금은 로그만.
	UE_LOG(LogEternalReturn, Verbose, TEXT("[스킬] %s 포인트 복제 -> %d"), *GetName(), SkillPoints);
}

// ─────────────────────────────────────────────────────────────
// 빈사 · 사망 · 소생 (F14 · Argument 75 — A1 GE+태그 · H1 같은 HP · K2 빈사로 만든 사람이 처치)
// ─────────────────────────────────────────────────────────────

bool AERPlayerState::IsDowned() const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(ERTags::State_Downed);
}

bool AERPlayerState::IsDead() const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(ERTags::State_Dead);
}

bool AERPlayerState::IsStanding() const
{
	return GetPawn() && !IsDowned() && !IsDead();
}

void AERPlayerState::ForEachTeammate(TFunctionRef<void(AERPlayerState&)> Fn) const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const int32 MyTeam = ERTeamStatics::GetTeamId(this);
	if (!GS || MyTeam == INDEX_NONE)
	{
		return;   // 무소속 = 팀원 없음
	}
	for (APlayerState* Other : GS->PlayerArray)
	{
		AERPlayerState* PS = Cast<AERPlayerState>(Other);
		if (PS && PS != this && ERTeamStatics::GetTeamId(PS) == MyTeam)
		{
			Fn(*PS);
		}
	}
}

void AERPlayerState::CancelActiveAbilities()
{
	// 시전 중 쓰러지면 판정 타이머가 계속 돈다 — 전부 취소 (패시브는 듣기만 하니 둔다 · Lyra 사망 어빌리티와 같은 이유 · Docs/6_Lyra참조/07)
	TArray<FGameplayAbilitySpecHandle> ToCancel;
	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (Spec.IsActive() && Spec.Ability && !Spec.Ability->IsA<UERPassiveAbility>())
		{
			ToCancel.Add(Spec.Handle);
		}
	}
	for (const FGameplayAbilitySpecHandle& H : ToCancel)
	{
		AbilitySystemComponent->CancelAbilityHandle(H);
	}
}

void AERPlayerState::HandleOutOfHealth(AActor* LastHitter)
{
	if (!HasAuthority() || !AbilitySystemComponent || IsDead())
	{
		return;
	}
	// 이번 일로 우리 팀이 전멸하면 알림을 받을 쪽 — 빈사로 만든 사람 (체력 감소로 죽어도) · 없으면 마무리한 사람. Die 가 DownedBy 를 비우기 전에 잡는다
	AERPlayerState* Terminator = DownedBy.IsValid() ? DownedBy.Get() : Cast<AERPlayerState>(LastHitter);
	if (IsDowned())
	{
		Die(LastHitter);   // 빈사 중 다시 0 — 맞아서든 체력 감소로든
	}
	else
	{
		bool bTeammateStanding = false;
		ForEachTeammate([&bTeammateStanding](AERPlayerState& PS) { bTeammateStanding |= PS.IsStanding(); });
		if (bTeammateStanding)
		{
			EnterDowned(Cast<AERPlayerState>(LastHitter));
		}
		else
		{
			Die(LastHitter);   // 서 있는 팀원이 없다 — 바로 사망 (원작: 팀원이 살아 있을 때만 빈사)
		}
	}
	CheckTeamWipe(Terminator);
}

void AERPlayerState::SendTeamSfx(FGameplayTag SfxKey, bool bIncludeSelf)
{
	if (bIncludeSelf)
	{
		ClientPlaySfx(SfxKey);
	}
	ForEachTeammate([SfxKey](AERPlayerState& PS) { PS.ClientPlaySfx(SfxKey); });
}

void AERPlayerState::ClientPlaySoundPath_Implementation(const FSoftObjectPath& SoundPath)
{
	// 작은 소리 하나 — 처음이면 동기 로드 (상자 열기 · 한 번)
	USoundBase* Sound = Cast<USoundBase>(SoundPath.TryLoad());
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 소리 %s (본인만)"), *GetNameSafe(this), Sound ? *Sound->GetName() : *FString::Printf(TEXT("없음 — %s"), *SoundPath.ToString()));
}

void AERPlayerState::ClientPlaySfx_Implementation(FGameplayTag SfxKey)
{
	const APawn* MyPawn = GetPawn();
	const UERPresentationComponent* Pres = MyPawn ? MyPawn->FindComponentByClass<UERPresentationComponent>() : nullptr;
	USoundBase* Sound = Pres ? Pres->PickSound(SfxKey) : nullptr;
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
	else if (UERSoundSubsystem* Common = UERSoundSubsystem::Get(this))
	{
		Sound = Common->Play2D(SfxKey);   // 캐릭터 DA 에 없으면 공용 (Argument 78)
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 본인 소리 %s — %s"), *GetNameSafe(this), *SfxKey.ToString(), Sound ? *Sound->GetName() : TEXT("줄 없음"));
}

void AERPlayerState::EnterDowned(AERPlayerState* By)
{
	const UERCombatSettings& S = UERCombatSettings::Get();
	DownedBy = By != this ? By : nullptr;
	CancelActiveAbilities();

	FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(UERDownedEffect::StaticClass(), 1.f, AbilitySystemComponent->MakeEffectContext());
	Spec.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_Downed_MoveSpeed, GetDownedMoveSpeed());
	Spec.Data->DynamicGrantedTags.AddTag(ERTags::State_Downed);
	DownedHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data);

	FGameplayEffectSpecHandle Bleed = AbilitySystemComponent->MakeOutgoingSpec(UERDownedBleedEffect::StaticClass(), 1.f, AbilitySystemComponent->MakeEffectContext());
	Bleed.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_Downed_Bleed, -S.DownedBleedPerSecond);
	BleedHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Bleed.Data);

	// 빈사 체력 (H1) — 같은 HP 를 400 으로. 이 함수는 사망 알림 안에서 불린다 → 알림 뒤 빗장이 HP 를 다시 읽어 풀린다 (UERAttributeSet)
	const float MaxHP = AbilitySystemComponent->GetNumericAttribute(UERAttributeSet::GetMaxHPAttribute());
	AbilitySystemComponent->SetNumericAttributeBase(UERAttributeSet::GetHPAttribute(), FMath::Min(S.DownedHP, MaxHP));
	const int32 Level = Growth ? Growth->GetLevel() : 1;
	const float Reduction = S.DownedDamageReductionBase + S.DownedDamageReductionPerLevel * Level;
	AttributeSet->SetDownedDamageReduction(Reduction);

	if (APawn* MyPawn = GetPawn())
	{
		UERPresentationComponent::SendSfxCue(MyPawn, ERTags::Pres_Sfx_Downed, MyPawn->GetActorLocation());
		SendTeamSfx(ERTags::Pres_Sfx_TeamDown, /*bIncludeSelf=*/false);   // 팀원 빈사 알림 (UI 소리 · 우리 팀만 · 사용자 2026-10-08)
		// ⭐ 스킬의 "처치 시" = **빈사로 만든 순간만** (재키 E 쿨 초기화 · R +5초 · 학살 재사용 · P 아드레날린 — 사용자 2026-10-08 "포함이 아니라 빈사만")
		//   처치 경험치 · 누적 처치 음성 · 시체 처치자는 사망 때 (K2 — Die)
		SendKillEvent(DownedBy.Get(), MyPawn);
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[빈사] %s 빈사 — 빈사로 만든 %s · 체력 %.0f · 이속 %.1f · 받는 피해 −%.0f%% (Lv.%d) · 초당 −%.1f"),
		*GetNameSafe(this), *GetNameSafe(DownedBy.Get()), FMath::Min(S.DownedHP, MaxHP), S.DownedMoveSpeed, Reduction * 100.f, Level, S.DownedBleedPerSecond);
}

float AERPlayerState::GetDownedMoveSpeed() const
{
	const UERCombatSettings& S = UERCombatSettings::Get();
	const bool bObstinacy = AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(ERTags::State_DownedSkill_Obstinacy);
	return S.DownedMoveSpeed * (bObstinacy ? 1.f + S.DownedObstinacyMoveSpeedBonus : 1.f);
}

void AERPlayerState::OnObstinacyChanged(const FGameplayTag Tag, int32 Count)
{
	if (!HasAuthority() || !DownedHandle.IsValid())
	{
		return;   // 빈사가 아니면 할 일 없다 (소생 뒤 남은 3초 등)
	}
	// ⭐ 빈사 이속은 Override 하나 — 버프 GE 를 얹으면 첫 Override 가 이겨 조용히 안 먹는다 (GameplayEffectAggregator.cpp:60).
	//   그 Override 값 자체를 바꾼다 — SetByCaller 갱신은 엔진이 모디파이어를 다시 모은다
	const float Speed = GetDownedMoveSpeed();
	AbilitySystemComponent->UpdateActiveGameplayEffectSetByCallerMagnitude(DownedHandle, ERTags::SetByCaller_Downed_MoveSpeed, Speed);
	UE_LOG(LogEternalReturn, Log, TEXT("[빈사 스킬] %s 집념 %s — 빈사 이속 %.1f"), *GetNameSafe(this), Count > 0 ? TEXT("시작") : TEXT("끝"), Speed);
}

void AERPlayerState::SelfDestruct()
{
	if (!HasAuthority() || !IsDowned() || IsDead())
	{
		return;
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[빈사] %s 분열 폭발로 사망"), *GetNameSafe(this));
	Die(nullptr);   // 처치 = DownedBy (K2) — 빈사에서 죽는 것이라 서 있는 사람 수는 그대로 (전멸 검사 불필요)
}

void AERPlayerState::ClearDowned()
{
	if (DownedHandle.IsValid())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(DownedHandle);
		DownedHandle.Invalidate();
	}
	if (BleedHandle.IsValid())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(BleedHandle);
		BleedHandle.Invalidate();
	}
	AttributeSet->SetDownedDamageReduction(0.f);
}

void AERPlayerState::Die(AActor* LastHitter)
{
	const bool bFromDowned = IsDowned();
	// K2 — 빈사로 만든 사람이 처치자. 없으면 (환경 · 야생동물이 쓰러뜨림 · 팀원 없이 바로 사망) 마무리한 사람
	AERPlayerState* KillerPS = DownedBy.IsValid() ? DownedBy.Get() : Cast<AERPlayerState>(LastHitter);
	if (KillerPS == this)
	{
		KillerPS = nullptr;
	}
	CancelActiveAbilities();

	// 사망 GE 를 **먼저** 건다 — 빈사에서 죽었다는 표시(State.Dead.FromDowned)가 같은 GE 에 있어 사망 애니(down_dead)를 고른다 (Argument 76)
	FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(UERDeadEffect::StaticClass(), 1.f, AbilitySystemComponent->MakeEffectContext());
	Spec.Data->DynamicGrantedTags.AddTag(ERTags::State_Dead);
	Spec.Data->DynamicGrantedTags.AddTag(ERTags::State_Block_Movement);
	Spec.Data->DynamicGrantedTags.AddTag(ERTags::State_Untargetable);
	if (bFromDowned)
	{
		Spec.Data->DynamicGrantedTags.AddTag(ERTags::State_Dead_FromDowned);
	}
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	ClearDowned();
	DownedBy.Reset();
	AbilitySystemComponent->SetNumericAttributeBase(UERAttributeSet::GetHPAttribute(), 0.f);   // 팀 전멸로 죽는 빈사자는 체력이 남아 있다

	APawn* MyPawn = GetPawn();
	if (MyPawn)
	{
		if (AController* C = MyPawn->GetController())
		{
			C->StopMovement();   // 가던 길 (클릭 이동) — 이동 차단 태그는 다음 입력부터
		}
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[빈사] %s 사망%s — 처치 %s (마무리 %s)"), *GetNameSafe(this),
		bFromDowned ? TEXT(" (빈사에서)") : TEXT(""), *GetNameSafe(KillerPS), *GetNameSafe(LastHitter));

	// 사망 음성 (Argument 73 · 주변) — **진짜 사망** 순간 (빈사가 아니다 · 사용자 2026-10-08)
	UERPresentationComponent::SendVoiceCue(MyPawn, ERTags::Pres_Voice_Death);
	// 시체 (F08-05) — 원본 인벤토리는 그대로 (원작 확인: 복사 드롭)
	if (Inventory && MyPawn)
	{
		if (AERItemDropActor* Corpse = Inventory->SpawnDeathDrop(MyPawn->GetActorLocation()))
		{
			Corpse->SetKiller(KillerPS);   // 시체 수색 음성 — 내가 처치한 시체인가 (Argument 73)
		}
	}
	if (!KillerPS)
	{
		return;
	}
	// ⭐ 처치 → 처치자의 무기 숙련도 (적 레벨 함수) — 실험체 경험치는 숙련도가 올린다 (F10-05 · Argument 26 B). 어시스트 분배 없음 `[자체]`.
	if (KillerPS->GetGrowth())
	{
		KillerPS->GetGrowth()->OnPlayerKilled(Growth ? Growth->GetLevel() : 1);
	}
	// 스킬의 "처치 시" 이벤트는 여기서 안 보낸다 — **빈사로 만든 순간만** (EnterDowned · 사용자 2026-10-08 "포함이 아니라 빈사만")
	// 누적 처치 음성 (Argument 73 · 본인) — kill1Player ~ kill14Player. 15명부터는 대사 없음
	const FGameplayTag KillVoice[] = {
		ERTags::Pres_Voice_Kill_1, ERTags::Pres_Voice_Kill_2, ERTags::Pres_Voice_Kill_3, ERTags::Pres_Voice_Kill_4, ERTags::Pres_Voice_Kill_5,
		ERTags::Pres_Voice_Kill_6, ERTags::Pres_Voice_Kill_7, ERTags::Pres_Voice_Kill_8, ERTags::Pres_Voice_Kill_9, ERTags::Pres_Voice_Kill_10,
		ERTags::Pres_Voice_Kill_11, ERTags::Pres_Voice_Kill_12, ERTags::Pres_Voice_Kill_13, ERTags::Pres_Voice_Kill_14,
	};
	++KillerPS->PlayerKills;
	if (KillerPS->PlayerKills <= static_cast<int32>(UE_ARRAY_COUNT(KillVoice)))
	{
		KillerPS->ClientPlayVoice(KillVoice[KillerPS->PlayerKills - 1]);
	}
}

void AERPlayerState::SendKillEvent(AERPlayerState* KillerPS, APawn* VictimPawn)
{
	UAbilitySystemComponent* KillerASC = KillerPS && KillerPS != this ? KillerPS->GetAbilitySystemComponent() : nullptr;
	if (!KillerASC)
	{
		return;
	}
	// 처치 이벤트 → 처치자 ASC (재키 P · E · R "처치 관여" · Argument 65) — 패시브 · 조각이 듣는다
	FGameplayEventData Kill;
	Kill.EventTag = ERTags::Event_Kill_Dealt;
	Kill.Instigator = KillerPS->GetPawn();
	Kill.Target = VictimPawn;
	KillerASC->HandleGameplayEvent(ERTags::Event_Kill_Dealt, &Kill);
}

void AERPlayerState::CheckTeamWipe(AERPlayerState* Terminator)
{
	if (IsStanding())
	{
		return;
	}
	bool bAnyStanding = false;
	ForEachTeammate([&bAnyStanding](AERPlayerState& PS) { bAnyStanding |= PS.IsStanding(); });
	if (bAnyStanding)
	{
		return;
	}
	// 전멸시킨 팀에 알림 (터미네이트 · 사용자 2026-10-08 "우리 팀만") — 같은 팀 · 야생동물 · 환경이면 없음
	if (Terminator && ERTeamStatics::IsHostile(Terminator, this))
	{
		Terminator->SendTeamSfx(ERTags::Pres_Sfx_Terminate, /*bIncludeSelf=*/true);
		UE_LOG(LogEternalReturn, Log, TEXT("[빈사] 팀 %d 전멸 — 터미네이트 %s (팀 %d)"), ERTeamStatics::GetTeamId(this), *GetNameSafe(Terminator), ERTeamStatics::GetTeamId(Terminator));
	}
	// 서 있는 사람이 없다 — 빈사자 전원 사망 (원작: 팀원이 모두 쓰러지면 전멸)
	TArray<AERPlayerState*> Downed;
	if (IsDowned()) { Downed.Add(this); }
	ForEachTeammate([&Downed](AERPlayerState& PS) { if (PS.IsDowned()) { Downed.Add(&PS); } });
	if (!Downed.IsEmpty())
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[빈사] 팀 %d 전멸 — 빈사 %d명 사망"), ERTeamStatics::GetTeamId(this), Downed.Num());
	}
	for (AERPlayerState* PS : Downed)
	{
		PS->Die(nullptr);
	}
}

void AERPlayerState::Revive(AERPlayerState* By)
{
	if (!HasAuthority() || !IsDowned() || IsDead())
	{
		return;
	}
	ClearDowned();
	DownedBy.Reset();
	const float Ratio = UERCombatSettings::Get().ReviveHPRatio;
	const float MaxHP = AbilitySystemComponent->GetNumericAttribute(UERAttributeSet::GetMaxHPAttribute());
	AbilitySystemComponent->SetNumericAttributeBase(UERAttributeSet::GetHPAttribute(), MaxHP * Ratio);
	if (APawn* MyPawn = GetPawn())
	{
		UERPresentationComponent::SendSfxCue(MyPawn, ERTags::Pres_Sfx_Resurrect, MyPawn->GetActorLocation());
	}
	ClientPlayVoice(ERTags::Pres_Voice_Resurrection);   // 아군이 나를 일으켜 세웠다 (Argument 73 · 본인)
	SendTeamSfx(ERTags::Pres_Sfx_TeamRevival, /*bIncludeSelf=*/false);   // 팀원 소생 알림 (UI 소리 · 우리 팀만 · 사용자 2026-10-08)
	UE_LOG(LogEternalReturn, Log, TEXT("[빈사] %s 소생 ← %s · 체력 %.0f (최대의 %.0f%%)"), *GetNameSafe(this), *GetNameSafe(By), MaxHP * Ratio, Ratio * 100.f);
}
