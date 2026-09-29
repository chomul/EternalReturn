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
#include "EternalReturn.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"
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
	// ⭐ 전투 중 다른 팀이 때려도 **먼저 때린 팀 우선** · 귀환 중에는 **돌아서지 않는다** (사용자 2026-09-24).
	if (State != EERWildlifeAIState::Idle)
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
	if (bStopped || !Me || Me->IsDead() || State != EERWildlifeAIState::Idle)
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
		GetWorldTimerManager().SetTimer(ThinkTimer, this, &AERWildlifeAIController::Think, S.ThinkInterval, true);
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
			// 이미 자리에 있거나 길이 없다 — 바로 도착 처리.
			Heal();
			SetState(EERWildlifeAIState::Idle, TEXT("귀환 즉시 완료"));
			return;
		}
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
	if (bStopped || !Me || Me->IsDead() || State != EERWildlifeAIState::Combat)
	{
		return;
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

void AERWildlifeAIController::TryAttack(AActor* Target)
{
	AERWildlifeCharacter* Me = GetWildlife();
	UAbilitySystemComponent* ASC = Me ? Me->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
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
		// ⭐ 플레이어와 같은 경로 — 조준 데이터를 실어 서버에서 발동 (AERPlayerController::ServerActivateSkill 과 같다). 쿨다운 중이면 거부된다.
		const FHitResult Hit(Target, nullptr, Target->GetActorLocation(), FVector::UpVector);
		FGameplayEventData Payload;
		Payload.EventTag = ERTags::Event_Skill_Aim;
		Payload.Instigator = Me;
		Payload.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(Hit);
		ASC->TriggerAbilityFromGameplayEvent(Spec.Handle, ASC->AbilityActorInfo.Get(), ERTags::Event_Skill_Aim, &Payload, *ASC);
		return;
	}
	if (!bWarnedNoAttack)
	{
		bWarnedNoAttack = true;
		UE_LOG(LogEternalReturn, Warning, TEXT("[야생AI] %s (%s) 에 평타 슬롯(Ability.Slot.Attack) 스킬이 없다 — Project Settings > ER Wildlife > DefaultAttackSkill 을 지정하라. 쫓기만 한다."),
			*Me->GetName(), *GetNameSafe(Me->GetData()));
	}
}
