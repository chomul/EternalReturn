// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

struct FERCharStats;
class UAbilitySystemComponent;
class UGameplayEffect;

/**
 * 실험체 기본 스탯을 어트리뷰트에 넣는다.
 *
 * ⭐ 값을 직접 Set 하지 않는다. Instant GameplayEffect 의 SetByCaller 로 넣어
 *   클램프(F02-03)와 복제 경로를 그대로 탄다.
 *   근거: Docs/4_Argument/3_어트리뷰트셋_구조.md C 절
 *
 * ⭐ GE 는 **에디터 애셋**이다(C-6 결정). 기획자가 소유한다.
 *   대신 애셋이 기대한 모양인지 C++ 이 검사한다 - GAS 는 어긋나도 조용하기 때문이다:
 *   "skip over any modifiers for attributes that we don't have" (GameplayEffect.cpp:4238)
 *
 * 캐릭터와 야생동물(F12)이 같이 쓴다.
 */
class ERAttributeInit
{
public:
	/**
	 * 스탯 값을 초기화 GE 로 적용한다.
	 *
	 * ⚠ **서버에서만 부른다.** 클라는 복제로 받는다.
	 * ⚠ ASC 의 InitAbilityActorInfo 가 **끝난 뒤에** 불러야 한다. 그 전이면 조용히 실패한다.
	 *
	 * @return 적용에 성공했으면 true. 실패 사유는 전부 UE_LOG(Error) 로 남는다.
	 */
	static bool ApplyStatRow(UAbilitySystemComponent* ASC,
		TSubclassOf<UGameplayEffect> InitEffectClass,
		const FERCharStats& Row);

	/**
	 * 초기화 GE 애셋이 기대한 모양인지 검사한다.
	 *
	 * 개수 · 어트리뷰트 이름 · **순서** · 연산 · SetByCaller 키를 모두 본다.
	 * 틀린 줄마다 인덱스와 이름을 찍는다.
	 *
	 * ⚠ 순서를 검사하는 이유: MaxHP 가 HP 보다 뒤로 밀리면 체력이 0 으로 클램프된다.
	 *   모디파이어는 배열 순서대로 즉시 적용된다 (GameplayEffect.cpp:3004).
	 *
	 * 배포 빌드에서는 아무것도 하지 않고 true 를 돌려준다.
	 */
	static bool ValidateInitEffect(const UGameplayEffect* Effect);
};
