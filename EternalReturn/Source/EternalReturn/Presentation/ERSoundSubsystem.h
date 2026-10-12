// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "ERSoundSubsystem.generated.h"

class USoundBase;

/** 한 상황의 로드된 소리 (GC 가 붙잡게 UPROPERTY). */
USTRUCT()
struct FERLoadedCommonSound
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> Sounds;

	int32 Count = 1;
	float Interval = 0.f;
};

/**
 * 공용 소리 재생 (Argument 78 S1). 표는 UERSoundSettings (ER Sound).
 *
 * ⭐ 보내는 길은 그대로 (N1) — 주변 = 소리 큐 GameplayCue.Pres.Sfx · 본인 · 팀 = AERPlayerState::ClientPlaySfx.
 *   받는 쪽이 캐릭터 연출 DA 에 키가 없으면 여기서 찾는다.
 * 데디 서버는 소리를 안 내므로 로드도 안 한다. 월드 시작 때 표를 한 번 로드한다 (작은 표).
 */
UCLASS()
class ETERNALRETURN_API UERSoundSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	static UERSoundSubsystem* Get(const UObject* WorldContext);

	bool HasKey(FGameplayTag Key) const { return Loaded.Contains(Key); }

	/** 자리에서 (주변). Count > 1 이면 Interval 마다 다시 골라 이어서. 처음 튼 소리 (로그용) · 없으면 nullptr */
	USoundBase* PlayAt(FGameplayTag Key, const FVector& Location);

	/** 본인 화면 2D. 이어서 여러 번은 PlayAt 과 같다 */
	USoundBase* Play2D(FGameplayTag Key);

private:
	USoundBase* Pick(FGameplayTag Key) const;
	USoundBase* Play(FGameplayTag Key, const FVector* Location);

	UPROPERTY()
	TMap<FGameplayTag, FERLoadedCommonSound> Loaded;
};
