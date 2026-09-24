// Copyright Epic Games, Inc. All Rights Reserved.

#include "Wildlife/ERWildlifeCharacter.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "ERCollisionChannels.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "EternalReturn.h"
#include "GAS/ERAttributeInit.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Core/ERPlayerState.h"
#include "Growth/ERGrowthComponent.h"
#include "Growth/ERGrowthSettings.h"
#include "Item/ERItemDropActor.h"
#include "Item/ERItemSettings.h"
#include "Wildlife/ERWildlifeAIController.h"
#include "Wildlife/ERWildlifeData.h"
#include "Wildlife/ERWildlifeSettings.h"
#include "Wildlife/ERWildlifeSpawnPoint.h"

AERWildlifeCharacter::AERWildlifeCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// ⭐ ASC 는 폰에 · Minimal — PlayerState 가 없다. Mixed 는 소유자 있는 ASC 용 (AbilitySystemComponent.h:87).
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	AttributeSet = CreateDefaultSubobject<UERAttributeSet>(TEXT("AttributeSet"));

	// ⭐ 복제 비용 (역기획서 §7.3) — 120~200마리가 대부분 가만히 있다. 재워 두고 맞을 때 깨운다.
	bReplicates = true;
	NetUpdateFrequency = 5.f;
	MinNetUpdateFrequency = 1.f;
	NetDormancy = DORM_DormantAll;

	// ⭐ 히트 박스 — 가로로 긴 몸. 판정(SkillTarget) · 커서 클릭(Pawn 트레이스) · 다른 폰 충돌은 박스가, 이동 · 지형은 캡슐이 (사용자 2026-09-22).
	// 기본 서브오브젝트라 BP 뷰포트에 보인다 — 종 메시를 넣고 눈으로 맞춘 값을 DA.HitBoxExtent 에 옮긴다. 크기 · 위치는 ApplyVisuals 가 DA 로 덮어쓴다.
	HitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
	HitBox->SetupAttachment(GetCapsuleComponent());
	HitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitBox->SetCollisionObjectType(ECC_Pawn);
	// ⚠ Pawn 응답은 Ignore 로 둔다 — 몸 부딪힘 · 이동 차단은 캡슐만 (원작 관측). 클릭은 Select, 판정은 SkillTarget 이 맡는다.
	HitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitBox->SetCollisionResponseToChannel(ERCollisionChannel::Select, ECR_Block);       // 커서 클릭 — 몸 끝을 찍어도 잡힌다
	HitBox->SetCollisionResponseToChannel(ERCollisionChannel::SkillTarget, ECR_Overlap); // F04 판정. ⚠ Block 이면 관통 투사체 스윕이 여기서 멈춘다
	HitBox->SetCanEverAffectNavigation(false);
	// 캡슐은 판정에서 뺀다 — 같은 액터가 두 컴포넌트로 잡히면 AddUnique 로 합쳐지긴 하지만 크기 기준을 박스 하나로.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ERCollisionChannel::SkillTarget, ECR_Ignore);

	// 야생동물은 컨트롤 회전 없음 · 이동 방향을 본다 (AI 가 움직인다, 04).
	bUseControllerRotationYaw = false;
	// ⭐ AI 는 서버에만 있다 (§7.2). 스폰되면 빙의 — BP 의 Auto Possess AI 가 Disabled 면 BP 쪽을 고친다.
	AIControllerClass = AERWildlifeAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
	}
}

void AERWildlifeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AERWildlifeCharacter, Data, COND_InitialOnly);
	DOREPLIFETIME(AERWildlifeCharacter, Level);   // 시간 성장(03)이 바꾼다
	DOREPLIFETIME(AERWildlifeCharacter, Corpse);  // 죽은 뒤 한 번 — 클라가 클릭해서 열 대상
	DOREPLIFETIME(AERWildlifeCharacter, bDead);   // 사망 포즈 (AnimBP) — Argument 36 "상태는 복제 값"
}

void AERWildlifeCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Owner = Avatar = 이 폰. 서버 · 클라 양쪽 — 클라도 어트리뷰트 복제를 받으려면 ActorInfo 가 있어야 한다.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (HasAuthority() && AttributeSet)
	{
		AttributeSet->OnOutOfHealth.AddWeakLambda(this, [this](AActor* Killer) { HandleOutOfHealth(Killer); });
		// 맞으면 깨운다 — 체력 변화가 클라로 가야 한다. 전투 진입 · 재우기는 AI 가 (F12-04).
		AttributeSet->OnDamageTaken.AddWeakLambda(this, [this](AActor* Attacker, float)
		{
			FlushNetDormancy();
			if (AERWildlifeAIController* AI = GetController<AERWildlifeAIController>())
			{
				AI->NotifyDamaged(Attacker);
			}
		});
	}
}

void AERWildlifeCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	// ⚠ AI 컨트롤러가 Owner 가 되면 Minimal 이 깨진다 (소유자 있는 ASC). Possess 가 Owner 를 컨트롤러로 바꾸므로 되돌린다.
	SetOwner(nullptr);
}

bool AERWildlifeCharacter::IsMutant() const { return Data && Data->Variant != EERWildlifeVariant::Normal; }
bool AERWildlifeCharacter::IsBoss() const   { return Data && Data->bBoss; }

bool AERWildlifeCharacter::Initialize(const UERWildlifeData* InData, int32 InLevel)
{
	if (!HasAuthority())
	{
		return false;
	}
	if (!InData)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[야생동물] %s 종 정의(UERWildlifeData)가 null 이다."), *GetName());
		return false;
	}
	Data = InData;
	Level = FMath::Max(1, InLevel);

	// 스탯 = Base + PerLevel × (Lv − 1). F10-02 와 같은 4항목만 레벨을 탄다 (역기획서 §2 "공격력 / 레벨당").
	FERCharStats Stats = Data->BaseStats;
	const float Steps = static_cast<float>(Level - 1);
	Stats.MaxHP       += Data->PerLevel.MaxHPPerLevel * Steps;
	Stats.AttackPower += Data->PerLevel.AttackPowerPerLevel * Steps;
	Stats.Defense     += Data->PerLevel.DefensePerLevel * Steps;
	Stats.HPRegen     += Data->PerLevel.HPRegenPerLevel * Steps;

	if (!ERAttributeInit::ApplyStatRow(AbilitySystemComponent, InitStatsEffect, Stats))
	{
		return false;   // ApplyStatRow 가 Error 로그를 남겼다 (GE 없음 · 모양 다름)
	}

	// 태그 — F03 이 흡혈 감소 · 비례 피해 감쇠에 읽는다. 복제 태그 — 클라 표시 · 필터에도 보이게.
	const FGameplayTag TypeTag = Data->bBoss ? ERTags::Actor_Type_Boss : ERTags::Actor_Type_Wildlife;
	if (!AbilitySystemComponent->HasMatchingGameplayTag(TypeTag))
	{
		AbilitySystemComponent->AddLooseGameplayTag(TypeTag);
		AbilitySystemComponent->AddReplicatedLooseGameplayTag(TypeTag);
	}
	// 체력 비례 피해 저항 — 보스마다 다르다 (알파 0.3 · 오메가 0.4 · 위클라인 0.5 · Argument 38 B). F03 이 대상 어트리뷰트로 읽는다. 서버 전용 값.
	AbilitySystemComponent->SetNumericAttributeBase(UERAttributeSet::GetProportionalDamageResistAttribute(), Data->ProportionalDamageResist);

	// 스킬 — 실험체와 같은 경로 (F07-01 GrantSkills · F11.5 조각). 1회만 (재레벨 때 중복 부여 금지).
	if (!bSkillsGranted)
	{
		// ⭐ 평타 슬롯이 없으면 공통 평타(설정)를 붙인다 — 종마다 넣지 않는다. 곰처럼 자기 평타가 있으면 그게 우선.
		TArray<TObjectPtr<UERSkillData>> Skills = Data->Skills;
		const bool bHasAttack = Skills.ContainsByPredicate([](const UERSkillData* S) { return S && S->SlotTag.MatchesTagExact(ERTags::Ability_Slot_Attack); });
		const UERWildlifeSettings& WS = UERWildlifeSettings::Get();
		if (!bHasAttack && !WS.DefaultAttackSkill.IsNull())
		{
			if (UERSkillData* Default = WS.DefaultAttackSkill.LoadSynchronous())
			{
				Skills.Add(Default);
			}
		}
		if (!Skills.IsEmpty())
		{
			ERSkill::GrantSkills(AbilitySystemComponent, Skills, GrantedSkills);
		}
		bSkillsGranted = true;
	}

	// 이동 속도는 어트리뷰트 → CMC (실험체와 같은 단위 m/s → cm/s).
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = AbilitySystemComponent->GetNumericAttribute(UERAttributeSet::GetMoveSpeedAttribute()) * 100.f;
	}
	ApplyVisuals();

	if (Data->bBoss)
	{
		// ⭐ 보스는 재우지 않는다 (Task 05 · §7.3) — 한 판에 몇 마리뿐이고 등장 · 전투가 모두의 관심사다.
		SetNetDormancy(DORM_Never);
		NetUpdateFrequency = UERWildlifeSettings::Get().BossNetUpdateFrequency;
	}
	FlushNetDormancy();   // 초기값이 나가게

	// 스폰 선공 (위클라인 20m — 실험 대상 추적). 첫 초기화 때만.
	if (!bInitializedOnce && Data->SpawnAggroRadius > 0.f)
	{
		if (AERWildlifeAIController* AI = GetController<AERWildlifeAIController>())
		{
			AI->AggroNearestInRadius(Data->SpawnAggroRadius);
		}
	}
	bInitializedOnce = true;
	UE_LOG(LogEternalReturn, Log, TEXT("[야생동물] %s <- %s Lv.%d%s%s — 체력 %.0f · 공격력 %.1f · 방어력 %.1f · 이동 %.2f · 스킬 %d"),
		*GetName(), *Data->GetName(), Level, Data->Variant == EERWildlifeVariant::Mutant ? TEXT(" (변이)") : Data->Variant == EERWildlifeVariant::Corrupted ? TEXT(" (잠식)") : TEXT(""), Data->bBoss ? TEXT(" (보스)") : TEXT(""),
		AbilitySystemComponent->GetNumericAttribute(UERAttributeSet::GetMaxHPAttribute()),
		AbilitySystemComponent->GetNumericAttribute(UERAttributeSet::GetAttackPowerAttribute()),
		AbilitySystemComponent->GetNumericAttribute(UERAttributeSet::GetDefenseAttribute()),
		AbilitySystemComponent->GetNumericAttribute(UERAttributeSet::GetMoveSpeedAttribute()),
		GrantedSkills.AbilityHandles.Num());
	return true;
}

void AERWildlifeCharacter::ApplyVisuals()
{
	if (!Data || !GetMesh())
	{
		return;
	}
	// 캡슐은 BP 기본 하나 (이동 · 지형만이라 종별 조정 없음). 메시 피벗은 발끝 → 캡슐 바닥에 놓는다 (실험체 BP 와 같은 규약: 메시 Z = −반높이).
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight));
	GetMesh()->SetRelativeScale3D(FVector(Data->MeshScale));

	// 히트 박스 — DA 값이 있으면 덮어쓴다 (없으면 BP 값). 위치는 DA 중심, 없으면 바닥을 발끝에 맞춘다.
	if (HitBox)
	{
		if (!Data->HitBoxExtent.IsNearlyZero())
		{
			HitBox->SetBoxExtent(Data->HitBoxExtent);
		}
		HitBox->SetRelativeLocation(Data->HitBoxOffset.IsNearlyZero()
			? FVector(0.f, 0.f, HitBox->GetUnscaledBoxExtent().Z - HalfHeight)
			: Data->HitBoxOffset);
	}
	if (!Data->Mesh.IsNull())
	{
		if (USkeletalMesh* LoadedMesh = Data->Mesh.LoadSynchronous())
		{
			GetMesh()->SetSkeletalMesh(LoadedMesh);
		}
	}
	if (!Data->AnimClass.IsNull())
	{
		if (UClass* Anim = Data->AnimClass.LoadSynchronous())
		{
			GetMesh()->SetAnimInstanceClass(Anim);
		}
	}
}

void AERWildlifeCharacter::SpawnCorpse()
{
	if (!Data || Data->LootRow.IsNone())
	{
		return;   // 루트 행이 없는 종 — 드랍 없음
	}
	// ⭐ 시체는 **상자와 같은 액터**다 (F08-05 · F09-03). 새 액터 · 새 습득 RPC 를 만들지 않는다.
	//   상자와 다른 점 두 가지: 1분 뒤 사라지고, 다 가져가도 사라진다 (원작 [확인] 2026-09-23 · 역기획서 몬스터 §3.3).
	const FVector Loc = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Corpse = GetWorld()->SpawnActor<AERItemDropActor>(AERItemDropActor::StaticClass(), Loc, GetActorRotation(), Params);
	if (!Corpse)
	{
		return;
	}
	// 시체는 **비어도 남는다** — 원작은 시체를 열어 필요한 것만 가져간다 (사용자 2026-09-23). 사라지는 건 시간(1분)뿐이라 몸과 수명을 맞춘다.
	Corpse->InitializeCorpse(Data->LootRow, UERItemSettings::Get().WildlifeCorpseSeconds, /*bKeepWhenEmpty=*/true, Level);
	Corpse->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
}

void AERWildlifeCharacter::EnterCorpseState()
{
	// 원작: 사망 애니메이션 상태로 그 자리에 남고, 클릭하면 인벤토리처럼 열린다 (사용자 2026-09-23). 애니 · 루팅 창은 F17.
	// 여기서는 "더 못 때리고 · 통과되고 · 클릭은 되는" 상태만 만든다.
	// ⭐ 태그가 본체다 — SingleTarget 은 커서가 짚은 액터를 그대로 쓰므로 콜리전만 꺼서는 안 걸러진다 (ERTargeting::PassesFilter).
	AbilitySystemComponent->AddLooseGameplayTag(ERTags::State_Untargetable);
	AbilitySystemComponent->AddReplicatedLooseGameplayTag(ERTags::State_Untargetable);
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // 몸으로 길을 막지 않는다
	}
	if (HitBox)
	{
		HitBox->SetCollisionResponseToChannel(ERCollisionChannel::SkillTarget, ECR_Ignore);   // 시체는 판정 대상이 아니다
		HitBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		// ⭐ Select(클릭)는 **그대로 둔다** — 이게 루팅 상호작용의 대상이다.
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
		Move->SetComponentTickEnabled(false);
	}
	if (USkeletalMeshComponent* BodyMesh = GetMesh())
	{
		BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	FlushNetDormancy();   // 시체 · 컨테이너 참조가 클라로 가게
	SetLifeSpan(UERItemSettings::Get().WildlifeCorpseSeconds);   // 1분 [확인] — 재등장은 리스폰 + 이 시간 (역기획서 §3.3)
}

void AERWildlifeCharacter::GrantKillRewards(AActor* Killer)
{
	// 처치자 = 피해를 준 액터(아바타) → PlayerState. 어시스트 분배는 없다 — 처치자만 [자체] (Task 02).
	const APawn* KillerPawn = Cast<APawn>(Killer);
	AERPlayerState* PS = KillerPawn ? KillerPawn->GetPlayerState<AERPlayerState>() : Cast<AERPlayerState>(Killer);
	UERGrowthComponent* Growth = PS ? PS->GetGrowth() : nullptr;
	if (!Growth)
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[야생동물] %s 처치자에게서 성장 컴포넌트를 못 찾았다 (%s) — 보상 없음."), *GetName(), *GetNameSafe(Killer));
		return;
	}
	// 사냥 숙련도 = 종 정의의 기본값 + 레벨당 × Lv ([확인] 역기획서 §1.2). 값이 없는 종(변이 · 보스 · 미확인)은 공통 설정으로 폴백.
	const UERGrowthSettings& S = UERGrowthSettings::Get();
	const float HuntExp = (Data && Data->HuntExpBase > 0.f)
		? Data->HuntExpBase + Data->HuntExpPerLevel * Level
		: S.HuntExpPerKillBase + S.HuntExpPerKillPerLevel * Level;
	Growth->OnWildlifeKilled(Level, HuntExp);   // 경험치는 숙련도 입구 하나로 흐른다 (F10-05)
}

void AERWildlifeCharacter::SetHome(AERWildlifeSpawnPoint* Point, const FVector& Location)
{
	HomePoint = Point;
	HomeLocation = Location;
	bHasHome = true;
}

void AERWildlifeCharacter::SetBodyActive(bool bActive)
{
	if (!HasAuthority())
	{
		return;
	}
	const UERWildlifeSettings& S = UERWildlifeSettings::Get();
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetComponentTickEnabled(bActive);
	}
	if (USkeletalMeshComponent* BodyMesh = GetMesh())
	{
		BodyMesh->SetComponentTickEnabled(bActive);
	}
	if (Data && Data->bBoss)
	{
		return;   // 보스는 도먼시 · 복제 빈도를 바꾸지 않는다 (DORM_Never · 30Hz — Initialize). 몸 틱만 위에서.
	}
	NetUpdateFrequency = bActive ? S.CombatNetUpdateFrequency : S.IdleNetUpdateFrequency;
	if (bActive)
	{
		SetNetDormancy(DORM_Awake);   // 전투 · 귀환 — 이동이 클라로 가야 한다
		FlushNetDormancy();
	}
	else
	{
		FlushNetDormancy();           // 마지막 상태(회복된 체력 · 위치)를 보낸 뒤
		SetNetDormancy(DORM_DormantAll);
	}
}

void AERWildlifeCharacter::OnRep_Data()
{
	ApplyVisuals();
}

void AERWildlifeCharacter::HandleOutOfHealth(AActor* Killer)
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	UE_LOG(LogEternalReturn, Log, TEXT("[야생동물] %s (%s Lv.%d) 사망 — 처치 %s"), *GetName(), *GetNameSafe(Data), Level, *GetNameSafe(Killer));

	if (AERWildlifeAIController* AI = GetController<AERWildlifeAIController>())
	{
		AI->NotifyPawnDied();   // 판단 · 이동을 멈춘다
	}
	SpawnCorpse();
	GrantKillRewards(Killer);
	OnWildlifeKilled.Broadcast(this, Killer);   // 03 리스폰 스케줄이 구독한다
	EnterCorpseState();
}
