// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ERSkillStateEffects.generated.h"

/**
 * 스킬 상태 GE 들 (F07-07). 전부 **태그 없음** — 어빌리티가 DynamicGrantedTags 로 심는다 (16 · 17 과 같은 패턴).
 * 길이는 SetByCaller.StateDuration. **0 이하를 넣을 수 없어서** 무한 버전을 따로 둔다.
 * 근거: Docs/4_Argument/19_평타_다음평타강화_리캐스트_구조.md ②A · ③A
 */

/** 다음 기본 공격 강화 대기 — HasDuration. 컨텍스트 SourceObject = 버프를 건 스킬의 UERSkillData. 평타가 적중 시 읽고 제거한다. */
UCLASS()
class UERNextAttackBuffEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UERNextAttackBuffEffect();
};

/** 같은 것, Infinite — NextAttackBuffDuration 이 0 (만료 없음 · 재키 W 는 만료 (미확인)) 일 때. */
UCLASS()
class UERNextAttackBuffInfiniteEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UERNextAttackBuffInfiniteEffect();
};

/** 리캐스트 윈도우 — HasDuration. 동적 태그 Recast.Slot.* 가 있는 동안 그 슬롯은 쿨다운을 무시하고 발동된다. */
UCLASS()
class UERRecastWindowEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UERRecastWindowEffect();
};
