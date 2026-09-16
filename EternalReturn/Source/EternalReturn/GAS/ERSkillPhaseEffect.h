// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ERSkillPhaseEffect.generated.h"

/**
 * 스킬 페이즈(선딜 · 후딜) 상태 GE. **HasDuration, 길이 = SetByCaller.PhaseDuration, 태그 없음.**
 *
 * ⭐ 어떤 페이즈인지는 UERGameplayAbility 가 DynamicGrantedTags 로 심는다:
 *   선딜 = { State.Casting, (State.Block.Movement — bMoveCancelsCast 가 아니면) }
 *   후딜 = { State.Recovering }
 *   쿨다운(UERCooldownEffect)과 같은 패턴 — GE 하나, 태그는 동적.
 *
 * ⭐ 왜 GE 인가 (Docs/4_Argument/17_시전상태_표현과_취소경로.md ①A):
 *   "시전 중" 은 지속시간이 있는 상태다. 부여 태그는 Mixed 에서 전원에게 복제되어
 *   다른 플레이어도 시전 중임을 보고, 클라 UI 가 남은 시간을 읽을 수 있다.
 *
 * ⚠ 어빌리티가 페이즈를 끝낼 때 **핸들로 직접 제거**한다. WaitDelay 와 GE 만료가 각자 시간을
 *   세므로 둘 중 먼저 끝난 쪽에 맞춰 정리해야 한다.
 */
UCLASS()
class UERSkillPhaseEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UERSkillPhaseEffect();
};
