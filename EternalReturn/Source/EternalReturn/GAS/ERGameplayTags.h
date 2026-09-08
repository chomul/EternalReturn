// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

/**
 * 프로젝트 전역 게임플레이 태그.
 *
 * 태그 문자열은 이 파일과 짝 .cpp 에만 존재한다. 다른 곳에서
 * FGameplayTag::RequestGameplayTag(FName("...")) 로 태그를 만들지 않는다 —
 * 오타가 런타임까지 안 걸리고, 이름을 바꿀 때 조용히 깨진다.
 *
 * ⚠ 태그 이름을 바꾸면 이미 만든 GameplayEffect 애셋 참조가 끊어진다.
 *
 * 행동 강제 계열(공포·매혹·도발·광기)과 수면·에어본은 일부러 넣지 않았다.
 * 선행 구현 6인 중 요구하는 캐릭터가 없어 검증할 방법이 없다.
 * 7번째 캐릭터가 올 때 추가한다.
 */
namespace ERTags
{
	// ── 군중 제어 ──────────────────────────────────────────────
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CC_Stun);        // 기절 — 이동·평타·스킬 전부 차단
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CC_Snare);       // 속박 — 이동만 차단
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CC_Silence);     // 침묵 — 스킬만 차단
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CC_Disarm);      // 무장 해제 — 평타 + "평타 판정 스킬"까지 차단
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CC_Slow);        // 둔화
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CC_Blind);       // 실명
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CC_VisionBlock); // 시야 차단

	// ── 상태 ───────────────────────────────────────────────────
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Stealth);        // 은신
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);   // 무적
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CCImmune);       // 이동 방해 면역 (매그너스 R)

	// ── 어빌리티 형태 ──────────────────────────────────────────
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Form_Channeled);      // 채널링 — CC로 중단된다
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Form_NextAttackBuff); // 다음 평타 강화 — 무장 해제가 이것도 막아야 한다

	// ── 어빌리티 슬롯 ──────────────────────────────────────────
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_P);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_Q);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_W);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_E);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_R);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_D);       // 무기가 소유하는 슬롯

	// ── 피해 채널 ──────────────────────────────────────────────
	// 방어력·치명타 적용 여부가 이 셋으로 갈린다. 데미지 GE 에 반드시 하나가 붙어야 한다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_BasicAttack); // 방어력 O / 치명타 O
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Skill);       // 방어력 O / 치명타 X
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_True);        // 방어력 X / 치명타 X

	// ── 초기 스탯 SetByCaller 키 ───────────────────────────────
	//
	// GE_ERInitStats 애셋의 모디파이어가 이 태그로 값을 받는다.
	//
	// ⚠ 반드시 "SetByCaller." 로 시작해야 한다 - FSetByCallerFloat::DataTag 의
	//   meta = (Categories = "SetByCaller") 가 에디터 태그 선택기를 그 아래로만 거른다
	//   (GameplayEffect.h:257).
	//
	// ⚠ FName 쪽(DataName)은 쓰지 않는다. VisibleDefaultsOnly 라 에디터에서 편집이 안 되고
	//   (GameplayEffect.h:253-255), 런타임 경로도 deprecated 다 (GameplayEffect.cpp:1128).
	//
	// 순서와 값 대응은 ERAttributeInit.cpp 의 BuildInitMagnitudes 가 갖는다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_MaxHP);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_MaxVP);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_HP);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_VP);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_HPRegen);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_VPRegen);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_AttackPower);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Defense);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_AttackSpeed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_MoveSpeed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Sight);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_AttackRange);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_CritChance);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_CritDamageUp);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_SkillAmp);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_BasicAtkAmp);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_DefPenPercent);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_DefPenFlat);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_DamageUp);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_FinalDamageUpPercent);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_FinalDamageUpFlat);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_SkillHaste);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_DamageDown);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_BasicAtkDamageDown);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_SkillDamageDown);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_SlowResist);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_CCResist);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Lifesteal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_OmniLifesteal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_HealAmp);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_OutOfCombatRegen);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_ModeDamageUp);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_ModeDamageDown);
}
