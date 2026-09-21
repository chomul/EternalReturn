// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ERPlayerState.h"
#include "AbilitySystemComponent.h"
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
			});
			// ⭐ 숙련도 레벨업 → D 해금 · 강화 (F11-03). 장착 계열의 이벤트만 — 다른 계열은 교체할 때 맞춘다.
			Growth->OnWeaponProficiencyLevelUp.AddWeakLambda(this, [this](EERWeaponType WeaponType, int32 /*NewLevel*/)
			{
				const FName WeaponId = Inventory->GetEquippedItem(EEREquipSlot::Weapon);
				const FERItemRow* Item = WeaponId.IsNone() ? nullptr : ERItem::Find(WeaponId);
				if (Item && Item->WeaponType == WeaponType)
				{
					SyncWeaponSkillLevel();
				}
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
			AttributeSet->OnOutOfHealth.AddWeakLambda(this, [this](AActor* Killer)
			{
				const APawn* MyPawn = GetPawn();
				if (Inventory && MyPawn)
				{
					Inventory->SpawnDeathDrop(MyPawn->GetActorLocation());
				}

				// ⭐ 처치 경험치 (F10-01) + 처치 숙련도 (F10-04). Killer 는 GE 컨텍스트의 OriginalInstigator = 가해자의 ASC 소유자 = PlayerState.
				//   자기 자신(자해) · 환경 피해(null) · 야생동물(PlayerState 아님) 은 제외. 어시스트 분배 없음 `[자체]`.
				if (const AERPlayerState* KillerPS = Cast<AERPlayerState>(Killer); KillerPS && KillerPS != this && KillerPS->GetGrowth())
				{
					KillerPS->GetGrowth()->AddExp(UERGrowthSettings::Get().PlayerKillExp, EERExpSource::PlayerKill);
					KillerPS->GetGrowth()->AddEquippedWeaponProficiencyExp(UERGrowthSettings::Get().ProficiencyExpPerPlayerKill, TEXT("실험체 처치"));
				}
			});

			// ⭐ 내가 맞은 피해 → 가해자의 숙련도 (F10-04). 처치 경험치와 같은 경로 — 어트리뷰트셋은 알리기만 한다.
			//   양쪽 다 전투 상태 (F11-04).
			AttributeSet->OnDamageTaken.AddWeakLambda(this, [this](AActor* Attacker, float Damage)
			{
				EnterCombat();
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

void AERPlayerState::SyncWeaponSkillLevel()
{
	if (!HasAuthority() || !AbilitySystemComponent || !Inventory || !Growth)
	{
		return;
	}
	const FName WeaponId = Inventory->GetEquippedItem(EEREquipSlot::Weapon);
	const FERItemRow* Item = WeaponId.IsNone() ? nullptr : ERItem::Find(WeaponId);
	const FERWeaponClassRow* Row = Item ? ERWeapon::Find(Item->WeaponType) : nullptr;
	if (!Row || !Row->DSkill)
	{
		return;
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
	ERSkill::SetSkillLevel(AbilitySystemComponent, ERTags::Ability_Slot_D, DLevel,
		*FString::Printf(TEXT("%s 숙련도 %d · 해금 %d · 강화 %d개"), *UEnum::GetValueAsString(Item->WeaponType), Proficiency, Row->UnlockLevel, Row->UpgradeLevels.Num()));
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
