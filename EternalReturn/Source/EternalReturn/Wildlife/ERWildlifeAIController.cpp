// Copyright Epic Games, Inc. All Rights Reserved.

#include "Wildlife/ERWildlifeAIController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/ERTargeting.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/ERGameState.h"
#include "Core/ERPlayerState.h"
#include "Core/ERTeamStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EternalReturn.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "Wildlife/ERWildlifeCharacter.h"
#include "Wildlife/ERWildlifeData.h"
#include "Wildlife/ERWildlifeSettings.h"
#include "Wildlife/ERWildlifeSpawnSubsystem.h"

AERWildlifeAIController::AERWildlifeAIController()
{
	// ⭐ 컨트롤러 액터 틱은 쓰지 않는다 — 판단은 전투 중 타이머, 대기 200마리는 아무것도 안 돈다 (Argument 35 A).
	PrimaryActorTick.bStartWithTickEnabled = false;
}

const TCHAR* AERWildlifeAIController::StateName(EERWildlifeAIState InState)
{
	switch (InState)
	{
	case EERWildlifeAIState::Idle:   return TEXT("대기");
	case EERWildlifeAIState::Combat: return TEXT("전투");
	case EERWildlifeAIState::Return: return TEXT("귀환");
	case EERWildlifeAIState::Tracking: return TEXT("추적");
	}
	return TEXT("?");
}

AERWildlifeCharacter* AERWildlifeAIController::GetWildlife() const
{
	return Cast<AERWildlifeCharacter>(GetPawn());
}

void AERWildlifeAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	AERWildlifeCharacter* Me = GetWildlife();
	if (!Me)
	{
		return;
	}
	// 디버그 스폰(ER.Wild.Spawn · Stress)은 자리가 없다 — 빙의 위치를 자리로.
	if (!Me->HasHome())
	{
		Me->SetHome(nullptr, Me->GetActorLocation());
	}
	// 대기로 시작 — 몸 틱 끔 · DormantAll. SetState 는 같은 상태면 건너뛰므로 직접 부른다.
	State = EERWildlifeAIState::Idle;
	Me->SetBodyActive(false);
	if (UPathFollowingComponent* PF = GetPathFollowingComponent())
	{
		PF->SetComponentTickEnabled(false);
	}
}

void AERWildlifeAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	Super::OnUnPossess();
}

void AERWildlifeAIController::NotifyDamaged(AActor* Attacker)
{
	if (bStopped)
	{
		return;
	}
	// 돌아다니는 종 (위클라인) — 전투 중 맞을 때마다 그 자리가 새 기준 (사용자 2026-10-01). 어그로 한계 10m 는 "마지막으로 맞은 곳에서" 가 된다. 귀환 중엔 그대로
	if (State == EERWildlifeAIState::Combat)
	{
		if (AERWildlifeCharacter* Roamer = GetWildlife(); Roamer && Roamer->GetData() && Roamer->GetData()->bRoams)
		{
			Roamer->MoveHomeHere();
			UE_LOG(LogEternalReturn, Log, TEXT("[야생AI] %s 피격 — 새 자리 %s"), *GetNameSafe(Roamer), *Roamer->GetHomeLocation().ToCompactString());
		}
	}
	// ⭐ 전투 중 다른 팀이 때려도 **먼저 때린 팀 우선** · 귀환 중에는 **돌아서지 않는다** (사용자 2026-09-24). 추적 중엔 먼저 때린 사람과 싸운다 (F12.6-06).
	if (State != EERWildlifeAIState::Idle && State != EERWildlifeAIState::Tracking)
	{
		return;
	}
	// ⚠ 치명타면 아무것도 안 한다 — 이 알림(OnDamageTaken)은 **사망 판정보다 먼저** 온다 (ERAttributeSet).
	//   여기서 귀환 → 회복까지 가면 체력 0 이 되돌려져 **죽어야 할 개체가 산다** (E24).
	const AERWildlifeCharacter* Me = GetWildlife();
	const UAbilitySystemComponent* MyASC = Me ? Me->GetAbilitySystemComponent() : nullptr;
	if (!MyASC || MyASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute()) <= 0.f)
	{
		return;
	}
	const int32 Team = ERTeamStatics::GetTeamId(Attacker);
	if (Team == INDEX_NONE)
	{
		return;   // 팀이 없는 공격자 (다른 야생동물 · 환경) — 어그로 대상이 아니다
	}
	EnterCombat(Team, /*bPropagateToPack=*/true);
}

void AERWildlifeAIController::EnterCombat(int32 TeamId, bool bPropagateToPack, const TCHAR* Reason)
{
	AERWildlifeCharacter* Me = GetWildlife();
	if (bStopped || !Me || Me->IsDead() || (State != EERWildlifeAIState::Idle && State != EERWildlifeAIState::Tracking))
	{
		return;
	}
	AggroTeam = TeamId;
	SetState(EERWildlifeAIState::Combat, Reason ? Reason : (bPropagateToPack ? TEXT("피격") : TEXT("무리 전파")));

	// ⭐ 무리는 전체가 달려든다 (사용자 2026-09-24). 전파받은 쪽은 다시 전파하지 않는다.
	if (bPropagateToPack)
	{
		if (const UERWildlifeSpawnSubsystem* Sub = GetWorld()->GetSubsystem<UERWildlifeSpawnSubsystem>())
		{
			TArray<AERWildlifeCharacter*> Mates;
			Sub->GetPackMates(Me, Mates);
			for (AERWildlifeCharacter* Mate : Mates)
			{
				if (AERWildlifeAIController* MateAI = Mate->GetController<AERWildlifeAIController>())
				{
					MateAI->EnterCombat(TeamId, false);
				}
			}
		}
	}
	// 첫 판단은 **다음 틱** — 피해 알림 안에서 상태를 바꾸지 않는다 (E24: 알림 도중 귀환 · 회복이 사망 판정을 앞질렀다).
	GetWorldTimerManager().SetTimerForNextTick(this, &AERWildlifeAIController::Think);
}

void AERWildlifeAIController::AggroNearestInRadius(float Meters)
{
	const AERWildlifeCharacter* Me = GetWildlife();
	const AERGameState* GS = GetWorld() ? GetWorld()->GetGameState<AERGameState>() : nullptr;
	if (!Me || !GS || State != EERWildlifeAIState::Idle)
	{
		return;
	}
	const AERPlayerState* Nearest = nullptr;
	float BestDistSq = FMath::Square(Meters * 100.f);
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const AERPlayerState* ERPS = Cast<AERPlayerState>(PS);
		const APawn* Candidate = ERPS ? ERPS->GetPawn() : nullptr;
		if (!Candidate || ERPS->TeamId == INDEX_NONE)
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(Candidate->GetActorLocation(), Me->GetActorLocation());
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Nearest = ERPS;
		}
	}
	if (Nearest)
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[야생AI] %s 스폰 선공 — %.0fm 안 %s (팀 %d)"), *Me->GetName(), Meters, *Nearest->GetName(), Nearest->TeamId);
		EnterCombat(Nearest->TeamId, /*bPropagateToPack=*/false, TEXT("스폰 선공"));
	}
}

void AERWildlifeAIController::NotifyPawnDied()
{
	bStopped = true;
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	StopMovement();
}

void AERWildlifeAIController::SetState(EERWildlifeAIState NewState, const TCHAR* Reason)
{
	AERWildlifeCharacter* Me = GetWildlife();
	if (!Me || State == NewState)
	{
		return;
	}
	const EERWildlifeAIState Old = State;
	const UERWildlifeSettings& S = UERWildlifeSettings::Get();

	// 이동 요청을 먼저 끊는다 — 끊긴 요청의 OnMoveCompleted 가 새 상태로 잘못 해석되지 않게.
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	StopMovement();
	MoveTarget.Reset();
	State = NewState;

	// 연출 상태 (F12.6-01) — 클라가 발견음 · 전투 끝을 튼다. 몸 전환(아래) **전에** — 재우기 직전 마지막 전송에 실린다.
	// 추적은 연출상 "전투" (다가오는 몸짓 · 발견음 자리 = 위클라인 추적 시작음) — 추적 → 전투는 같은 값이라 사건 없음
	Me->SetPresState((NewState == EERWildlifeAIState::Combat || NewState == EERWildlifeAIState::Tracking) ? EERWildlifePresState::Combat
		: NewState == EERWildlifeAIState::Return ? EERWildlifePresState::Return : EERWildlifePresState::Idle);
	const bool bActive = NewState != EERWildlifeAIState::Idle;
	Me->SetBodyActive(bActive);   // ⭐ 도먼시 · CMC/메시 틱 · 복제 빈도 — 한 곳
	if (UPathFollowingComponent* PF = GetPathFollowingComponent())
	{
		PF->SetComponentTickEnabled(bActive);
	}

	switch (NewState)
	{
	case EERWildlifeAIState::Idle:
		AggroTeam = INDEX_NONE;
		break;
	case EERWildlifeAIState::Combat:
		TrackTarget.Reset();   // 한 번이라도 전투가 되면 추적은 끝
		// 돌아다니는 종 (위클라인) — 전투가 시작된 지점이 새 자리 (사용자 2026-10-01). 어그로 한계 · 귀환이 여기 기준
		if (Me->GetData() && Me->GetData()->bRoams && (Old == EERWildlifeAIState::Idle || Old == EERWildlifeAIState::Tracking))
		{
			Me->MoveHomeHere();
			UE_LOG(LogEternalReturn, Log, TEXT("[야생AI] %s 돌아다니는 종 — 전투 시작 지점이 새 자리 %s"), *GetNameSafe(Me), *Me->GetHomeLocation().ToCompactString());
		}
		GetWorldTimerManager().SetTimer(ThinkTimer, this, &AERWildlifeAIController::Think, S.ThinkInterval, true);
		break;
	case EERWildlifeAIState::Tracking:
		GetWorldTimerManager().SetTimer(ThinkTimer, this, &AERWildlifeAIController::ThinkTracking, S.ThinkInterval, true);
		break;
	case EERWildlifeAIState::Return:
	{
		// "돌아갈 때는 체력이 회복된다" (사용자 2026-09-24) — 출발할 때 한 번, 도착해서 한 번 (귀환 중 맞은 만큼).
		Heal();
		const EPathFollowingRequestResult::Type R = MoveToLocation(Me->GetHomeLocation(), /*AcceptanceRadius=*/30.f, /*bStopOnOverlap=*/false,
			/*bUsePathfinding=*/true, /*bProjectDestinationToNavigation=*/true, /*bCanStrafe=*/false, nullptr, /*bAllowPartialPath=*/true);
		ReturnRequestId = GetCurrentMoveRequestID();
		if (R != EPathFollowingRequestResult::RequestSuccessful)
		{
			// 이미 자리에 있거나 길이 없다 — 바로 도착 처리. 아래 공통 로그를 건너뛰므로 왜 귀환했는지 여기서 남긴다 (2026-10-01 이유 없는 "귀환 즉시 완료" 반복)
			UE_LOG(LogEternalReturn, Log, TEXT("[야생AI] %s %s → 귀환 (%s)"), *Me->GetName(), StateName(Old), Reason);
			Heal();
			SetState(EERWildlifeAIState::Idle, TEXT("귀환 즉시 완료"));
			return;
		}
		// 귀환 중에도 움직이며 쓰는 스킬 (위클라인 유해 물질) — Think 가 귀환이면 그것만 한다
		GetWorldTimerManager().SetTimer(ThinkTimer, this, &AERWildlifeAIController::Think, S.ThinkInterval, true);
		break;
	}
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[야생AI] %s %s → %s (%s)%s"), *Me->GetName(), StateName(Old), StateName(NewState), Reason,
		NewState == EERWildlifeAIState::Combat ? *FString::Printf(TEXT(" · 어그로 팀 %d"), AggroTeam) : TEXT(""));
}

void AERWildlifeAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);
	if (State == EERWildlifeAIState::Return && RequestID == ReturnRequestId)
	{
		Heal();
		SetState(EERWildlifeAIState::Idle, Result.IsSuccess() ? TEXT("자리 도착") : TEXT("귀환 경로 끊김"));
	}
}

void AERWildlifeAIController::Heal()
{
	AERWildlifeCharacter* Me = GetWildlife();
	UAbilitySystemComponent* ASC = Me ? Me->GetAbilitySystemComponent() : nullptr;
	// ⭐ 위클라인은 어그로가 풀려도 **회복하지 않는다** (역기획서 §5.3 · 사용자 제공 2026-09-24) — 팀 사이 눈치 싸움의 원천.
	if (Me && Me->GetData() && !Me->GetData()->bHealOnReturn)
	{
		return;
	}
	// 체력 0 이하는 회복하지 않는다 — 사망 판정 전이라 bDead 가 아직 false 일 수 있다 (E24).
	if (ASC && !Me->IsDead() && ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute()) > 0.f)
	{
		ASC->SetNumericAttributeBase(UERAttributeSet::GetHPAttribute(), ASC->GetNumericAttribute(UERAttributeSet::GetMaxHPAttribute()));
	}
}

APawn* AERWildlifeAIController::PickTarget() const
{
	const AERWildlifeCharacter* Me = GetWildlife();
	const AERGameState* GS = GetWorld() ? GetWorld()->GetGameState<AERGameState>() : nullptr;
	if (!Me || !GS || AggroTeam == INDEX_NONE)
	{
		return nullptr;
	}
	// ⭐ 대상 = 어그로 팀에서 **가장 가까운 생존자** — 때린 사람이 아니라 팀 기준 (사용자 2026-09-24). 매 판단마다 다시 고른다.
	APawn* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const AERPlayerState* ERPS = Cast<AERPlayerState>(PS);
		APawn* Candidate = ERPS && ERPS->TeamId == AggroTeam ? ERPS->GetPawn() : nullptr;
		const UAbilitySystemComponent* ASC = Candidate ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Candidate) : nullptr;
		if (!ASC || ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute()) <= 0.f || ASC->HasMatchingGameplayTag(ERTags::State_Untargetable))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(Candidate->GetActorLocation(), Me->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	}
	return Best;
}

void AERWildlifeAIController::Think()
{
	AERWildlifeCharacter* Me = GetWildlife();
	if (bStopped || !Me || Me->IsDead())
	{
		return;
	}
	TryWhileMovingSkills();   // 위클라인 유해 물질 — 전투 · 귀환 둘 다
	if (State != EERWildlifeAIState::Combat)
	{
		return;
	}
	// 돌아다니는 종 (위클라인) — 통제 돌진처럼 **맞힌 뒤 날아가는** 동안은 한계를 보지 않고, 착지하면 그 자리가 새 기준 (2026-10-01 로그 21:30:39 돌진 9.9m 직후 귀환)
	if (Me->GetData() && Me->GetData()->bRoams)
	{
		const UCharacterMovementComponent* CMC = Me->GetCharacterMovement();
		const bool bRootMoving = CMC && CMC->HasRootMotionSources();
		if (bRootMoving)
		{
			bWasRootMoving = true;
			return;   // 이동 중엔 판단도 쉰다 (스킬 · 추격은 착지 뒤)
		}
		if (bWasRootMoving)
		{
			bWasRootMoving = false;
			Me->MoveHomeHere();
			UE_LOG(LogEternalReturn, Log, TEXT("[야생AI] %s 이동 스킬 착지 — 새 자리 %s"), *GetNameSafe(Me), *Me->GetHomeLocation().ToCompactString());
		}
	}
	// ⭐ 어그로 한계 — 자기 자리 기준 10m (사용자 2026-09-24).
	const float LeashUU = UERWildlifeSettings::Get().LeashMeters * 100.f;
	if (FVector::DistSquared2D(Me->GetActorLocation(), Me->GetHomeLocation()) > FMath::Square(LeashUU))
	{
		SetState(EERWildlifeAIState::Return, TEXT("자리에서 10m 초과"));
		return;
	}
	APawn* Target = PickTarget();
	if (!Target)
	{
		SetState(EERWildlifeAIState::Return, TEXT("어그로 팀에 생존자 없음"));
		return;
	}

	// 사거리 = 판정과 **같은 함수** — 시전자 몸 끝 → 대상 표면 (Argument 32). 다르면 붙었다 멈췄다를 반복한다.
	const UAbilitySystemComponent* ASC = Me->GetAbilitySystemComponent();
	const float RangeUU = ASC ? ASC->GetNumericAttribute(UERAttributeSet::GetAttackRangeAttribute()) * 100.f : 100.f;
	const float Dist = ERTargeting::SingleTargetDistance(Me, ERTargeting::GetTargetingLocation(Me), Target, /*bIgnoreZ=*/true);

	// 스킬 시전 · 돌진 중에는 쫓지 않는다 — 쫓으면 길찾기 이동 방향으로 몸이 돌아간다 (2026-09-30 PIE: 돌진을 피하자 멧돼지가 돌진하며 회전)
	if (const UAbilitySystemComponent* MyASC = Me->GetAbilitySystemComponent())
	{
		for (const FGameplayAbilitySpec& Spec : MyASC->GetActivatableAbilities())
		{
			if (Spec.IsActive() && !Spec.DynamicAbilityTags.HasTagExact(ERTags::Ability_Slot_Attack))
			{
				if (MoveTarget.IsValid())
				{
					StopMovement();
					MoveTarget.Reset();
				}
				return;
			}
		}
	}

	// 스킬이 평타보다 먼저 (F12.6-03 K1) — 쿨이 돌면 쓴다 (GAS 쿨다운이 거른다 · 사용자 2026-09-30)
	if (TrySkill(Target, Dist))
	{
		return;
	}

	if (Dist <= RangeUU)
	{
		if (MoveTarget.IsValid())
		{
			StopMovement();
			MoveTarget.Reset();
		}
		// [진단 · E31] AI 는 "안" 인데 어빌리티가 "밖" 으로 거부하는 경우 — 어빌리티는 발동 때 몸을 돌린 뒤 다시 잰다 (ResolveAim → 사전 확인).
		//   히트박스가 비대칭인 종(박쥐: 날개 좌우 72 · 앞 끝 6 · 캡슐 34)에서 난다. 평소엔 Verbose — `log LogEternalReturn Verbose` 로 본다.
		const UBoxComponent* Box = Me->FindComponentByClass<UBoxComponent>();
		UE_LOG(LogEternalReturn, Verbose, TEXT("[야생AI] %s 공격 시도 — 거리 %.1fcm · 사거리 %.0fcm · 몸 Yaw %.0f (회전 전) · 히트박스 중심 %s 반크기 %s · 캡슐 반경 %.0f"),
			*GetNameSafe(Me), Dist, RangeUU, Me->GetActorRotation().Yaw,
			Box ? *Box->GetRelativeLocation().ToCompactString() : TEXT("-"), Box ? *Box->GetUnscaledBoxExtent().ToCompactString() : TEXT("-"),
			Me->GetCapsuleComponent() ? Me->GetCapsuleComponent()->GetScaledCapsuleRadius() : -1.f);
		TryAttack(Target);
	}
	else if (MoveTarget.Get() != Target || GetMoveStatus() != EPathFollowingStatus::Moving)
	{
		// 사거리 밖 — 쫓는다. 멈추는 판단은 위에서 매 판단마다 (도착 반경은 작게).
		MoveToActor(Target, /*AcceptanceRadius=*/5.f, /*bStopOnOverlap=*/true, /*bUsePathfinding=*/true, /*bCanStrafe=*/false, nullptr, /*bAllowPartialPath=*/true);
		MoveTarget = Target;
	}
}

bool AERWildlifeAIController::TriggerSpec(const FGameplayAbilitySpec& Spec, AActor* Target)
{
	AERWildlifeCharacter* Me = GetWildlife();
	UAbilitySystemComponent* ASC = Me ? Me->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return false;
	}
	// ⭐ 플레이어와 같은 경로 — 조준 데이터를 실어 서버에서 발동 (AERPlayerController::ServerActivateSkill 과 같다). 쿨다운 · CC 면 거부된다.
	AActor* AimActor = Target ? Target : Me;
	const FHitResult Hit(AimActor, nullptr, AimActor->GetActorLocation(), FVector::UpVector);
	FGameplayEventData Payload;
	Payload.EventTag = ERTags::Event_Skill_Aim;
	Payload.Instigator = Me;
	Payload.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(Hit);
	return ASC->TriggerAbilityFromGameplayEvent(Spec.Handle, ASC->AbilityActorInfo.Get(), ERTags::Event_Skill_Aim, &Payload, *ASC);
}

bool AERWildlifeAIController::TrySkill(AActor* Target, float DistUU)
{
	AERWildlifeCharacter* Me = GetWildlife();
	UAbilitySystemComponent* ASC = Me ? Me->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return false;
	}
	// 무엇이든 시전 중이면 새로 시작하지 않는다 — 아래 평타 경로가 "기다림" 을 맡는다
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive())
		{
			return false;
		}
	}
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		const UERSkillData* Skill = Cast<UERSkillData>(Spec.SourceObject.Get());
		if (Skill && Skill->AIUse == EERAIUse::None && !Spec.DynamicAbilityTags.HasTagExact(ERTags::Ability_Slot_Attack))
		{
			// 동물에 붙인 스킬인데 AI 가 쓰지 않는다 — DA AIUse 를 빠뜨린 실수 (2026-10-01 위클라인 통제 한 번도 안 나옴). 스킬당 한 번
			static TSet<const UObject*> WarnedNone;
			if (!WarnedNone.Contains(Skill))
			{
				WarnedNone.Add(Skill);
				UE_LOG(LogEternalReturn, Warning, TEXT("[야생AI] %s 스킬 %s — AIUse None 이라 AI 가 쓰지 않는다 (DA AIUse 를 본다)"), *GetNameSafe(Me), *Skill->GetName());
			}
		}
		if (!Skill || Skill->AIUse != EERAIUse::InRange || Spec.Level <= 0)
		{
			continue;
		}
		const float RangeUU = (Skill->AIUseRange > 0.f ? Skill->AIUseRange : Skill->Shape.RangeMax) * 100.f;
		// 최소 거리 — 판정 모양의 RangeMin 을 그대로 (위클라인 통제 "2m 이상 떨어진 적에게" · F12.6-06)
		if (DistUU > RangeUU || DistUU < Skill->Shape.RangeMin * 100.f)
		{
			continue;
		}
		if (MoveTarget.IsValid())
		{
			StopMovement();   // 돌진 · 자기 이동 스킬이 길찾기와 싸우지 않게
			MoveTarget.Reset();
		}
		if (TriggerSpec(Spec, Target))
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[야생AI] %s 스킬 %s — 거리 %.1fm ≤ %.1fm"), *GetNameSafe(Me), *Skill->GetName(), DistUU / 100.f, RangeUU / 100.f);
			return true;
		}
		// 쿨다운 · CC — 다음 스킬 또는 평타
	}
	return false;
}

void AERWildlifeAIController::NotifyDealtHit()
{
	AERWildlifeCharacter* Me = GetWildlife();
	if (bStopped || State != EERWildlifeAIState::Combat || !Me || !Me->GetData() || !Me->GetData()->bRoams)
	{
		return;
	}
	Me->MoveHomeHere();
	UE_LOG(LogEternalReturn, Log, TEXT("[야생AI] %s 적중 — 새 자리 %s"), *GetNameSafe(Me), *Me->GetHomeLocation().ToCompactString());
}

void AERWildlifeAIController::NotifySteppedOnHazard(AActor* Victim)
{
	if (bStopped || (State != EERWildlifeAIState::Idle && State != EERWildlifeAIState::Tracking))
	{
		return;   // 이미 전투 · 귀환 — 먼저 때린 팀 우선 · 귀환 중엔 돌아서지 않는다 (§6.5)
	}
	const int32 Team = ERTeamStatics::GetTeamId(Victim);
	if (Team == INDEX_NONE)
	{
		return;
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[야생AI] %s 유해 물질을 %s 가 밟음 → 팀 %d 로 전투"), *GetNameSafe(GetPawn()), *GetNameSafe(Victim), Team);
	EnterCombat(Team, /*bPropagateToPack=*/true, TEXT("유해 물질 밟음"));
}

void AERWildlifeAIController::StartTrackingNearest(float Meters)
{
	const AERWildlifeCharacter* Me = GetWildlife();
	const AERGameState* GS = GetWorld() ? GetWorld()->GetGameState<AERGameState>() : nullptr;
	if (!Me || !GS || State != EERWildlifeAIState::Idle || Meters <= 0.f)
	{
		return;
	}
	APawn* Nearest = nullptr;
	float BestSq = FMath::Square(Meters * 100.f);
	for (const APlayerState* PS : GS->PlayerArray)
	{
		APawn* Candidate = PS ? PS->GetPawn() : nullptr;
		const UAbilitySystemComponent* ASC = Candidate ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Candidate) : nullptr;
		if (!ASC || ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute()) <= 0.f)
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(Candidate->GetActorLocation(), Me->GetActorLocation());
		if (DistSq <= BestSq)
		{
			BestSq = DistSq;
			Nearest = Candidate;
		}
	}
	if (!Nearest)
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[야생AI] %s 생성 — %.0fm 안 실험체 없음 · 추적 안 함 (⏸ 순찰은 F13)"), *Me->GetName(), Meters);
		return;
	}
	TrackTarget = Nearest;
	TrackRadiusUU = Meters * 100.f;
	UE_LOG(LogEternalReturn, Log, TEXT("[야생AI] %s 실험 대상 추적 — %s (%.1fm · 반경 %.0fm)"), *Me->GetName(), *Nearest->GetName(), FMath::Sqrt(BestSq) / 100.f, Meters);
	SetState(EERWildlifeAIState::Tracking, TEXT("실험 대상 추적"));
}

void AERWildlifeAIController::ThinkTracking()
{
	AERWildlifeCharacter* Me = GetWildlife();
	if (bStopped || !Me || Me->IsDead() || State != EERWildlifeAIState::Tracking)
	{
		return;
	}
	TryWhileMovingSkills();
	// ⚠ 방금 흘린 독이 바로 펄스를 돌아 밟은 사람이 있으면 그 안에서 전투로 바뀐다 (TrackTarget 도 비워진다) — 이어 가면 "반경 밖" 으로 대기가 된다 (2026-10-01 로그 21:20:39)
	if (State != EERWildlifeAIState::Tracking)
	{
		return;
	}
	APawn* Target = TrackTarget.Get();
	const UAbilitySystemComponent* TASC = Target ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target) : nullptr;
	if (!TASC || TASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute()) <= 0.f
		|| FVector::DistSquared2D(Target->GetActorLocation(), Me->GetActorLocation()) > FMath::Square(TrackRadiusUU))
	{
		TrackTarget.Reset();
		SetState(EERWildlifeAIState::Idle, TEXT("추적 대상이 반경 밖 (⏸ 순찰은 F13)"));
		return;
	}
	// 다가가기만 — 붙으면 멈춘다 (공격 안 함 · 먼저 때리거나 독을 밟아야 전투)
	if (MoveTarget.Get() != Target || GetMoveStatus() != EPathFollowingStatus::Moving)
	{
		MoveToActor(Target, /*AcceptanceRadius=*/150.f, /*bStopOnOverlap=*/true, /*bUsePathfinding=*/true, /*bCanStrafe=*/false, nullptr, /*bAllowPartialPath=*/true);
		MoveTarget = Target;
	}
}

void AERWildlifeAIController::TryWhileMovingSkills()
{
	AERWildlifeCharacter* Me = GetWildlife();
	UAbilitySystemComponent* ASC = Me ? Me->GetAbilitySystemComponent() : nullptr;
	if (!ASC || Me->GetVelocity().SizeSquared2D() < FMath::Square(10.f))
	{
		return;   // 서 있으면 안 흘린다
	}
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		const UERSkillData* Skill = Cast<UERSkillData>(Spec.SourceObject.Get());
		if (Skill && Skill->AIUse == EERAIUse::WhileMoving && Spec.Level > 0 && !Spec.IsActive())
		{
			if (TriggerSpec(Spec, nullptr))   // 자기 자리 · 쿨이면 거부 (조용히)
			{
				UE_LOG(LogEternalReturn, Verbose, TEXT("[야생AI] %s 움직이며 %s"), *Me->GetName(), *Skill->GetName());
			}
		}
	}
}

void AERWildlifeAIController::NotifyAllyDied()
{
	AERWildlifeCharacter* Me = GetWildlife();
	UAbilitySystemComponent* ASC = Me ? Me->GetAbilitySystemComponent() : nullptr;
	if (bStopped || !ASC || Me->IsDead())
	{
		return;
	}
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		const UERSkillData* Skill = Cast<UERSkillData>(Spec.SourceObject.Get());
		if (Skill && Skill->AIUse == EERAIUse::OnAllyDeath && Spec.Level > 0)
		{
			const bool bOk = TriggerSpec(Spec, nullptr);
			UE_LOG(LogEternalReturn, Log, TEXT("[야생AI] %s 짝이 죽음 → 스킬 %s %s"), *GetNameSafe(Me), *Skill->GetName(), bOk ? TEXT("발동") : TEXT("거부 (쿨 · CC)"));
		}
	}
}

void AERWildlifeAIController::TryAttack(AActor* Target)
{
	AERWildlifeCharacter* Me = GetWildlife();
	UAbilitySystemComponent* ASC = Me ? Me->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}
	// 스킬 시전 중이면 평타를 겹쳐 치지 않는다 (F12.6-03 PIE: 선딜 1초 중에 평타가 나갔다 — 평타는 State.Casting 에 안 막힌다)
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive() && !Spec.DynamicAbilityTags.HasTagExact(ERTags::Ability_Slot_Attack))
		{
			return;
		}
	}
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.Ability || !Spec.DynamicAbilityTags.HasTagExact(ERTags::Ability_Slot_Attack))
		{
			continue;
		}
		if (Spec.IsActive())
		{
			return;   // 앞 공격이 아직 끝나지 않았다 (선후딜)
		}
		TriggerSpec(Spec, Target);
		return;
	}
	if (!bWarnedNoAttack)
	{
		bWarnedNoAttack = true;
		UE_LOG(LogEternalReturn, Warning, TEXT("[야생AI] %s (%s) 에 평타 슬롯(Ability.Slot.Attack) 스킬이 없다 — Project Settings > ER Wildlife > DefaultAttackSkill 을 지정하라. 쫓기만 한다."),
			*Me->GetName(), *GetNameSafe(Me->GetData()));
	}
}
