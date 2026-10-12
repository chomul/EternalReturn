// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERDownedAbility.h"
#include "GAS/ERGameplayTags.h"

UERDownedAbility::UERDownedAbility()
{
	bDownedSkill = true;
	// 빈사 · 무기 없음은 막지 않는다 (빈사 스킬은 빈사일 때 · 맨손으로 쓴다). CC(State.Block.Skill) · 사망 · 아군 소생 중은 부모 그대로
	ActivationBlockedTags.RemoveTag(ERTags::State_Downed);
	ActivationBlockedTags.RemoveTag(ERTags::State_Unarmed);
	// 빈사 스킬 하나가 도는 동안 (집념 3초 · 도화선 4초 · 자가 제세동 집중) 다른 빈사 스킬은 못 쓴다 (사용자 2026-10-12 "스킬 하나 쓰면 다른 스킬 못 씀")
	//   R 재입력 취소는 발동이 아니라 PC 가 직접 끊는다 — 이 차단에 안 걸린다
	ActivationBlockedTags.AddTag(ERTags::State_DownedSkill_SelfReviving);
	ActivationBlockedTags.AddTag(ERTags::State_DownedSkill_Obstinacy);
	ActivationBlockedTags.AddTag(ERTags::State_DownedSkill_Fuse);
}
