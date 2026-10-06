// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ERWilson.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "EternalReturn.h"
#include "Combat/ERForcedMove.h"
#include "GameFramework/Character.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"
#include "Presentation/ERPresentationComponent.h"
#include "TimerManager.h"

AERWilson::AERWilson()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

AERWilson* AERWilson::Drop(ACharacter* Sissela, const FVector& Location, TSubclassOf<AERWilson> Class)
{
	if (!Sissela || !Sissela->HasAuthority())
	{
		return nullptr;
	}
	if (AERWilson* Existing = FindFor(Sissela))
	{
		Existing->SetActorLocation(Location);
		Existing->SetInFlight(false);
		UE_LOG(LogEternalReturn, Log, TEXT("[윌슨] %s 이미 떨어져 있음 — 옮김 %s%s"), *GetNameSafe(Sissela), *Location.ToCompactString(),
			Existing->bPullingOwner ? TEXT(" · 끌려가는 중 → 새 자리로 다시") : TEXT(""));
		if (Existing->bPullingOwner)
		{
			Existing->PullOwner(Existing->PullSpeedMps);
		}
		return Existing;
	}
	FActorSpawnParameters Params;
	Params.Owner = Sissela;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* SpawnClass = Class ? Class.Get() : AERWilson::StaticClass();
	AERWilson* Wilson = Sissela->GetWorld()->SpawnActor<AERWilson>(SpawnClass, Location, Sissela->GetActorRotation(), Params);
	if (Wilson)
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[윌슨] %s 떨어짐 %s (%s · 줍기 %.1fm · 복귀 %.2fm)"),
			*GetNameSafe(Sissela), *Location.ToCompactString(), *GetNameSafe(SpawnClass), Wilson->PickupRadius, Wilson->ReturnRange);
	}
	return Wilson;
}

AERWilson* AERWilson::FindFor(const AActor* Sissela)
{
	if (!Sissela || !Sissela->GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<AERWilson> It(Sissela->GetWorld()); It; ++It)
	{
		if (It->GetOwner() == Sissela && !It->bJoined && IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

void AERWilson::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		SetAwayTag(true);
		GetWorldTimerManager().SetTimer(CheckTimer, this, &AERWilson::CheckDistance, 0.1f, true);
	}
}

void AERWilson::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 어떤 길로 사라져도 (시셀라 사망 · 레벨 끝) 태그는 걷는다
	if (HasAuthority())
	{
		SetAwayTag(false);
	}
	Super::EndPlay(EndPlayReason);
}

void AERWilson::SetAwayTag(bool bAway)
{
	if (bAway == bAwayTagged)
	{
		return;
	}
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (!ASC)
	{
		return;
	}
	// ⚠ 복제 루즈 태그는 서버 자기 태그맵을 안 바꾼다 (FMinimalReplicationTagCountMap::AddTag) — 둘 다 건다
	if (bAway)
	{
		ASC->AddLooseGameplayTag(ERTags::State_WilsonAway);
		ASC->AddReplicatedLooseGameplayTag(ERTags::State_WilsonAway);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(ERTags::State_WilsonAway);
		ASC->RemoveReplicatedLooseGameplayTag(ERTags::State_WilsonAway);
	}
	bAwayTagged = bAway;
}

void AERWilson::CheckDistance()
{
	const ACharacter* Sissela = Cast<ACharacter>(GetOwner());
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Sissela);
	if (!Sissela || !ASC || ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute()) <= 0.f)
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[윌슨] 주인 없음 · 사망 — 사라짐"));
		Destroy();
		return;
	}
	if (bPullingOwner || bFlying)
	{
		return;   // 끌려오는 중 — 합침은 도착 타이머가 ("E") · Q 로 날아가는 중 — 착지가 풀어 준다
	}
	const float Dist = FVector::Dist2D(Sissela->GetActorLocation(), GetActorLocation()) / 100.f;
	static const IConsoleVariable* CVarDraw = IConsoleManager::Get().FindConsoleVariable(TEXT("ER.Skill.DebugDraw"));
	if (CVarDraw && CVarDraw->GetInt() > 0)
	{
		DrawDebugSphere(GetWorld(), GetActorLocation(), 40.f, 12, FColor::Orange, false, 0.12f);
		DrawDebugCircle(GetWorld(), GetActorLocation(), PickupRadius * 100.f, 32, FColor::Orange, false, 0.12f, 0, 2.f, FVector::YAxisVector, FVector::XAxisVector, false);
	}
	if (Dist <= PickupRadius)
	{
		Join(TEXT("줍기"));
	}
	else if (Dist > ReturnRange)
	{
		Join(*FString::Printf(TEXT("거리 %.1fm"), Dist));
	}
}

void AERWilson::PullOwner(float SpeedMps)
{
	ACharacter* Sissela = Cast<ACharacter>(GetOwner());
	if (!HasAuthority() || !Sissela || bJoined)
	{
		return;
	}
	PullSpeedMps = FMath::Max(SpeedMps, 1.f);
	bPullingOwner = true;
	GetWorldTimerManager().ClearTimer(PullTimer);
	FVector To = GetActorLocation() - Sissela->GetActorLocation();
	To.Z = 0.f;
	const float DistUU = To.Size();
	const float Duration = DistUU / (PullSpeedMps * 100.f);
	if (DistUU > 10.f)
	{
		ERForcedMove::ApplySelfMove(Sissela, To, DistUU, Duration, false, 0.f, /*bThroughWalls=*/true);   // E 는 벽을 넘는다 (사용자 2026-10-06)   // 앞 끌기는 각 머신의 AddForcedMoveSource 가 지우고 새로 (ERForcedMove.cpp:213)
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[윌슨] %s 끌려감 → 윌슨 %.1fm · %.2f초"), *GetNameSafe(Sissela), DistUU / 100.f, Duration);
	GetWorldTimerManager().SetTimer(PullTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { Join(TEXT("E")); }), FMath::Max(Duration, 0.01f), false);
}

void AERWilson::SetInFlight(bool bInFlight)
{
	if (bFlying != bInFlight)
	{
		bFlying = bInFlight;
		SetActorHiddenInGame(bInFlight);
	}
}

void AERWilson::HoldPullForThrow()
{
	if (bPullingOwner)
	{
		GetWorldTimerManager().ClearTimer(PullTimer);
		UE_LOG(LogEternalReturn, Log, TEXT("[윌슨] %s 끌려가는 중 Q — 착지 자리까지 다시 끈다"), *GetNameSafe(GetOwner()));
	}
}

void AERWilson::Join(const TCHAR* Reason)
{
	if (!HasAuthority() || bJoined)
	{
		return;
	}
	bJoined = true;
	bPullingOwner = false;
	GetWorldTimerManager().ClearTimer(CheckTimer);
	GetWorldTimerManager().ClearTimer(PullTimer);
	SetAwayTag(false);
	AActor* Sissela = GetOwner();
	UE_LOG(LogEternalReturn, Log, TEXT("[윌슨] %s 합침 — %s"), *GetNameSafe(Sissela), Reason);
	if (Sissela)
	{
		UERPresentationComponent::SendSfxCue(Sissela, ERTags::Pres_Sfx_Join, Sissela->GetActorLocation());   // Passive_Union
	}
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Sissela))
	{
		FGameplayEventData Payload;
		Payload.EventTag = ERTags::Event_Wilson_Joined;
		Payload.Instigator = Sissela;
		Payload.Target = Sissela;
		ASC->HandleGameplayEvent(ERTags::Event_Wilson_Joined, &Payload);
	}
	Destroy();
}
