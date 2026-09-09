// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "ERLifestealEffect.generated.h"

/**
 * 흡혈 회복을 공격자에게 넣는 Instant GameplayEffect.
 *
 * ⭐ **애셋이 아니라 네이티브 클래스다.** 프로젝트 방침은 "GE 는 애셋"(Argument 3 C-6)이지만
 *   이건 예외다 - 기획자가 볼 내용이 **하나도 없다**:
 *     · 모디파이어 1개, 조정할 수치 0개 (값은 전부 SetByCaller)
 *     · Execution 이 코드로 만들어 코드로 적용한다. 아무도 열어보지 않는다
 *     · 애셋으로 두면 Execution 이 그 애셋을 참조할 경로를 따로 만들어야 한다
 *       (Execution 은 GE 애셋마다 설정되는 게 아니라 클래스 하나다)
 *   근거: Docs/4_Argument/6_흡혈_적용경로.md
 *
 * ⚠ 이 GE 는 **공격자에게** 적용된다. 피격자가 아니다.
 *   Execution 의 출력 수정자는 언제나 Target 에 가므로(GameplayEffect.cpp:3809-3821)
 *   흡혈은 그 경로를 쓸 수 없다.
 *
 * 회복량은 IncomingHealing 메타로 들어가고, UERAttributeSet 이 HealAmp 를 곱해 HP 에 넣는다.
 */
UCLASS()
class ETERNALRETURN_API UERLifestealEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UERLifestealEffect();

	// SetByCaller 키는 ERTags::SetByCaller_HealAmount 이다.
	// ⚠ FName 경로(DataName)를 쓰지 않는다 - deprecated 다 (GameplayEffect.cpp:1128, E06 참조).
};
