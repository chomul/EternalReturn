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
 * 행동 강제 계열(공포·매혹·도발·광기)과 수면은 일부러 넣지 않았다.
 * 선행 구현 6인 중 요구하는 캐릭터가 없어 검증할 방법이 없다.
 * 7번째 캐릭터가 올 때 추가한다. (에어본은 F12.6 멧돼지 돌진이 요구해 추가 — 2026-09-30)
 */
namespace ERTags
{
	// ── 군중 제어 ──────────────────────────────────────────────
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CC_Stun);        // 기절 — 이동·평타·스킬 전부 차단
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CC_Airborne);    // 에어본 — 기절과 같은 축 전부 차단 · 방해 저항 안 받음 (F12.6-03 멧돼지 돌진)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Frustrated);     // 좌절 — 스킬 실패 벌칙 표시 (멧돼지 돌진 빗나감 · 시전 중 CC). 막는 축은 함께 거는 기절이 맡는다
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
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Bleeding);       // 출혈 중 (재키 P · UERBleedEffect 동적 태그 · Argument 65 D1)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Adrenaline);     // 아드레날린 분비 (재키 P — 적중마다 출혈 최대 · 추가 피해 · 회복)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Riding);         // 탑승 중 (매그너스 R 바이크 — GE_Magnus_Ride 가 단다 · UERRideComponent 가 본다 · Argument 62)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_AnimHold);       // 스킬 모션 유지 — 이동해도 안 끊고, 빠지면 End 섹션 (매그너스 W 장판 동안 · Argument 63 M1). 연출 전용
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Gathering);      // 채집 중 — 연출 전용 (AnimBP 채집 포즈 · Argument 46). 아무것도 막지 않는다

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

	// ── 스킬 페이즈 (F07-04) ───────────────────────────────────
	// UERSkillPhaseEffect 가 동적 부여 태그로 심는다. 전원에게 복제된다(Mixed).
	//   근거: Docs/4_Argument/17_시전상태_표현과_취소경로.md (①A)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Casting);           // 선딜(캐스팅) 중 — CC(State.Block.Skill) 로 끊긴다
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Recovering);        // 후딜 중 — 스킬 발동 차단, 이동 입력이 끝낸다

	// ── 무기 (F11-02) ──────────────────────────────────────────
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Unarmed);           // 무기 없음 — 평타 · 스킬 전부 차단 (원작 확인: 무기 없으면 아무 스킬도 못 쓴다). UERUnarmedEffect 가 준다
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_InCombat);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_ConsumeOnAttack);   // 이 태그가 있는 자기 버프 GE 는 기본 공격 적중마다 Charges 가 1 줄고 0 이면 사라진다 (F11-05 B)          // 전투 중 — 피해를 주거나 받은 뒤 CombatStateSeconds. 무기 교체 불가 (원작 확인). 비전투 재생 · 귀환도 이걸 본다

	// 다음 기본 공격 강화 대기 (F07-07). UERNextAttackBuffEffect 가 부여, 평타가 적중 시 소비.
	//   카티야 P · 재키 W · 시셀라 Q · 권총 D 가 같은 GE 를 쓴다 (역기획서 §8 "4곳이 같은 구조").
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_NextAttackBuff);

	// ── 시셀라 (F19-04 · Argument 68) ─────────────────────────
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Shielded);          // 보호막 지속 중 — 끝나면 Shield 어트리뷰트를 0 으로 (ERShield)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_WilsonAway);        // 윌슨이 떨어져 있다 (AERWilson 이 있는 동안 · 복제 루즈 태그)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_LostHPStats);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Bubble);            // 시셀라 W 감싸는 중 (피해 면역 · 이속 GE 에 같이) — 터질 때 이걸로 지운다       // 잃은 체력 비례 스탯 GE (상시 1개 — 레벨이 바뀌면 이걸로 찾아 지우고 다시)

	// ── 게임플레이 이벤트 ──────────────────────────────────────
	// 입력 → 어빌리티. PC 가 어빌리티 내부를 모르게 하는 통로 (SendGameplayEventToActor).
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Input_Move);        // 이동 명령이 서버에서 수락됨
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Skill_Aim);         // 스킬 발동 요청에 실린 조준 데이터 (Docs/4_Argument/18)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Kill_Dealt);        // 내가 적 실험체를 처치했다 (막타 — F14 전 임시 "처치 관여") — 플레이어 스테이트가 처치자 ASC 에 · 패시브가 듣는다 (Argument 65)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Wilson_Joined);     // 윌슨과 하나가 됐다 (줍기 · 거리 복귀 · E · W) — AERWilson 이 시셀라 ASC 에 · 패시브가 듣는다 (Argument 68)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit_Dealt);         // 내 피해가 들어갔다 — 어트리뷰트셋이 가해자에게 (Target = 맞은 쪽 · InstigatorTags = Damage.Type.*) · 패시브가 듣는다 (Argument 61 E1)

	// ── 어빌리티 형태 ──────────────────────────────────────────
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Form_Channeled);      // 채널링 — CC로 중단된다
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Form_NextAttackBuff); // 다음 평타 강화 — 무장 해제가 이것도 막아야 한다

	// ── 어빌리티 슬롯 ──────────────────────────────────────────
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot);         // 부모 — RPC 가 "슬롯 계열인지" 검증할 때
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_P);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_Q);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_W);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_E);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_R);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_D);       // 무기가 소유하는 슬롯
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_Attack);  // 기본 공격 (F07-07). 무기가 소유 (F11). 포인트 대상 아님

	// ── 리캐스트 연출 키 (F12.5-03 · 사용자 2026-09-29) ── 슬롯의 **자식**이라 슬롯 매칭(HasTagExact)에는 안 걸린다.
	//   리캐스트(재입력)로 발동하면 연출 컴포넌트가 이 키를 먼저 찾고, 없으면 슬롯 키로 떨어진다 — 단검 D: 1번째(망토) 모션 없음 · 2번째 = 찌르기
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_Q_Recast);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_W_Recast);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_E_Recast);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_R_Recast);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_D_Recast);
	// 판정 순간 애니 (동작표 키 · F12.6-04 멧돼지 차징 → 돌진). 줄이 없으면 안 튼다
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_Q_Execute);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_W_Execute);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_E_Execute);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_R_Execute);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_D_Execute);

	// ── 쿨다운 (슬롯별) ────────────────────────────────────────
	// ⭐ 쿨다운 GE 는 UERCooldownEffect **하나**다. 어느 슬롯의 쿨인지는 이 태그가 말한다 —
	//   어빌리티가 ApplyCooldown 에서 DynamicGrantedTags 로 심고, GetCooldownTags 로 돌려준다.
	//   GE 애셋의 GrantedTags 가 아니라서 GE 를 슬롯마다 만들 필요가 없다.
	//   근거: Docs/4_Argument/16_쿨다운_가속환산_위치.md
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Slot_P);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Slot_Q);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Slot_W);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Slot_E);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Slot_R);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Slot_D);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Slot_Attack);   // 평타 간격 = 1 / AttackSpeed

	// ── 무기별 D 쿨다운 (F11-04 · Docs/4_Argument/25 방안 B) ─────
	// D 는 슬롯 태그 대신 이 태그로 쿨다운을 건다 (UERSkillData.CooldownTagOverride) — 무기를 바꿔도 각자 보존. 초기 8계열.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Weapon_Hammer);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Weapon_Bat);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Weapon_Axe);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Weapon_Dagger);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Weapon_Shuriken);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Weapon_SniperRifle);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Weapon_Pistol);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Weapon_Throw);

	// ── 리캐스트 윈도우 (F07-07) ───────────────────────────────
	// UERRecastWindowEffect 가 동적 부여. 있으면 그 슬롯은 쿨다운 중에도 발동된다 (재키 Q "적중 시 3초 내 재사용").
	//   근거: Docs/4_Argument/19_평타_다음평타강화_리캐스트_구조.md ③A
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Recast_Slot_P);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Recast_Slot_Q);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Recast_Slot_W);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Recast_Slot_E);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Recast_Slot_R);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Recast_Slot_D);

	// ── 피해 채널 ──────────────────────────────────────────────
	// 방어력·치명타 적용 여부가 이 셋으로 갈린다. 데미지 GE 에 반드시 하나가 붙어야 한다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_BasicAttack); // 방어력 O / 치명타 O
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Skill);       // 방어력 O / 치명타 X
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_True);        // 방어력 X / 치명타 X
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Secondary);        // 부가 피해 (도트 틱 · 적중 시 추가 피해) — 적중 이벤트를 안 보낸다. 안 막으면 출혈이 출혈을 영원히 갱신 (Argument 65)

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

	// ⭐ 쿨다운 길이(초). **스킬 가속이 이미 반영된 최종값**이다 —
	//   UERGameplayAbility::ApplyCooldown 이 계산해서 넣는다. GE 는 받기만 한다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Cooldown);

	// ⭐ 코스트 차감량(양수). UERVPCostEffect · UERHPCostEffect 가 받아 -값으로 뺀다.
	//   HP 하한(1)은 UERGameplayAbility::ApplyCost 가 미리 깎아서 넣는다 — 어트리뷰트셋은 모른다 (F02-04).
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Cost);

	// 스킬 페이즈(선딜·후딜) GE 의 길이(초). UERGameplayAbility 가 SkillData 의 CastTime/RecoveryTime 을 넣는다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_PhaseDuration);

	// 상태 GE(다음 평타 강화 · 리캐스트 윈도우)의 길이(초). 0 이하면 어빌리티가 Infinite 클래스를 쓴다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_StateDuration);

	// 잃은 체력 비례 스탯 (시셀라 P) — 최소(체력 100%) · 최대(체력 0%). MMC 가 읽는다 (UERLostHPStatCalc)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_LostHP_RegenMin);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_LostHP_RegenMax);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_LostHP_SkillAmpMin);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_LostHP_SkillAmpMax);

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
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_OnHitMagnitude); // 적중 효과 · 자기 버프 GE 의 범용 크기 (F11-05). 의미는 GE 가 정한다 — 망치 D: 방어력 배율 0.9
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Charges);        // 기본 공격 N회로 소비되는 자기 버프의 남은 횟수 (F11-05 B 권총 D)
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
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage_HPFloor);      // 대상 체력 하한 — 이 아래로 안 깎는다 (시셀라 R 자해 100 · 자해 스펙에만)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage_Multiplier);   // 최종 피해 배율 (없으면 1) — 조건부 증가 (재키 Q 출혈 최대 대상 +30% → 1.3)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage_TargetLostHPScaleMax);   // **대상이 잃은** 체력 비율 × 이 값 만큼 최종 피해 증가 (저격총 데드아이 "잃은 체력 비례 최대 200%" = 1.0)

	// ── 판정 형상 · 액터 유형 ──────────────────────────────
	//
	// 흡혈의 치유 감소 조건이다. ⭐ Execution 이 Cast<> 로 클래스를 검사하지 않는다 -
	// 그러면 F03(데미지)이 F12(야생동물)에 의존하게 된다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Shape_AoE);   // 어빌리티가 GE Spec 에 붙인다
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Actor_Type_Wildlife); // 야생동물 액터가 갖는다
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Actor_Type_Boss);     // 보스 액터가 갖는다 (F12 책임)

	// ── 연출 키 (F12.5 · Argument 39) ──────────────────────
	// ⭐ 스킬 애니 키는 **슬롯 태그 그대로** (Ability.Slot.*). 여기는 스킬이 아닌 동작만.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Anim_Dance);   // 춤 — 스킨마다 다를 수 있다 (사용자 2026-09-24)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Anim_Death);   // 사망 (04 에서 재생)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Anim_Gather);  // 채집 — 공용 collect (04 · Argument 46)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Prop_Bike);    // 소품 — 매그너스 R 바이크 (스킨 DA Props 키 · 발사 바이크가 이 키로 찾는다 · Argument 64 B1)

	// 소리 키 (F12.5-05 · Argument 49) — 동작표 줄의 키. 큐가 시전자의 연출 컴포넌트에서 이 키로 찾는다 (39 ④ K2)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_Attack);     // 평타 공격음 (휘두름 · 총성)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_Hit);        // 평타 타격음
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillCast);  // 스킬 시전음 (D · Q~R)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillHit);   // 스킬 타격음
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_Die);        // 사망음 (야생동물 — 사망 포즈와 같이 · 복제 상태 bDead 가 신호)
	// 야생동물 상태 사건 (F12.6-01 · 복제 상태 PresState 가 신호)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Anim_Appear);    // 등장 appear
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Anim_EndBattle); // 전투 끝 endbattle — 자리 도착 때
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_Appear);     // 등장음 (보스만 원본에 있다)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_Discover);   // 발견음 — 대기 → 전투 (<종>WakeUp_Start · ready_bear/wolf)
	// 경계 · 잠 (F12.6-02) — 상태라 AnimBP 가 튼다 (연출 컴포넌트가 애니를 넘긴다)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Anim_BewareStart);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Anim_BewareLoop);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Anim_BewareEnd);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Anim_SleepStart);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Anim_SleepLoop);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Anim_Wake);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_Beware);
	// 스킬별 시전음 · 타격음 (F12.6-05 · F19) — 있으면 공통 SkillCast/SkillHit 보다 먼저
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillCast_Q);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillCast_W);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillCast_E);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillCast_R);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillCast_D);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillHit_Q);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillHit_W);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillHit_E);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillHit_R);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillHit_D);     // 경계 들어갈 때 (<종>WakeUp_Ing — 뜻 (미확인) · 사용자가 들어보고 정한다)
	// F19-01 K8 — 다음 평타 강화 (카티야 P Reinforce_*) · 순차 사격 발 사이 조준 (카티야 R Aiming_02 · _03). 없으면 평소 키로 떨어진다
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_AttackEnhanced);  // 강화를 소비하는 평타의 공격음 (평소 Attack 대신)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_HitEnhanced);     // 강화 평타 타격음 (평소 Hit 대신)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_EnhanceReady);    // 강화가 걸린 순간
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillAim_R);      // R 다음 발 조준 (발 번호로 고른다)
	// F19-02 매그너스 소리 (Docs/3_EditorTasks/Audio/Magnus.md) — 지금 필요한 슬롯만
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillHit_P);       // 패시브가 대상에게 무언가를 걸 때 (재키 출혈 · Audio/Jackie.md 5)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillLoop_P);      // 패시브 상태 동안 반복 (재키 아드레날린 · State.Adrenaline 동안)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillLoopStart_P); // 그 상태가 시작될 때 한 번 (재키 아드레날린 시작)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillLand_E);      // E 착지 순간 (재키 Skill03_Bump — 시전음은 뛰어오를 때 · Audio/Jackie.md 17 · 18)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillHitLate_Q);   // Q 타격음 뒤 조금 늦게 한 번 더 (매그너스 Skill01_Impact)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillLoop_W);      // W 도는 동안 반복 (State.AnimHold 동안 · 매그너스 Skill02_Attack)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillLoop_R);      // R 탄 동안 반복 (State.Riding 동안 · 매그너스 Skill04_Drive)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillLoopStart_R); // R 반복이 시작될 때 한 번 (시동 · 매그너스 Skill04_GoActive)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillRecast_R);
	// 시셀라 (Audio/Sissela.md · 사용자 2026-10-06) — 소리 큐(GameplayCue.Pres.Sfx)로 바로 튼다
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_Join);             // 윌슨과 합칠 때 (Passive_Union)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillMove_Q);      // Q 윌슨이 날기 시작할 때 한 번 (Skill01_Move)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillLand_Q);      // Q 착지 폭발 (Skill01_Hit2 · 맞힌 사람 없어도)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillBurst_W);     // W 터짐 (Skill02_End · 맞힌 사람 없어도)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillStun_E);      // E 적 적중 — 기절 (Skill03_Stun · 타격음과 겹쳐도 됨)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillPull_E);      // E 끌어올 때 (Skill03_Take — 적 · 시셀라)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillShield_E);    // E 시셀라 적중 — 보호막 (Skill03_Shield)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillCount_R);     // R 카운트 (Skill04_Count · 한 번)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Sfx_SkillLand_R);      // R 늦춘 판정 순간 폭발 (Skill04_Explosion · 착지 큐의 R 몫)    // R 재사용의 공격음 (평소 SkillCast.R 대신 · 매그너스 Skill04_Attack 바이크 발사)

	// 연출 큐 (F12.5-05 · Argument 49) — 네트워크 사건. 무엇을 틀지는 키가 정한다 (큐 ≠ 키)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Pres_Attack);   // 시전자 — 공격이 나갔다 (판정 시점)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Pres_Hit);      // 대상 — 맞았다 (서버 확정)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Pres_Aim);      // 시전자 — 순차 사격 다음 발 조준 시작 (발 번호 · K8)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Pres_Land);     // 시전자 — 늦춘 판정 순간 (도약 착지 · 재키 E · JudgeDelay)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Pres_Ready);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Pres_Sfx);      // 시전자 — 소리 키 하나를 그대로 (키 = AggregatedSourceTags 의 Pres.Sfx.* · 위치 = Location · UERPresentationComponent::SendSfxCue)    // 시전자 — 다음 평타 강화가 걸렸다 (K8)
	// 모드 상태 애니 (Argument 42 ⑥ A2) — 모드 칸이 있는 줄에만 쓴다. C++ 가 해석해 AnimInstance 에 넘기고 상태머신이 튼다
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Anim_AttackEnhanced);   // 강화를 소비하는 평타 (재키 W skill02_attack) — 없으면 평소 평타 (Argument 66 E1)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Anim_ModeStart);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Anim_ModeIdle);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Anim_ModeRun);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pres_Anim_ModeEnd);

	// ── 연출 모드 (Argument 42) ── 서버가 복제 loose 태그로 붙인다. 각 머신의 연출 컴포넌트가 부모(Mode)를 구독해 모드 세트를 고른다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mode);              // 부모 — 구독용
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mode_Sniper);       // 저격총 D (카티야)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mode_Chainsaw);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mode_Bubble);   // 시셀라 W 감싸기 1.5초 — 감싸기 GE 에 같이 (Argument 69 B1)     // 전기톱 살인마 R (재키) — R 버프 GE 의 동적 태그 = "R 중" · 처치 +5초도 이 태그로 (Argument 66 M1)
}
