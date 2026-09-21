// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ERUnarmedEffect.generated.h"

/**
 * 무기 없음 상태 GE (F11-02). **Infinite · 부여 태그 State.Unarmed** — UERGameplayAbility 의 ActivationBlockedTags 에 있어 평타 · Q/W/E/R 전부 막힌다.
 * ⭐ 원작 확인 (사용자 2026-09-19): 무기가 없으면 아무 스킬도 못 쓴다. 맨손 평타 없음.
 * 무기 슬롯이 비면 AERPlayerState::RefreshWeaponSkills 가 걸고, 장착하면 핸들로 지운다. 태그는 Mixed 에서 전원 복제 — UI 가 잠금 표시.
 */
UCLASS()
class UERUnarmedEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UERUnarmedEffect();
};
