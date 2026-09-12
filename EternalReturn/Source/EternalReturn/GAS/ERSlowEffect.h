// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ERSlowEffect.generated.h"

/**
 * "지금 적용 중인 가장 강한 둔화" 를 나타내는 GE. **서버가 하나만 유지한다.**
 *
 * ⭐ 둔화 GE 애셋은 MoveSpeed 를 직접 건드리지 않는다. State.CC.Slow 태그와
 *   SetByCaller.SlowPercent(감소율)만 들고 있다.
 *   서버가 그중 **가장 큰 감소율**을 골라 이 GE 를 그 값으로 적용한다.
 *   -> ERCC::RecalculateSlow
 *
 * ⭐ **왜 이렇게 하나** (Docs/4_Argument/12_둔화_중첩방식.md)
 *   원작·LoL 모두 다중 둔화에서 **가장 강한 것만** 적용하고 나머지는 무시한다.
 *   그런데 GAS 의 Multiply 모디파이어는 **합산**된다
 *   (GameplayEffectAggregator.cpp:78 + :194-207) — 60% 둔화 둘이면 감소율이
 *   120% 가 되어 음수가 나온다. 그래서 "최대값 하나" 를 우리가 만들어 넣는다.
 *
 * ⭐ 이 방식의 이점: MoveSpeed 어트리뷰트가 **실제로** 깎이므로
 *   기존 하한 클램프(PreAttributeChange, F02-03)를 그대로 탄다.
 *   클램프를 두 곳에서 관리하지 않는다.
 *
 * ⚠ **애셋이 아니라 네이티브인 이유**: 조정 대상 수치가 0개다.
 *   지속시간도 값도 전부 코드가 정한다 — UERLifestealEffect 와 같은 논리
 *   (Docs/4_Argument/9_CC효과_표현방식.md). 기획자가 만질 것이 없다.
 *
 * ⚠ 이 GE 는 태그를 부여하지 않는다. State.CC.Slow 는 둔화 GE 애셋의 것이다.
 *   여기에 태그를 붙이면 RecalculateSlow 가 자기 자신을 둔화로 세어 무한히 커진다.
 */
UCLASS()
class UERSlowEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UERSlowEffect();
};
