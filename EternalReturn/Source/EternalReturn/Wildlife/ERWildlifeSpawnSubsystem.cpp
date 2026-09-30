// Copyright Epic Games, Inc. All Rights Reserved.

#include "Wildlife/ERWildlifeSpawnSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "GAS/ERAttributeSet.h"
#include "Core/ERGameState.h"
#include "Core/ERPlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EternalReturn.h"
#include "Growth/ERGrowthComponent.h"
#include "TimerManager.h"
#include "Wildlife/ERWildlifeCharacter.h"
#include "Wildlife/ERWildlifeData.h"
#include "Wildlife/ERWildlifeSettings.h"
#include "Wildlife/ERWildlifeSpawnPoint.h"

namespace
{
	/** 슬롯을 훑는 주기 (초). 원작 시각이 초 단위라 1초면 충분하다. */
	constexpr float PollInterval = 1.f;

	const TCHAR* RespawnName(EERWildlifeRespawn Mode)
	{
		switch (Mode)
		{
		case EERWildlifeRespawn::Interval:   return TEXT("주기");
		case EERWildlifeRespawn::EveryDay:   return TEXT("낮마다");
		case EERWildlifeRespawn::EveryNight: return TEXT("밤마다");
		case EERWildlifeRespawn::Never:      return TEXT("없음");
		}
		return TEXT("?");
	}
}

bool UERWildlifeSpawnSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UERWildlifeSpawnSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;   // ⭐ 서버 전용 — 클라는 스폰된 액터만 받는다
	}

	for (TActorIterator<AERWildlifeSpawnPoint> It(&InWorld); It; ++It)
	{
		AERWildlifeSpawnPoint* Point = *It;
		if (!Point->Species)
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[스폰] %s 에 종 정의(Species)가 비어 있다 — 건너뜀."), *Point->GetName());
			continue;
		}
		for (int32 i = 0; i < Point->Count; ++i)
		{
			FSlot& Slot = Slots.AddDefaulted_GetRef();
			Slot.Point = Point;
			Slot.Index = i;
		}
	}
	if (!Slots.IsEmpty())
	{
		InWorld.GetTimerManager().SetTimer(PollTimer, this, &UERWildlifeSpawnSubsystem::Poll, PollInterval, true);
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[스폰] 스폰 자리 %d칸 등록 (서버)"), Slots.Num());
}

void UERWildlifeSpawnSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PollTimer);
		World->GetTimerManager().ClearTimer(SenseTimer);
	}
	Slots.Reset();
	SensedAnimals.Reset();
	Super::Deinitialize();
}

void UERWildlifeSpawnSubsystem::RegisterAnimal(AERWildlifeCharacter* Animal)
{
	UWorld* World = GetWorld();
	if (!Animal || !World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	SensedAnimals.AddUnique(Animal);
	if (!SenseTimer.IsValid())
	{
		World->GetTimerManager().SetTimer(SenseTimer, this, &UERWildlifeSpawnSubsystem::Sense, UERWildlifeSettings::Get().SenseInterval, true);
	}
}

void UERWildlifeSpawnSubsystem::UnregisterAnimal(AERWildlifeCharacter* Animal)
{
	SensedAnimals.Remove(Animal);
}

void UERWildlifeSpawnSubsystem::Sense()
{
	UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}
	// 살아 있는 실험체 위치 — 한 번만 모은다 (≤24)
	TArray<FVector, TInlineAllocator<24>> Players;
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
		const UAbilitySystemComponent* ASC = Pawn ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn) : nullptr;
		if (ASC && ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute()) > 0.f)
		{
			Players.Add(Pawn->GetActorLocation());
		}
	}
	const double Now = World->GetTimeSeconds();
	SensedAnimals.RemoveAll([](const TWeakObjectPtr<AERWildlifeCharacter>& W) { return !W.IsValid(); });
	for (const TWeakObjectPtr<AERWildlifeCharacter>& W : SensedAnimals)
	{
		AERWildlifeCharacter* Animal = W.Get();
		float BestSq = TNumericLimits<float>::Max();
		for (const FVector& P : Players)
		{
			BestSq = FMath::Min(BestSq, static_cast<float>(FVector::DistSquared2D(P, Animal->GetActorLocation())));
		}
		Animal->SenseNearby(Players.IsEmpty() ? TNumericLimits<float>::Max() : FMath::Sqrt(BestSq) / 100.f, Now);
	}
}

bool UERWildlifeSpawnSubsystem::IsFirstSpawnDue(const FSlot& Slot, const AERGameState& GS) const
{
	const UERWildlifeData* D = Slot.Point->Species;
	const int32 Target = AERGameState::ToPhaseIndex(D->FirstSpawnDay, D->bFirstSpawnNight);
	const int32 Current = GS.GetPhaseIndex();
	// 원작 표기는 **카운트다운** — "1일차 01:00" = 1일차 낮 타이머가 1:00 남았을 때 (사용자 2026-09-23).
	return Current > Target || (Current == Target && GS.GetPhaseRemaining() <= D->FirstSpawnTimer);
}

void UERWildlifeSpawnSubsystem::Poll()
{
	const UWorld* World = GetWorld();
	const AERGameState* GS = World ? World->GetGameState<AERGameState>() : nullptr;
	if (!GS || !GS->IsClockRunning())
	{
		return;
	}
	const double Now = GS->GetServerWorldTimeSeconds();
	const int32 Phase = GS->GetPhaseIndex();

	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FSlot& Slot = Slots[i];
		if (!Slot.Point.IsValid() || !Slot.Point->Species)
		{
			continue;
		}
		if (Slot.Animal.IsValid() && !Slot.Animal->IsDead())
		{
			continue;   // 살아 있다
		}
		if (!Slot.bSpawnedOnce)
		{
			if (IsFirstSpawnDue(Slot, *GS))
			{
				SpawnSlot(i);
			}
		}
		else if ((Slot.RespawnAt >= 0.0 && Now >= Slot.RespawnAt) || (Slot.RespawnPhase >= 0 && Phase >= Slot.RespawnPhase))
		{
			SpawnSlot(i);
		}
	}
}

int32 UERWildlifeSpawnSubsystem::GetIslandMaxPlayerLevel() const
{
	int32 MaxLevel = 1;
	const AERGameState* GS = GetWorld() ? GetWorld()->GetGameState<AERGameState>() : nullptr;
	if (GS)
	{
		for (const APlayerState* PS : GS->PlayerArray)
		{
			const AERPlayerState* ERPS = Cast<AERPlayerState>(PS);
			if (const UERGrowthComponent* Growth = ERPS ? ERPS->GetGrowth() : nullptr)
			{
				MaxLevel = FMath::Max(MaxLevel, Growth->GetLevel());
			}
		}
	}
	return MaxLevel;
}

void UERWildlifeSpawnSubsystem::SpawnSlot(int32 SlotIndex)
{
	FSlot& Slot = Slots[SlotIndex];
	AERWildlifeSpawnPoint* Point = Slot.Point.Get();
	UWorld* World = GetWorld();
	const UERWildlifeSettings& S = UERWildlifeSettings::Get();
	UClass* Class = S.WildlifeClass.IsNull() ? nullptr : S.WildlifeClass.LoadSynchronous();
	if (!Point || !World || !Class)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[스폰] WildlifeClass 가 비어 있다 (Project Settings > ER Wildlife)."));
		return;
	}
	// ⭐ 변이 — 첫 스폰은 항상 일반, **재생성할 때마다** 확률로 변이 (나무위키 · F12-06). 재생성 시각 규칙은 자리의 일반 종 것.
	const UERWildlifeData* Data = Point->Species;
	if (Slot.bSpawnedOnce && Point->MutantSpecies && FMath::FRand() < S.MutantChance)
	{
		Data = Point->MutantSpecies;
	}

	// ⭐ 레벨은 **스폰 시점**에 정한다 — "같은 자리에 레벨이 오른 채 다시 스폰" (사용자 2026-09-23). 공식은 [자체].
	const int32 Level = FMath::Max(Data->BaseLevel, GetIslandMaxPlayerLevel());

	// 자리는 바닥에 놓는다 → 캡슐 중심이 바닥 위에 오게 반높이만큼 올린다.
	float HalfHeight = 0.f;
	if (const ACharacter* CDO = Class->GetDefaultObject<ACharacter>())
	{
		HalfHeight = CDO->GetCapsuleComponent() ? CDO->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FVector Loc = Point->GetSpawnLocation(Slot.Index) + FVector(0.f, 0.f, HalfHeight);
	AERWildlifeCharacter* Animal = World->SpawnActor<AERWildlifeCharacter>(Class, Loc, Point->GetActorRotation(), Params);
	if (!Animal || !Animal->Initialize(Data, Level))
	{
		if (Animal)
		{
			Animal->Destroy();
		}
		return;
	}

	const bool bFirst = !Slot.bSpawnedOnce;
	Animal->SetHome(Point, Animal->GetActorLocation());   // 귀환 지점 = 자기가 스폰된 자리 (무리는 흩어진 각자 자리)
	Slot.Animal = Animal;
	Slot.bSpawnedOnce = true;
	Slot.RespawnAt = -1.0;
	Slot.RespawnPhase = -1;
	Animal->OnWildlifeKilled.AddWeakLambda(this, [this, SlotIndex](AERWildlifeCharacter*, AActor*) { OnAnimalKilled(SlotIndex); });

	const AERGameState* GS = World->GetGameState<AERGameState>();
	UE_LOG(LogEternalReturn, Log, TEXT("[스폰] %s #%d <- %s Lv.%d (%s) · %d일차 %s 남은 %.0f초"),
		*Point->GetName(), Slot.Index, *Data->GetName(), Level, bFirst ? TEXT("최초") : TEXT("재생성"),
		GS ? GS->GetDay() : 0, GS && GS->IsNight() ? TEXT("밤") : TEXT("낮"), GS ? GS->GetPhaseRemaining() : 0.f);
}

void UERWildlifeSpawnSubsystem::OnAnimalKilled(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex) || !Slots[SlotIndex].Point.IsValid())
	{
		return;
	}
	FSlot& Slot = Slots[SlotIndex];
	const UERWildlifeData* Data = Slot.Point->Species;
	const AERGameState* GS = GetWorld() ? GetWorld()->GetGameState<AERGameState>() : nullptr;
	if (!Data || !GS)
	{
		return;
	}
	const int32 Phase = GS->GetPhaseIndex();

	// ⭐ 무리 동시 리젠 (F12-06) — 한 마리라도 살아 있으면 타이머를 걸지 않는다. 마지막 한 마리가 죽은 순간 **전원**에게 같은 일정.
	const AERWildlifeSpawnPoint* Point = Slot.Point.Get();
	if (Point->bPackRespawn)
	{
		for (const FSlot& Other : Slots)
		{
			if (Other.Point.Get() == Point && Other.Animal.IsValid() && !Other.Animal->IsDead())
			{
				UE_LOG(LogEternalReturn, Log, TEXT("[스폰] %s #%d 처치 — 무리가 남아 있어 대기 (전부 잡혀야 리젠)"), *Point->GetName(), Slot.Index);
				return;
			}
		}
	}

	// ⭐ 기준점은 **처치 순간** — 무리면 마지막 한 마리의 처치 순간.
	double RespawnAt = -1.0;
	int32 RespawnPhase = -1;
	switch (Data->RespawnMode)
	{
	case EERWildlifeRespawn::Interval:
		RespawnAt = GS->GetServerWorldTimeSeconds() + Data->RespawnSeconds;
		break;
	case EERWildlifeRespawn::EveryDay:     // 다음 낮 = 짝수 번호
		RespawnPhase = (Phase % 2 == 0) ? Phase + 2 : Phase + 1;
		break;
	case EERWildlifeRespawn::EveryNight:   // 다음 밤 = 홀수 번호
		RespawnPhase = (Phase % 2 == 1) ? Phase + 2 : Phase + 1;
		break;
	case EERWildlifeRespawn::Never:
		break;
	}
	for (FSlot& Target : Slots)
	{
		if (Target.Point.Get() == Point && (Point->bPackRespawn || &Target == &Slot))
		{
			Target.RespawnAt = RespawnAt;
			Target.RespawnPhase = RespawnPhase;
		}
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[스폰] %s #%d 처치 — 재생성 %s%s"),
		*Slot.Point->GetName(), Slot.Index, RespawnName(Data->RespawnMode),
		Data->RespawnMode == EERWildlifeRespawn::Interval ? *FString::Printf(TEXT(" %.0f초 뒤"), Data->RespawnSeconds)
		: Slot.RespawnPhase >= 0 ? *FString::Printf(TEXT(" (%d일차 %s)"), Slot.RespawnPhase / 2 + 1, Slot.RespawnPhase % 2 ? TEXT("밤") : TEXT("낮")) : TEXT(""));
}

void UERWildlifeSpawnSubsystem::GetPackMates(const AERWildlifeCharacter* Animal, TArray<AERWildlifeCharacter*>& Out) const
{
	const AERWildlifeSpawnPoint* Point = Animal ? Animal->GetHomePoint() : nullptr;
	if (!Point)
	{
		return;   // 디버그 스폰 — 무리 없음
	}
	for (const FSlot& Slot : Slots)
	{
		AERWildlifeCharacter* Other = Slot.Animal.Get();
		if (Slot.Point.Get() == Point && Other && Other != Animal && !Other->IsDead())
		{
			Out.Add(Other);
		}
	}
}

void UERWildlifeSpawnSubsystem::DumpSlots() const
{
	const AERGameState* GS = GetWorld() ? GetWorld()->GetGameState<AERGameState>() : nullptr;
	const double Now = GS ? GS->GetServerWorldTimeSeconds() : 0.0;
	UE_LOG(LogEternalReturn, Warning, TEXT("[스폰] 자리 %d칸 · 섬 최고 실험체 Lv.%d · %d일차 %s 남은 %.0f초"),
		Slots.Num(), GetIslandMaxPlayerLevel(), GS ? GS->GetDay() : 0, GS && GS->IsNight() ? TEXT("밤") : TEXT("낮"), GS ? GS->GetPhaseRemaining() : 0.f);
	for (const FSlot& Slot : Slots)
	{
		const AERWildlifeSpawnPoint* P = Slot.Point.Get();
		const UERWildlifeData* D = P ? P->Species.Get() : nullptr;
		FString State;
		if (Slot.Animal.IsValid() && !Slot.Animal->IsDead()) { State = FString::Printf(TEXT("살아 있음 Lv.%d"), Slot.Animal->GetLevel()); }
		else if (!Slot.bSpawnedOnce) { State = D ? FString::Printf(TEXT("최초 대기 (%d일차 %s %.0f)"), D->FirstSpawnDay, D->bFirstSpawnNight ? TEXT("밤") : TEXT("낮"), D->FirstSpawnTimer) : TEXT("최초 대기"); }
		else if (Slot.RespawnAt >= 0.0) { State = FString::Printf(TEXT("재생성 %.0f초 뒤"), Slot.RespawnAt - Now); }
		else if (Slot.RespawnPhase >= 0) { State = FString::Printf(TEXT("재생성 %d일차 %s"), Slot.RespawnPhase / 2 + 1, Slot.RespawnPhase % 2 ? TEXT("밤") : TEXT("낮")); }
		else { State = TEXT("없음"); }
		UE_LOG(LogEternalReturn, Warning, TEXT("  %s #%d  %s  — %s"), *GetNameSafe(P), Slot.Index, *GetNameSafe(D), *State);
	}
}
