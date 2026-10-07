// Copyright Epic Games, Inc. All Rights Reserved.

#include "Wildlife/ERWildlifeCharacter.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/BoxComponent.h"
#include "Combat/ERForcedMoveComponent.h"
#include "Components/CapsuleComponent.h"
#include "ERCollisionChannels.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "EternalReturn.h"
#include "GAS/ERAttributeInit.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Core/ERPlayerState.h"
#include "Growth/ERGrowthComponent.h"
#include "Growth/ERGrowthSettings.h"
#include "Presentation/ERPresentationComponent.h"
#include "Item/ERItemDropActor.h"
#include "Item/ERItemSettings.h"
#include "Wildlife/ERWildlifeAIController.h"
#include "Wildlife/ERWildlifeData.h"
#include "Wildlife/ERWildlifeSettings.h"
#include "Wildlife/ERWildlifeSpawnPoint.h"
#include "Wildlife/ERWildlifeSpawnSubsystem.h"

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

	// 연출 (F12.5-01) · P1 — 안 그려지면 몽타주만 진행 (실험체와 같은 이유 · ERCharacterBase 생성자 참조).
	Presentation = CreateDefaultSubobject<UERPresentationComponent>(TEXT("Presentation"));
	ForcedMove = CreateDefaultSubobject<UERForcedMoveComponent>(TEXT("ForcedMove"));
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
		// Argument 40 ⑤ — URO · 고정 바운드 (실험체와 같은 이유. 120~200 마리라 효과가 더 크다 — 역기획서 §7.3).
		MeshComp->bEnableUpdateRateOptimizations = true;
		MeshComp->bComponentUseFixedSkelBounds = true;
	}
}

void AERWildlifeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AERWildlifeCharacter, Data, COND_InitialOnly);
	DOREPLIFETIME(AERWildlifeCharacter, Level);   // 시간 성장(03)이 바꾼다
	DOREPLIFETIME(AERWildlifeCharacter, Corpse);  // 죽은 뒤 한 번 — 클라가 클릭해서 열 대상
	DOREPLIFETIME(AERWildlifeCharacter, bDead);   // 사망 포즈 (AnimBP) — Argument 36 "상태는 복제 값"
	DOREPLIFETIME(AERWildlifeCharacter, DeathServerTime);   // bDead 와 같은 묶음으로 간다 — OnRep_Dead 때 이미 있다
	DOREPLIFETIME(AERWildlifeCharacter, PresState);         // 연출 상태 + 바뀐 시각 (F12.6-01 · Argument 50 N1)
}

void AERWildlifeCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// [진단] F12.5-06 스트레스 — 200마리 스폰 뒤 30초 안에 87마리만 남았다 (로그 없이 사라짐). 살아 있는 채로 지워지면 사유 · 위치를 남긴다
	if (HasAuthority() && !bDead && EndPlayReason == EEndPlayReason::Destroyed)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[야생동물] %s 살아 있는 채로 제거 — 위치 %s"), *GetName(), *GetActorLocation().ToCompactString());
	}
	if (UERWildlifeSpawnSubsystem* Sub = HasAuthority() ? GetWorld()->GetSubsystem<UERWildlifeSpawnSubsystem>() : nullptr)
	{
		Sub->UnregisterAnimal(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AERWildlifeCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Owner = Avatar = 이 폰. 서버 · 클라 양쪽 — 클라도 어트리뷰트 복제를 받으려면 ActorInfo 가 있어야 한다.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	// BeginPlay 전에 받은 연출 전이 (새로 relevant 된 액터 — 등장 등). 시각은 지금 기준으로 다시 잰다.
	if (bPresPending)
	{
		bPresPending = false;
		const AGameStateBase* GS = GetWorld()->GetGameState();
		PlayPresTransition(PendingPresOld, PresState.State, GS ? GS->GetServerWorldTimeSeconds() - PresState.ServerTime : 0.f);
	}

	// 경계 · 수면 판정 대상 (F12.6-02 · 서버) — 스폰 시각부터 "아무도 없음" 을 잰다
	if (HasAuthority())
	{
		LastPlayerNearTime = GetWorld()->GetTimeSeconds();
		if (UERWildlifeSpawnSubsystem* Sub = GetWorld()->GetSubsystem<UERWildlifeSpawnSubsystem>())
		{
			Sub->RegisterAnimal(this);
		}
	}

	if (HasAuthority() && AttributeSet)
	{
		AttributeSet->OnOutOfHealth.AddWeakLambda(this, [this](AActor* Killer) { HandleOutOfHealth(Killer); });
		// 맞으면 깨운다 — 체력 변화가 클라로 가야 한다. 전투 진입 · 재우기는 AI 가 (F12-04).
		AttributeSet->OnDamageTaken.AddWeakLambda(this, [this](AActor* Attacker, float Damage)
		{
			FlushNetDormancy();
			// 야생동물을 때려도 무기 숙련도 (원작 "야생 동물 사냥" — 사용자 자료 2026-10-01). 실험체 피해와 같은 입구 — 분기는 OnDamageDealt 가 (야생동물 배율)
			//   지금까지 이 줄이 없어 `야생동물 피해` 무숙이 0 이었다 (F10 체크리스트 05 대조)
			if (AERPlayerState* AttackerPS = Cast<AERPlayerState>(Attacker); AttackerPS && AttackerPS->GetGrowth())
			{
				AttackerPS->GetGrowth()->OnDamageDealt(this, Damage);
			}
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

	// 등장 (F12.6-01) — 첫 초기화 한 번. 선공(아래)보다 먼저: 같은 프레임이면 클라는 마지막 값만 받지만 None → * 는 등장으로 본다.
	if (!bInitializedOnce)
	{
		SetPresState(EERWildlifePresState::Idle);
	}

	// 스폰 선공 (선제 공격 종 후보). 첫 초기화 때만.
	if (!bInitializedOnce && Data->SpawnAggroRadius > 0.f)
	{
		if (AERWildlifeAIController* AI = GetController<AERWildlifeAIController>())
		{
			AI->AggroNearestInRadius(Data->SpawnAggroRadius);
		}
	}
	// 실험 대상 추적 (위클라인 20m — **생성 때 한 번** · 다가가기만 · F12.6-06)
	if (!bInitializedOnce && Data->SpawnTrackRadius > 0.f)
	{
		if (AERWildlifeAIController* AI = GetController<AERWildlifeAIController>())
		{
			AI->StartTrackingNearest(Data->SpawnTrackRadius);
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
	// 서버(Initialize) · 클라(OnRep_Data) 양쪽이 여기를 지난다.
	if (Presentation)
	{
		Presentation->SetBase(Data->Presentation);
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
	PS->ClientPlayVoice(ERTags::Pres_Voice_KillMonster);   // 야생동물 처치 혼잣말 (Argument 73 · 본인)
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
		if (!bActive)
		{
			// ⭐ 잠들기 전에 속도를 0 으로 — AI StopMovement 는 경로만 끊고 CMC Velocity 는 남긴다. 그대로 틱을 끄면
			//   마지막 복제 속도가 달리는 값으로 굳어 클라 AnimBP 가 계속 뛴다 (2026-09-28 "제자리로 돌아가면 Idle 로 안 돌아감").
			const float Speed = Move->Velocity.Size2D();
			if (Speed > 1.f)
			{
				UE_LOG(LogEternalReturn, Log, TEXT("[야생동물] %s 잠들기 전 속도 %.0f → 0"), *GetName(), Speed);
			}
			Move->StopMovementImmediately();
		}
		Move->SetComponentTickEnabled(bActive);
	}
	// 메시 틱은 **데디 서버에서만** 끈다 — 리슨 서버 호스트 화면에서는 대기 모션이 돌아야 한다 (끄면 포즈가 굳는다).
	//   렌더 안 되는 메시의 비용은 P1(OnlyTickMontagesWhenNotRendered) · URO 가 이미 줄인다 (Argument 40 ⑤). ⏸ 06 에서 측정.
	if (GetNetMode() == NM_DedicatedServer)
	{
		if (USkeletalMeshComponent* BodyMesh = GetMesh())
		{
			BodyMesh->SetComponentTickEnabled(bActive);
		}
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

void AERWildlifeCharacter::HandleGameplayCue(UObject* Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	if (EventType == EGameplayCueEvent::Executed && Presentation)
	{
		Presentation->HandlePresCue(GameplayCueTag, Parameters);
	}
	IGameplayCueInterface::HandleGameplayCue(Self, GameplayCueTag, EventType, Parameters);
}

void AERWildlifeCharacter::OnRep_Dead()
{
	// ⭐ 새로 relevant 된 액터는 복제 값 OnRep 이 PostNetInit(BeginPlay) **전에** 불린다 (DataChannel.cpp:3331 → 3345)
	//   → 그때 이미 죽어 있었다 = 쓰러지는 걸 못 본 클라. 누운 채로 시작한다 (Task 04 "늦게 relevant 된 클라도 누운 시체").
	// ⭐ 그것만으로는 모자란다 — 대기 야생동물은 DormantAll 이라 **한 번 본 클라에 산 채로 남는다** (도먼시로 닫힌 채널은
	//   클라가 액터를 지우지 않는다). 떠났다 돌아오면 BeginPlay 가 끝난 액터에 bDead 가 늦게 온다 (F12.5-06 2026-09-30 로그).
	//   → 죽은 지 LateDeathSeconds 가 지났으면 누운 채로. 서버 시각 오차(핑 · GameState 동기 주기)보다 넉넉히 1초.
	static constexpr float LateDeathSeconds = 1.f;
	if (bDead && Presentation)
	{
		const AGameStateBase* GS = GetWorld()->GetGameState();
		const float Elapsed = GS ? GS->GetServerWorldTimeSeconds() - DeathServerTime : 0.f;
		const bool bLate = !HasActorBegunPlay() || Elapsed > LateDeathSeconds;
		UE_LOG(LogEternalReturn, Log, TEXT("[야생동물] %s 사망 수신 — 죽은 지 %.2f초 · BeginPlay %s"),
			*GetName(), Elapsed, HasActorBegunPlay() ? TEXT("끝남") : TEXT("전"));
		Presentation->SetDead(bLate);
	}
}

namespace
{
	const TCHAR* PresStateName(EERWildlifePresState S)
	{
		switch (S)
		{
		case EERWildlifePresState::Idle:   return TEXT("대기");
		case EERWildlifePresState::Combat: return TEXT("전투");
		case EERWildlifePresState::Return: return TEXT("귀환");
		case EERWildlifePresState::Beware: return TEXT("경계");
		case EERWildlifePresState::Sleep:  return TEXT("잠");
		default:                           return TEXT("없음");
		}
	}
	/** 이보다 오래된 전이는 사건을 틀지 않는다 — 사망(OnRep_Dead)과 같은 1초 (서버 시각 오차 여유 · E32). */
	constexpr float LatePresSeconds = 1.f;
}

void AERWildlifeCharacter::SetPresState(EERWildlifePresState NewState)
{
	if (!HasAuthority() || PresState.State == NewState)
	{
		return;
	}
	const EERWildlifePresState Old = PresState.State;
	PresState.State = NewState;
	// 전투 · 귀환에서 막 돌아왔으면 "아무도 없음" 을 다시 잰다 — 안 그러면 도착하자마자(endbattle 중) 잠든다 (2026-09-30 로그: 도착 13ms 뒤 잠) [자체]
	if (NewState == EERWildlifePresState::Idle && (Old == EERWildlifePresState::Combat || Old == EERWildlifePresState::Return))
	{
		LastPlayerNearTime = GetWorld()->GetTimeSeconds();
	}
	const AGameStateBase* GS = GetWorld()->GetGameState();
	PresState.ServerTime = GS ? GS->GetServerWorldTimeSeconds() : 0.f;
	FlushNetDormancy();
	PlayPresTransition(Old, NewState, 0.f);   // 리슨 호스트 화면 (데디는 연출 컴포넌트가 건너뛴다)
}

void AERWildlifeCharacter::SenseNearby(float NearestMeters, double Now)
{
	const EERWildlifePresState Cur = PresState.State;
	const bool bResting = Cur == EERWildlifePresState::Idle || Cur == EERWildlifePresState::Beware || Cur == EERWildlifePresState::Sleep;
	if (!Data || Data->bBoss || bDead || !bResting)
	{
		return;
	}
	const UERWildlifeSettings& S = UERWildlifeSettings::Get();
	if (NearestMeters <= S.SleepMeters)
	{
		LastPlayerNearTime = Now;
	}
	// 규칙 (Argument 51 B1 · 사용자 확인 2026-09-30 "다가가면 경계 · 근처에 사람이 없을 때 잠") — 수치 [자체]
	EERWildlifePresState Next = Cur;
	switch (Cur)
	{
	case EERWildlifePresState::Sleep:
		if (NearestMeters <= S.SleepMeters)
		{
			Next = (Data->bCanBeware && NearestMeters <= S.BewareMeters) ? EERWildlifePresState::Beware : EERWildlifePresState::Idle;   // 깬다
		}
		break;
	case EERWildlifePresState::Beware:
		if (NearestMeters > S.BewareExitMeters)
		{
			Next = EERWildlifePresState::Idle;
		}
		break;
	default:   // Idle
		if (Data->bCanBeware && NearestMeters <= S.BewareMeters)
		{
			Next = EERWildlifePresState::Beware;
		}
		else if (Data->bCanSleep && Now - LastPlayerNearTime >= S.SleepDelaySeconds)
		{
			Next = EERWildlifePresState::Sleep;
		}
		break;
	}
	if (Next != Cur)
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[야생AI] %s %s → %s (가장 가까운 실험체 %.1fm)"), *GetName(), PresStateName(Cur), PresStateName(Next),
			NearestMeters > 1e6f ? -1.f : NearestMeters);
		SetPresState(Next);
	}
}

void AERWildlifeCharacter::OnRep_PresState(const FERWildlifePresState& OldState)
{
	if (!HasActorBegunPlay())
	{
		// 애님 인스턴스 · 연출 표가 아직일 수 있다 — BeginPlay 에서. 여러 번 받으면 처음 Old 를 지킨다 (None → 등장).
		if (!bPresPending)
		{
			PendingPresOld = OldState.State;
			bPresPending = true;
		}
		return;
	}
	const AGameStateBase* GS = GetWorld()->GetGameState();
	PlayPresTransition(OldState.State, PresState.State, GS ? GS->GetServerWorldTimeSeconds() - PresState.ServerTime : 0.f);
}

void AERWildlifeCharacter::PlayPresTransition(EERWildlifePresState Old, EERWildlifePresState New, float Elapsed)
{
	if (!Presentation || bDead)
	{
		return;
	}
	// 경계 · 잠 = **상태** → AnimBP 상태머신 (Argument 48). 매 전이마다 넘긴다 (늦게 받았으면 시작 동작 없이 반복부터).
	const bool bLate = Elapsed > LatePresSeconds;
	Presentation->SetRestPose(New == EERWildlifePresState::Beware ? EERRestPose::Beware : New == EERWildlifePresState::Sleep ? EERRestPose::Sleep : EERRestPose::None, bLate);
	// 전이 → 사건 (Task F12.6-01 표). 발견은 소리만 (Argument 50 (b)) · 전투 끝은 자리 도착 때 [자체] (귀환 출발에 틀면 걸으며 미끄러진다).
	FGameplayTag AnimKey;
	FGameplayTag SfxKey;
	const TCHAR* What = nullptr;
	if (Old == EERWildlifePresState::None && New != EERWildlifePresState::Idle)
	{
		// 처음 받았는데 이미 경계 · 잠 · 전투 — 스폰이 아니라 **나중에 보게 된 것** (시각은 그 전이의 것). 등장을 틀지 않는다
		//   (2026-09-30 데디 로그: 늦게 들어온 클라가 경계 중인 닭에 `없음 → 경계 · 등장` 을 틀었다)
		return;
	}
	if (Old == EERWildlifePresState::None)
	{
		AnimKey = ERTags::Pres_Anim_Appear;
		SfxKey = ERTags::Pres_Sfx_Appear;
		What = TEXT("등장");
	}
	else if (New == EERWildlifePresState::Combat && (Old == EERWildlifePresState::Idle || Old == EERWildlifePresState::Beware || Old == EERWildlifePresState::Sleep))
	{
		SfxKey = ERTags::Pres_Sfx_Discover;
		What = TEXT("발견");
	}
	else if (New == EERWildlifePresState::Beware)
	{
		SfxKey = ERTags::Pres_Sfx_Beware;   // 몸짓은 AnimBP (위 SetRestPose)
		What = TEXT("경계");
	}
	else if (New == EERWildlifePresState::Idle && Old != EERWildlifePresState::Beware && Old != EERWildlifePresState::Sleep)
	{
		AnimKey = ERTags::Pres_Anim_EndBattle;
		What = TEXT("전투 끝");
	}
	if (!What)
	{
		return;   // 전투 → 귀환 — 사건 없음
	}
	const TCHAR* Side = HasAuthority() ? TEXT("서버") : TEXT("클라");
	if (Elapsed > LatePresSeconds)
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[야생연출] %s %s → %s · %s (늦음 %.1f초 · 건너뜀) (%s)"),
			*GetName(), PresStateName(Old), PresStateName(New), What, Elapsed, Side);
		return;
	}
	const FString Played = Presentation->PlayEventPres(AnimKey, SfxKey);
	UE_LOG(LogEternalReturn, Log, TEXT("[야생연출] %s %s → %s · %s %s (%s · %.2f초)"),
		*GetName(), PresStateName(Old), PresStateName(New), What, *Played, Side, Elapsed);
}

void AERWildlifeCharacter::HandleOutOfHealth(AActor* Killer)
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	if (const AGameStateBase* GS = GetWorld()->GetGameState())
	{
		DeathServerTime = GS->GetServerWorldTimeSeconds();
	}
	if (Presentation)
	{
		Presentation->SetDead(false);   // 서버(리슨 호스트 화면) — 클라는 OnRep_Dead
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[야생동물] %s (%s Lv.%d) 사망 — 처치 %s"), *GetName(), *GetNameSafe(Data), Level, *GetNameSafe(Killer));

	if (AERWildlifeAIController* AI = GetController<AERWildlifeAIController>())
	{
		AI->NotifyPawnDied();   // 판단 · 이동을 멈춘다
	}
	// 무리에게 알린다 — 짝이 죽으면 쓰는 스킬 (늑대 울부짖기 · F12.6-03). 무리 = 같은 스폰 자리 (GetPackMates · 살아 있는 개체만)
	if (const UERWildlifeSpawnSubsystem* Sub = GetWorld()->GetSubsystem<UERWildlifeSpawnSubsystem>())
	{
		TArray<AERWildlifeCharacter*> Mates;
		Sub->GetPackMates(this, Mates);
		for (AERWildlifeCharacter* Mate : Mates)
		{
			if (AERWildlifeAIController* MateAI = Mate ? Mate->GetController<AERWildlifeAIController>() : nullptr)
			{
				MateAI->NotifyAllyDied();
			}
		}
	}
	SpawnCorpse();
	GrantKillRewards(Killer);
	OnWildlifeKilled.Broadcast(this, Killer);   // 03 리스폰 스케줄이 구독한다
	EnterCorpseState();
}
