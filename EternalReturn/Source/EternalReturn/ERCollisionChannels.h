// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"

/**
 * 프로젝트 커스텀 콜리전 채널.
 *
 * ⚠ 여기 있는 값은 Config/DefaultEngine.ini 의 [/Script/Engine.CollisionProfile]
 *   항목과 반드시 짝이 맞아야 한다. 한쪽만 바꾸면 조용히 엉뚱한 채널을 긁는다.
 *
 * ⚠ 코드에서 ECC_GameTraceChannel1 같은 이름을 직접 쓰지 않는다.
 *   번호가 바뀌면 어디를 고쳐야 하는지 알 수 없게 된다.
 *
 * 채널은 18개(ECC_GameTraceChannel1~18)뿐이다. 새로 팔 때마다 여기에 적는다.
 */
namespace ERCollisionChannel
{
	/**
	 * 스킬 판정 대상.
	 *
	 * 기본 응답이 Ignore 다 — 판정에 걸려야 하는 액터만 Overlap 으로 켠다.
	 * 실험체 · 야생동물 · 보스 · 설치물이 대상이고, 관전자나 연출용 더미는 켜지 않는다.
	 *
	 * ⚠ 켜는 것을 빠뜨리면 그 액터만 조용히 안 맞는다. 이 채널을 고른 대가다
	 *   (Docs/Argument/2. 판정 콜리전 설계.md A-5 참조).
	 */
	inline constexpr ECollisionChannel SkillTarget = ECC_GameTraceChannel1;
}
