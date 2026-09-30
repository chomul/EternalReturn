// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ERSkillAreaActor.generated.h"

class UERGameplayAbility;
class UERSkillData;

/**
 * 스킬 장판 (F11-05 C 암기 마름쇠). 서버가 조준점에 스폰. AreaTickInterval 마다 반경 안의 적을 F04 로 찾아 시전 어빌리티의 ApplySkillDamage 를 빌려 피해 · 적중 효과.
 * ⭐ 계산 경로가 하나다 — 장판이 피해를 직접 계산하지 않는다. 어빌리티 인스턴스(InstancedPerActor)는 살아 있으니 약참조로 빌린다.
 * 같은 대상은 맞을 때마다 (1 − AreaDamageDecay)^n 으로 감쇠 (원문 "밟을 때마다 45% 감소", 둔화는 불변). 수명 뒤 Destroy.
 * 복제: 위치만 (F17 연출용). 판정 · 피해는 서버.
 */
UCLASS()
class ETERNALRETURN_API AERSkillAreaActor : public AActor
{
	GENERATED_BODY()

public:
	AERSkillAreaActor();

	/** [서버] 스폰 직후 한 번. 첫 판정은 즉시. */
	/** 장판 조각이 반경 · 주기 · 감쇠를 넘긴다. 펄스마다 Ability->ApplyOnTargets (피해 · 적중 효과 조각). */
	void InitializeFromFragment(UERGameplayAbility* InAbility, const UERSkillData* InSkill, int32 InLevel, float InDuration, float InRadius, float InTickInterval, float InDecay,
		const class UERSkillFragment_Area* InFragment = nullptr);

protected:
	void Pulse();
	/** 시전자가 안에 있으면 CasterEffectsInside 를 다시 건다 (F12.6-06 신경 가스). */
	void ApplyCasterInside(AActor* Caster);

	/** 어그로 · 시전자 효과 옵션을 읽는 조각 (애셋 — 상태 없음). */
	UPROPERTY()
	TObjectPtr<const class UERSkillFragment_Area> Fragment;
	float TickInterval = 1.f;

	TWeakObjectPtr<UERGameplayAbility> Ability;
	UPROPERTY()
	TObjectPtr<const UERSkillData> Skill;
	int32 Level = 1;
	/** 대상별 맞은 횟수 — 감쇠용. */
	TMap<TWeakObjectPtr<AActor>, int32> HitCounts;
	FTimerHandle PulseTimer;
	float Radius = 1.5f;
	float Decay = 0.f;
};
