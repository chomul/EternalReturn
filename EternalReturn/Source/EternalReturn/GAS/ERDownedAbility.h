// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/ERGameplayAbility.h"
#include "ERDownedAbility.generated.h"

/**
 * 빈사 스킬 (F14 S4 · Argument 79) — 분열 폭발 (Q) · 집념 (W) · 자가 제세동 (R). 수치 · 기능은 DA 와 조각, 이 클래스는 발동 조건만.
 *
 * ⭐ 일반 스킬과 조건이 거꾸로다 — **빈사일 때만** 발동 (bDownedSkill · UERGameplayAbility::CanActivateAbility). 무기 없어도 된다.
 * ⭐ DA 의 AbilityClass 에 이 C++ 클래스를 바로 넣는다 (BP 자식 금지 — 차단 칸을 BP 가 저장하면 생성자 값이 안 내려간다 · E41).
 * 부여: 모든 실험체 — PlayerState 가 ER Combat 의 DownedSkills 를 준다 (아군 소생과 같은 자리). 입력: 빈사면 Q · W · R → Ability.Slot.Downed.* (PC).
 */
UCLASS()
class ETERNALRETURN_API UERDownedAbility : public UERGameplayAbility
{
	GENERATED_BODY()

public:
	UERDownedAbility();
};
