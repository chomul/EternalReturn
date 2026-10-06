// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ERBleedEffect.generated.h"

class UAbilitySystemComponent;

/**
 * 출혈 (F19-03 재키 P 피의 축제 · Argument 65 D1) — 프로젝트 첫 지속 피해.
 * HasDuration (SetByCaller.StateDuration) · 1초마다 ERDamageExecution · 걸은 사람(Source)별로 최대 5중첩 ·
 * **새로 쌓일 때마다 전체 지속 갱신** (사용자 2026-10-05) · 틱 주기는 갱신하지 않는다 (계속 1초 간격).
 *
 * 틱 피해 = 계수(중첩 하나의 한 틱 몫) × **중첩 수** — 실행 계산이 Spec 의 중첩 수를 곱한다 (주기 실행은 중첩된 Spec 으로 불린다 · GameplayEffect.cpp:3137).
 * 거는 쪽(조각)이 넣는 것: Data.Damage.* 계수 · Damage.Type.Skill + Damage.Secondary (틱이 적중 이벤트를 안 내게) · State.Bleeding (동적 태그).
 * ⚠ 중첩이 쌓여도 Spec 은 처음 것이 남는다 — 계수(공격력 · 레벨)는 첫 적용 때 값 `[자체]`.
 * 네이티브인 이유: 기획자가 만질 값이 없다 — 수치는 스킬 DA (UERSkillDamageEffect 와 같은 논리).
 */
UCLASS()
class ETERNALRETURN_API UERBleedEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UERBleedEffect();

	static constexpr int32 MaxStacks = 5;
	static constexpr float TickSeconds = 1.f;

	/** [서버] Target 에 Source 가 건 출혈 중첩 수 (0 = 없음). Source 가 null 이면 누구 것이든. */
	static int32 GetStacks(const AActor* Target, UAbilitySystemComponent* Source);
};
