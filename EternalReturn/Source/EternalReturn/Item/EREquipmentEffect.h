// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "AttributeSet.h"
#include "EREquipmentEffect.generated.h"

/**
 * 장비 스탯 GE. **Infinite 하나를 모든 장비가 공유한다.** (F08-03)
 *
 * ⭐⭐ **Infinite 여야 한다.** Instant 면 BaseValue 를 바꿔서 벗어도 안 돌아오고, 추가 공격력(EvaluateBonus = Current − Base)이 0 이 된다
 *   (Docs/4_Argument/5_추가공격력_산출방식.md · F02-05 의 함정). Infinite 는 CurrentValue 만 바꾼다 — 그게 곧 "추가" 분이다.
 *
 * ⭐ 모디파이어 = **장비 가능한 어트리뷰트 전부**, 각각 Additive · SetByCaller(기존 SetByCaller.<Attr> 태그).
 *   장착 시 아이템의 StatModifiers 를 넣고 **나머지는 0 을 명시**한다 (Additive 0 = 무효, 미지정이면 GAS 가 경고).
 *   왜 이렇게: GE 모디파이어 목록은 클래스에 고정이라 아이템마다 다른 어트리뷰트 조합을 GE 하나로 받으려면 "전부 있고 대부분 0" 이어야 한다.
 *   아이템마다 GE 애셋(수백 개)도, 런타임 GE 생성(Def 가 클라에 없어 복제 실패)도 안 한다.
 *
 * 해제 = 핸들로 RemoveActiveGameplayEffect. **수동 차감 없음** (부동소수 오차 · 중첩 어긋남).
 */
UCLASS()
class UEREquipmentEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UEREquipmentEffect();

	/** 장비가 건드릴 수 있는 어트리뷰트 ↔ SetByCaller 태그. 이 표에 없는 어트리뷰트는 아이템 행에 넣어도 무시된다 (경고). */
	struct FBinding
	{
		FGameplayAttribute Attribute;
		FGameplayTag Tag;
	};
	static const TArray<FBinding>& GetBindings();
};
