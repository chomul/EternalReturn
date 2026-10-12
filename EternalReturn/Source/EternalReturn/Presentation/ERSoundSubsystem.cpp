// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/ERSoundSubsystem.h"
#include "Engine/World.h"
#include "EternalReturn.h"
#include "Kismet/GameplayStatics.h"
#include "Presentation/ERSoundSettings.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

bool UERSoundSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UERSoundSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_DedicatedServer)
	{
		return;   // 데디 서버는 소리를 안 낸다
	}
	int32 Total = 0;
	for (const TPair<FGameplayTag, const FERCommonSound*>& P : UERSoundSettings::Get().GetAll())
	{
		FERLoadedCommonSound L;
		L.Count = FMath::Max(1, P.Value->Count);
		L.Interval = FMath::Max(0.f, P.Value->Interval);
		for (const TSoftObjectPtr<USoundBase>& S : P.Value->Sounds)
		{
			if (USoundBase* Sound = S.LoadSynchronous())
			{
				L.Sounds.Add(Sound);
			}
			else if (!S.IsNull())
			{
				// GAS 처럼 조용히 건너뛰지 않는다 — 경로가 틀렸으면 알린다
				UE_LOG(LogEternalReturn, Warning, TEXT("[소리] ER Sound %s — 애셋이 없다: %s"), *P.Key.ToString(), *S.ToString());
			}
		}
		if (!L.Sounds.IsEmpty())
		{
			Total += L.Sounds.Num();
			Loaded.Add(P.Key, MoveTemp(L));
		}
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[소리] 공용 소리 %d 상황 · %d 개 로드 (ER Sound)"), Loaded.Num(), Total);
}

UERSoundSubsystem* UERSoundSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UERSoundSubsystem>() : nullptr;
}

USoundBase* UERSoundSubsystem::Pick(FGameplayTag Key) const
{
	const FERLoadedCommonSound* L = Loaded.Find(Key);
	return L ? L->Sounds[FMath::RandRange(0, L->Sounds.Num() - 1)].Get() : nullptr;
}

USoundBase* UERSoundSubsystem::PlayAt(FGameplayTag Key, const FVector& Location)
{
	return Play(Key, &Location);
}

USoundBase* UERSoundSubsystem::Play2D(FGameplayTag Key)
{
	return Play(Key, nullptr);
}

USoundBase* UERSoundSubsystem::Play(FGameplayTag Key, const FVector* Location)
{
	USoundBase* First = Pick(Key);
	if (!First)
	{
		return nullptr;
	}
	const TOptional<FVector> At = Location ? TOptional<FVector>(*Location) : TOptional<FVector>();
	auto PlayOne = [this, At](USoundBase* Sound)
	{
		if (At.IsSet()) { UGameplayStatics::PlaySoundAtLocation(this, Sound, At.GetValue()); }
		else { UGameplayStatics::PlaySound2D(this, Sound); }
	};
	PlayOne(First);
	// 이어서 여러 번 — 매번 다시 무작위 · 같은 자리
	const FERLoadedCommonSound& L = Loaded.FindChecked(Key);
	for (int32 i = 1; i < L.Count; ++i)
	{
		if (L.Interval <= 0.f)
		{
			if (USoundBase* Next = Pick(Key)) { PlayOne(Next); }   // 타이머는 0 초면 안 돈다 — 겹쳐서 바로
			continue;
		}
		FTimerHandle Unused;
		GetWorld()->GetTimerManager().SetTimer(Unused, FTimerDelegate::CreateWeakLambda(this, [this, Key, PlayOne]()
		{
			if (USoundBase* Next = Pick(Key)) { PlayOne(Next); }
		}), L.Interval * i, false);
	}
	return First;
}
