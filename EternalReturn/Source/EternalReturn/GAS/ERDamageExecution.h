// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"

#include "ERDamageExecution.generated.h"

/**
 * 이 프로젝트의 **모든** 피해가 예외 없이 통과하는 계산기. 하나뿐이다.
 *
 * ⭐ Execution 을 여러 개 만들거나 어빌리티 안에서 직접 계산하지 않는다.
 *   그렇게 하면 계산식이 스킬 수만큼 복제되고, 밸런스를 고칠 때 전부 찾아야 한다.
 *   근거: Docs/0_GameDesign/Systems/스탯_데미지공식_역기획서.md §7
 *         *"2순위가 이 문서의 전부다. 나머지는 전부 2순위 함수에 인자를 더하는 작업이다."*
 *
 * ⭐ **어빌리티는 계수만 넘긴다.** 스탯을 읽어 곱하는 것은 여기뿐이다.
 *   입력은 Data.Damage.* SetByCaller 태그로 온다 (ERGameplayTags.h).
 *   근거: Docs/4_Argument/5_추가공격력_산출방식.md
 *
 * ⭐ **체력을 직접 건드리지 않는다.** IncomingDamage 메타 어트리뷰트에 출력만 하고,
 *   체력 차감·클램프·사망 알림은 UERAttributeSet 이 한다 (F02-03 / F02-04).
 *   이 분리를 깨면 클램프와 사망 알림이 통째로 우회된다.
 *
 * ⚠ 로그·이펙트·사운드를 여기에 섞지 않는다. 연출은 GameplayCue 다 (CLAUDE.md §8).
 *
 * 구현 진행 상황은 .cpp 상단 표를 본다.
 */
UCLASS()
class ETERNALRETURN_API UERDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UERDamageExecution();

	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
