// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "ERCCDurationCalc.generated.h"

/**
 * CC 지속시간 = 스킬이 준 길이 × (1 - 대상의 CCResist)
 *
 * ⭐ **CC GE 애셋의 `Duration Magnitude` 를 이 클래스로 지정한다.**
 *   그러면 모든 CC 가 자동으로 방해 저항을 받는다 — 스킬마다 빠뜨릴 일이 없다.
 *
 * ⭐⭐ **왜 여기서 계산하나** (Docs/4_Argument/14_저항_적용방식.md 방안 A)
 *   부여하는 쪽(스킬)이 **대상의 저항을 몰라도 되게** 하려는 것이다.
 *   F03 에서 어빌리티가 대상 방어력을 모르고 ExecutionCalculation 이 캡처해 쓰는 것과
 *   같은 구조다. 스킬이 대상 어트리뷰트를 읽기 시작하면 저항 계산이 흩어진다.
 *
 * ⚠ **`SetByCaller` 와 같이 쓸 수 없다.** DurationMagnitude 는 하나뿐이라,
 *   "스킬이 준 길이" 와 "저항 계산" 을 **이 클래스 안에서 합친다.**
 *   -> SetByCaller.CCDuration 을 여기서 읽는다.
 *
 * ⚠ **붙잡기 · 에어본 · 제압은 방해 저항을 받지 않는다** (역기획서 §4 · 원작 확인).
 *   그 CC 들의 GE 는 이 클래스를 쓰지 않고 SetByCaller 를 그대로 쓴다.
 */
UCLASS()
class UERCCDurationCalc : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UERCCDurationCalc();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;
};
