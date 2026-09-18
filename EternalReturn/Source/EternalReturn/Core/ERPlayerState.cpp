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

		// ⭐ 무기 교체 → 숙련도 증폭 GE 갱신 (F10-04). 장비 GE 와 별개 핸들.
		if (Inventory && Growth)
		{
			Inventory->OnEquippedChanged.AddWeakLambda(this, [this](EEREquipSlot Slot)
			{
				if (Slot == EEREquipSlot::Weapon)
				{
					Growth->RefreshProficiencyBonus();
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
			AttributeSet->OnDamageTaken.AddWeakLambda(this, [this](AActor* Attacker, float Damage)
			{
				if (const AERPlayerState* AttackerPS = Cast<AERPlayerState>(Attacker); AttackerPS && AttackerPS != this && AttackerPS->GetGrowth())
				{
					AttackerPS->GetGrowth()->OnDamageDealt(GetPawn(), Damage);
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

void AERPlayerState::OnRep_SkillPoints()
{
	// UI 가 생기면 여기서 갱신한다. 지금은 로그만.
	UE_LOG(LogEternalReturn, Verbose, TEXT("[스킬] %s 포인트 복제 -> %d"), *GetName(), SkillPoints);
}
