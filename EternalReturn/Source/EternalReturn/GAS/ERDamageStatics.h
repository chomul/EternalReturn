// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "GAS/ERAttributeSet.h"

/**
 * 데미지 Execution 이 읽을 어트리뷰트 캡처 정의.
 *
 * ⭐ 이게 자체 구현의 FDamageContext 를 대체한다. 호출부(스킬)는 아무것도 채우지 않는다 -
 *   캡처 정의만 선언하면 엔진이 공격자·피격자에서 값을 가져온다.
 *   근거: Docs/1_Task/F03_데미지_ExecutionCalc/01_어트리뷰트_캡처정의.md
 *
 * 목록은 계산식에서 역산했다: Docs/0_GameDesign/Systems/스탯_데미지공식_역기획서.md §2.2
 *
 * ⚠ **Source / Target 을 뒤집으면 자기 방어력으로 자기 피해를 깎는다.**
 *   증상이 "피해가 이상하다" 정도로만 나와서 찾기 어렵다. 각 항목에 근거 단계를 적어 둔다.
 *
 * ⚠ IncomingDamage 는 캡처하지 않는다. **출력 대상이지 입력이 아니다.**
 */
struct FERDamageStatics
{
	// ── Source (공격자) · 스냅샷 ────────────────────────────
	//
	// 스냅샷 = GE Spec 을 만든 시점의 값. 투사체가 날아가는 동안 공격자가
	// 장비를 바꿔도 시전 시점 값을 쓴다.
	// 근거: 매그너스 W "타격 횟수 = 11 + floor(추가방어력/35), **시전 시점 확정**"
	//       (Docs/0_GameDesign/Systems/스킬_프레임워크_역기획서.md §8)

	DECLARE_ATTRIBUTE_CAPTUREDEF(AttackPower)          // 3 계수 / BasicAttack 기본값
	DECLARE_ATTRIBUTE_CAPTUREDEF(SkillAmp)             // 3 계수
	DECLARE_ATTRIBUTE_CAPTUREDEF(DefPenPercent)        // 4 관통 (% 가 먼저)
	DECLARE_ATTRIBUTE_CAPTUREDEF(DefPenFlat)           // 4 관통 (고정이 나중)
	DECLARE_ATTRIBUTE_CAPTUREDEF(CritChance)           // 6 치명타 - 기본공격 채널 전용
	DECLARE_ATTRIBUTE_CAPTUREDEF(CritDamageUp)         // 6 치명타 배율
	DECLARE_ATTRIBUTE_CAPTUREDEF(DamageUp)             // 7 공통 증감
	DECLARE_ATTRIBUTE_CAPTUREDEF(BasicAtkAmp)          // 8 채널별 증감
	// ⚠ 「최종 피해 추가(%)」는 **캡처는 하되 적용을 보류**한다.
	//   원본 표기가 `x 최종 피해 추가(%)` 라 (1+x) 인지 x 배인지 해석이 안 됐다.
	//   근거: Docs/0_GameDesign/Systems/스탯_데미지공식_역기획서.md §8 (미확인)
	//   캡처를 미리 해 두는 이유는, 해석이 정해졌을 때 Execution 만 고치면 되게 하려는 것이다.
	DECLARE_ATTRIBUTE_CAPTUREDEF(FinalDamageUpPercent) // 9 최종 피해 추가
	DECLARE_ATTRIBUTE_CAPTUREDEF(FinalDamageUpFlat)    // 9 최종 피해 추가
	DECLARE_ATTRIBUTE_CAPTUREDEF(ModeDamageUp)         // 1·10 모드 보정
	DECLARE_ATTRIBUTE_CAPTUREDEF(Lifesteal)            // F03-05 흡혈
	DECLARE_ATTRIBUTE_CAPTUREDEF(OmniLifesteal)        // F03-05 흡혈

	// ── Target (피격자) · 스냅샷 아님 ───────────────────────
	//
	// ⭐ Target 을 스냅샷으로 하면 **방어 아이템을 낀 게 반영되지 않는다.**
	//   맞는 순간의 값이어야 한다.

	DECLARE_ATTRIBUTE_CAPTUREDEF(Defense)              // 4 관통 대상 / 5 감산
	DECLARE_ATTRIBUTE_CAPTUREDEF(DamageDown)           // 7 공통 증감
	DECLARE_ATTRIBUTE_CAPTUREDEF(BasicAtkDamageDown)   // 8 채널별 증감
	DECLARE_ATTRIBUTE_CAPTUREDEF(SkillDamageDown)      // 8 채널별 증감
	DECLARE_ATTRIBUTE_CAPTUREDEF(ModeDamageDown)       // 1·10 모드 보정

	// ── 양쪽에서 쓰는 것 ────────────────────────────────────
	//
	// ⚠ HP / MaxHP 는 **공격자와 피격자 양쪽**에서 필요하다.
	//   DECLARE_ATTRIBUTE_CAPTUREDEF(HP) 는 멤버 이름이 HPDef 로 고정이라
	//   같은 어트리뷰트를 두 번 선언할 수 없다. 그래서 이 넷만 손으로 만든다.
	//   (Lyra 도 이 형태를 쓴다 - LyraDamageExecution.cpp:19)
	//
	// 이름에 Source/Target 을 박아 둔다. 매크로 쪽은 그룹 주석으로 구분되지만
	// 이쪽은 둘이 나란히 있어서 이름으로 구분되지 않으면 반드시 헷갈린다.

	FGameplayEffectAttributeCaptureDefinition SourceHPDef;     // 3-b GetLostHP(A)
	FGameplayEffectAttributeCaptureDefinition SourceMaxHPDef;  // 3-b GetLostHP(A)
	FGameplayEffectAttributeCaptureDefinition TargetHPDef;     // 3-b 현재 체력 비례
	FGameplayEffectAttributeCaptureDefinition TargetMaxHPDef;  // 3-b 최대 체력 비례

	// ── 일부러 캡처하지 않은 것 ─────────────────────────────
	//
	// VP(기력)  : 계산식 §2.2 에 등장하지 않는다. 선행 구현 6인의 스킬 코스트가 전부 0 이라
	//             필요 여부를 확인할 방법이 없다. **(미확인)** - 코스트를 쓰는 실험체가 오면 다시 본다.
	// AttackRange / MoveSpeed / Sight / AttackSpeed / SkillHaste / HealAmp
	//           : 피해량 계산에 안 쓰인다. 각각 사거리·이동·시야·평타 주기·쿨다운·회복 소관이다.
	// HPRegen / VPRegen / OutOfCombatRegen : 회복 소관 (F15).
	// SlowResist / CCResist : CC 소관 (F06).
	// IncomingDamage : ⭐ **출력 대상이다.** 입력으로 캡처하면 안 된다.

	/**
	 * 캡처 정의 전부. Execution 생성자가 RelevantAttributesToCapture 에 그대로 붙인다.
	 *
	 * ⭐ 여기 빠지면 그 값은 **경고 없이 0 으로 읽힌다.**
	 *   그래서 생성자가 정의와 등록을 한 번에 하도록 묶어 뒀다 (아래 ER_CAPTURE).
	 */
	TArray<FGameplayEffectAttributeCaptureDefinition> AllDefs;

	FERDamageStatics()
	{
		// 정의와 등록을 한 줄로 묶는다. 둘을 따로 쓰면 언젠가 등록을 빠뜨린다.
#define ER_CAPTURE(PropertyName, CaptureSource, bSnapshot) \
		{ \
			DEFINE_ATTRIBUTE_CAPTUREDEF(UERAttributeSet, PropertyName, CaptureSource, bSnapshot); \
			AllDefs.Add(PropertyName##Def); \
		}

		// Source — 전부 스냅샷
		ER_CAPTURE(AttackPower,          Source, true);
		ER_CAPTURE(SkillAmp,             Source, true);
		ER_CAPTURE(DefPenPercent,        Source, true);
		ER_CAPTURE(DefPenFlat,           Source, true);
		ER_CAPTURE(CritChance,           Source, true);
		ER_CAPTURE(CritDamageUp,         Source, true);
		ER_CAPTURE(DamageUp,             Source, true);
		ER_CAPTURE(BasicAtkAmp,          Source, true);
		ER_CAPTURE(FinalDamageUpPercent, Source, true);
		ER_CAPTURE(FinalDamageUpFlat,    Source, true);
		ER_CAPTURE(ModeDamageUp,         Source, true);
		ER_CAPTURE(Lifesteal,            Source, true);
		ER_CAPTURE(OmniLifesteal,        Source, true);

		// Target — 전부 실시간
		ER_CAPTURE(Defense,              Target, false);
		ER_CAPTURE(DamageDown,           Target, false);
		ER_CAPTURE(BasicAtkDamageDown,   Target, false);
		ER_CAPTURE(SkillDamageDown,      Target, false);
		ER_CAPTURE(ModeDamageDown,       Target, false);

#undef ER_CAPTURE

		// 양쪽에서 쓰는 넷 - 이름이 겹쳐 매크로를 못 쓴다
		SourceHPDef = FGameplayEffectAttributeCaptureDefinition(
			UERAttributeSet::GetHPAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);
		SourceMaxHPDef = FGameplayEffectAttributeCaptureDefinition(
			UERAttributeSet::GetMaxHPAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);
		TargetHPDef = FGameplayEffectAttributeCaptureDefinition(
			UERAttributeSet::GetHPAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
		TargetMaxHPDef = FGameplayEffectAttributeCaptureDefinition(
			UERAttributeSet::GetMaxHPAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);

		AllDefs.Add(SourceHPDef);
		AllDefs.Add(SourceMaxHPDef);
		AllDefs.Add(TargetHPDef);
		AllDefs.Add(TargetMaxHPDef);
	}
};

/**
 * 캡처 정의 싱글턴. 첫 호출 때 한 번만 만들어진다.
 *
 * 정의를 .cpp 에 두는 이유는 두 가지다:
 *   1. 인스턴스가 번역 단위마다 생기지 않는다
 *   2. ⭐ .cpp 가 있어야 이 헤더가 **실제로 컴파일된다.** 헤더만 있으면
 *      아무도 include 하지 않는 동안 오류가 드러나지 않는다
 */
const FERDamageStatics& ERDamageStatics();
