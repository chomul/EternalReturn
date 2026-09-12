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
	// ⏸ 은신 — **태그만 있다. 가시성 처리는 F16(팀 시야)에서 한다.**
	//
	// ⚠⚠ **"화면에서 안 보이게" 만 만들면 안 된다.** 위치가 계속 복제되면
	//   치트 클라가 패킷을 읽어 은신한 적을 그대로 본다.
	//   역기획서 검증 기준: *"다니엘 E 은신 중 적 클라이언트에 위치가 복제되지 않음"*
	//
	// ⭐ 이건 F16 의 팀 시야와 **같은 문제**다 — 릴리번시(IsNetRelevantFor /
	//   ReplicationGraph)로 막아야 하고, 그 방식은 F16 착수 전에 정한다
	//   (Docs/1_Task/00_기능목록_및_의존성.md 의 미결 항목).
	//
	// ⚠ 여기서 반쪽(머티리얼 숨김)만 만들면 F16 에서 다시 걷어내야 하고,
	//   그 사이 "은신이 되는 줄 알았는데 뚫리는" 상태가 된다. 그래서 **안 만든다.**
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Stealth);        // 은신 — ⏸ F16
	// ⚠⚠ **무적과 피해 면역은 다른 것이다** — 역기획서 §3.4 가
	//   "반드시 다른 플래그여야 한다" 로 명시한다. 하나로 합치면 나중에 못 나눈다.
	//
	//   무적(Invincible)   : 피해 0. ⭐ **CC 는 걸린다** (사용자 확인 2026-09-10)
	//   피해 면역          : 피해 0. ⭐ **CC 는 걸린다** — 시셀라 W
	//
	// ⭐ 둘 다 "피해만 0" 이라 **지금은 로직이 같다.** ERDamageExecution 종단이 둘 다 본다.
	//
	// ⚠⚠ **그래도 태그를 합치지 않는 이유**: 원작 서술에 **피격 판정** 차이가 시사된다 —
	//   피해 면역은 *"피격 판정 자체는 남고, 표식을 남기는 효과는 막을 수 없다"*.
	//   무적은 피격 자체가 없을 수 있다. **(미확인)**
	//   온힛·표식 처리가 생기는 F07/F08 에서 갈릴 자리다. 지금 합치면 그때 못 나눈다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);   // 무적 — 양손검 D 패링

	// ⭐ 피해 면역 — 시셀라 W. **피해만 0 이고 CC 는 정상으로 걸린다.**
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_DamageImmune);

	// ⏸ 저지 불가 — CC 가 **걸리되 효과만 억제**된다. 풀리면 남은 시간만큼 발동한다.
	//   ⚠ "면역이면 무시" 로 만들면 원작과 다른 게임이 된다 (역기획서 §4).
	//   6인 중 요구 사례가 없어 **자리만** 만든다. 지연 발동은 그때 구현한다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Unstoppable);

	// ⏸ 대상 지정 불가 — 이미 적용된 효과 외 전부 무시. 요구 사례 없음. 자리만.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Untargetable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CCImmune);       // 이동 방해 면역 (매그너스 R)

	// ── 차단 축 ────────────────────────────────────────────────
	//
	// ⭐ 위의 State.CC.* 는 **무엇에 걸렸는지**, 아래는 **무엇이 막히는지**를 나타낸다.
	//   막는 쪽(CMC · 어빌리티)은 아래 태그만 본다 — CC 종류를 모른다.
	//   그래서 CC 를 추가해도 막는 쪽을 고치지 않는다.
	//
	// ⚠ CC GE 애셋은 두 종류를 **모두** 부여해야 한다.
	//   예) GE_CC_Stun → State.CC.Stun + Block.Movement + Block.BasicAttack + Block.Skill
	//       GE_CC_Snare → State.CC.Snare + Block.Movement 만 (스킬은 쓸 수 있다)
	//   축 태그를 빠뜨리면 **조용히 안 막힌다.** ERCCLibrary 가 검사한다.
	//
	// 근거: Docs/4_Argument/10_CC_차단축_태그설계.md (방안 A)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Block_Movement);    // 이동 차단
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Block_BasicAttack); // 평타 차단
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Block_Skill);       // 스킬 차단

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
	// ⭐ CC 지속시간 — GE 애셋의 DurationMagnitude 를 SetByCaller 로 두고 스킬이 넣는다.
	//   같은 기절이 0.5~1.3초로 스킬마다 다르기 때문이다 (역기획서 §3.1).
	//   ⚠ 값을 안 넣으면 지속시간이 0 이 되어 CC 가 안 걸린다. ERCCLibrary 가 검사한다.
	//   근거: Docs/4_Argument/9_CC효과_표현방식.md (방안 A)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_CCDuration);

	// ⭐ 둔화 — 태그가 **2개**인 이유: 담는 값의 의미가 다르다.
	//
	//   SlowPercent    : 둔화 GE 애셋이 받는 **감소율**.  60% 둔화 -> 0.6
	//   SlowMultiplier : UERSlowEffect 가 받는 **배수**.  60% 둔화 -> 0.4 (= 1 - 0.6)
	//
	// ⚠ 같은 태그에 다른 의미를 담으면 언젠가 한쪽을 잘못 읽는다. 그래서 나눴다.
	//   변환은 ERCC::RecalculateSlow 한 곳에서만 한다.
	//
	// 근거: Docs/4_Argument/12_둔화_중첩방식.md (방안 B + 구현 ③)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_SlowPercent);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_SlowMultiplier);

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

	// ⚠ 흡혈 회복 GE 가 쓰는 키. 위 33개는 **어트리뷰트 이름**이라 SetByCaller.Lifesteal 은
	//   "흡혈률 어트리뷰트를 세팅" 이라는 뜻으로 이미 쓰이고 있다. 회복량은 다른 키를 쓴다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_HealAmount);

	// ── 데미지 계수 (어빌리티 -> Execution) ────────────────────
	//
	// ⭐ 어빌리티는 **"몇 배"만** 넘긴다. 스탯을 읽어 곱하는 것은 ERDamageExecution 하나뿐이다.
	//   어빌리티가 곱하면 계산식이 스킬 수만큼 복제된다.
	//   근거: Docs/4_Argument/5_추가공격력_산출방식.md
	//
	// ⚠ 이 태그들은 SetByCaller 지만 접두사가 "SetByCaller." 가 아니다.
	//   그 접두사는 **GE 애셋의 모디파이어 선택기**가 요구하는 것이고
	//   (GameplayEffect.h:257), 이쪽은 C++ 이 넣고 C++ 이 읽어서 선택기를 안 탄다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage_Base);            // 스킬 고정 피해량
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage_APRatio);         // 공격력 계수
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage_BonusAPRatio);    // **추가** 공격력 계수
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage_SkillAmpRatio);   // 스킬 증폭 계수

	// 체력 비례 피해 계수. ⚠ 최대/현재를 혼동하면 스킬 성격이 정반대가 된다 -
	//   재키 Q 는 **현재** 체력 비례라 대상이 죽어갈수록 약해지는데,
	//   최대 체력으로 계산하면 처형기가 된다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage_MaxHPRatio);      // 대상 **최대** 체력 비례
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage_CurHPRatio);      // 대상 **현재** 체력 비례
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage_LostHPRatio);     // **자신이 잃은** 체력 비례

	// ── 판정 형상 · 액터 유형 ──────────────────────────────
	//
	// 흡혈의 치유 감소 조건이다. ⭐ Execution 이 Cast<> 로 클래스를 검사하지 않는다 -
	// 그러면 F03(데미지)이 F12(야생동물)에 의존하게 된다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Shape_AoE);   // 어빌리티가 GE Spec 에 붙인다
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Actor_Type_Wildlife); // 야생동물 액터가 갖는다
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Actor_Type_Boss);     // 보스 액터가 갖는다 (F12 책임)
}
