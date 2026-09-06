// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;

/**
 * 임의의 액터에서 팀을 알아내는 공용 함수.
 *
 * ⭐ 다른 곳에서 Cast<AERPlayerState> 를 직접 하지 않게 하려고 만든다.
 *   데미지 판정·시야·UI 가 각자 캐스팅하면, 나중에 팀 정보를 옮길 때 전부 고쳐야 한다.
 *
 * ⚠ 야생동물·보스·중립 오브젝트는 팀이 없다(INDEX_NONE). 이 "무소속"을
 *   아군으로 취급하면 곰끼리 서로 안 때린다. IsSameTeam 의 규칙을 반드시 지킨다.
 */
class ERTeamStatics
{
public:
	/** 액터의 팀 ID. 팀이 없으면 INDEX_NONE. nullptr 도 INDEX_NONE. */
	static int32 GetTeamId(const AActor* Actor);

	/** 둘이 같은 팀인가. ⭐ 어느 한쪽이라도 무소속이면 false. */
	static bool IsSameTeam(const AActor* A, const AActor* B);

	/**
	 * 서로 적인가.
	 *
	 * ⭐ !IsSameTeam 과 다르다. 무소속(야생동물)은 "같은 팀은 아니지만" 적으로 봐야 하고,
	 *   무소속끼리도 적이다(곰과 늑대). 반면 자기 자신은 적이 아니다.
	 *   부정 연산으로 대신하지 말고 이 함수를 쓴다.
	 */
	static bool IsHostile(const AActor* A, const AActor* B);
};
