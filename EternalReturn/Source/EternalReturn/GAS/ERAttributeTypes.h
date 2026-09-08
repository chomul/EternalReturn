// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ERAttributeTypes.generated.h"

/**
 * 실험체 1레벨 기본 스탯.
 *
 * UERCharacterData 애셋이 이걸 인라인으로 하나 들고 있다 - 데이터 테이블 행이 아니다.
 * 근거: Docs/4_Argument/4_스탯데이터_저장방식.md
 *
 * 값들은 Instant GameplayEffect 의 SetByCaller 로 들어간다 - 직접 Set 하지 않는다.
 * 근거: Docs/4_Argument/3_어트리뷰트셋_구조.md C 절
 *
 * ⚠ 필드가 31개다. 어트리뷰트는 33개인데 HP / VP 가 여기 없다 -
 *   시작 체력·기력은 **항상 최대치**라 MaxHP / MaxVP 에서 가져온다.
 *   따로 두면 최대치와 어긋난 값이 들어갈 수 있다 (같은 문서 C-4 절).
 *
 * ⚠ **필드 이름을 바꾸지 마라.** 애셋에 저장된 값이 날아간다.
 *   그리고 이름이 곧 SetByCaller 키라서, 바꾸면 GE 애셋의 모디파이어도 같이 끊긴다.
 *
 * ⚠ **필드는 손으로 쓴다.** UnrealHeaderTool 은 사용자 매크로를 펼치지 않아서
 *   매크로로 만든 UPROPERTY 는 리플렉션에 등록되지 않는다.
 *
 * 레벨 성장은 F10 이 담당한다. 여기는 **Lv.1 값만** 둔다.
 */
USTRUCT(BlueprintType)
struct FERCharStats
{
	GENERATED_BODY()

	// ── 기초 ────────────────────────────────────────────────

	/** 최대 체력. 시작 체력도 이 값이 된다. */
	UPROPERTY(EditDefaultsOnly, Category = "기초")
	float MaxHP = 0.f;

	/** 초당 체력 회복. */
	UPROPERTY(EditDefaultsOnly, Category = "기초")
	float HPRegen = 0.f;

	/** 최대 기력. 시작 기력도 이 값이 된다. */
	UPROPERTY(EditDefaultsOnly, Category = "기초")
	float MaxVP = 0.f;

	/** 초당 기력 회복. */
	UPROPERTY(EditDefaultsOnly, Category = "기초")
	float VPRegen = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "기초")
	float AttackPower = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "기초")
	float Defense = 0.f;

	/** 초당 공격 횟수. 상한은 ER Stat Caps 의 MaxAttackSpeed. */
	UPROPERTY(EditDefaultsOnly, Category = "기초")
	float AttackSpeed = 0.f;

	/** 단위는 m/s. ⚠ 0 을 넣으면 하한(MinMoveSpeed)으로 잘린다. */
	UPROPERTY(EditDefaultsOnly, Category = "기초")
	float MoveSpeed = 0.f;

	/** 시야 반경(m). 팀 시야 시스템이 읽는다. */
	UPROPERTY(EditDefaultsOnly, Category = "기초")
	float Sight = 0.f;

	/** 기본 공격 사거리(m). ⚠ 무기 계열이 덮어쓴다 - 여기는 맨몸 값이다. */
	UPROPERTY(EditDefaultsOnly, Category = "기초")
	float AttackRange = 0.f;

	// ── 공격 파생 ───────────────────────────────────────────

	/** 0~1. ⚠ 치명타는 기본 공격 채널에만 붙는다. 스킬에는 안 붙는다. */
	UPROPERTY(EditDefaultsOnly, Category = "공격 파생")
	float CritChance = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "공격 파생")
	float CritDamageUp = 0.f;

	/** 스킬 증폭. */
	UPROPERTY(EditDefaultsOnly, Category = "공격 파생")
	float SkillAmp = 0.f;

	/** 기본 공격 증폭. */
	UPROPERTY(EditDefaultsOnly, Category = "공격 파생")
	float BasicAtkAmp = 0.f;

	/** 0~1. ⚠ 퍼센트 관통을 먼저, 고정 관통을 나중에 적용한다. */
	UPROPERTY(EditDefaultsOnly, Category = "공격 파생")
	float DefPenPercent = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "공격 파생")
	float DefPenFlat = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "공격 파생")
	float DamageUp = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "공격 파생")
	float FinalDamageUpPercent = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "공격 파생")
	float FinalDamageUpFlat = 0.f;

	/** 스킬 가속. ⚠ 상한이 없다 - 원작 확인값이다. */
	UPROPERTY(EditDefaultsOnly, Category = "공격 파생")
	float SkillHaste = 0.f;

	// ── 방어 파생 ───────────────────────────────────────────

	/** 0~1. ⚠ 1.0 을 넘기면 맞을 때마다 체력이 찬다. 캡이 걸린다. */
	UPROPERTY(EditDefaultsOnly, Category = "방어 파생")
	float DamageDown = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "방어 파생")
	float BasicAtkDamageDown = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "방어 파생")
	float SkillDamageDown = 0.f;

	/** 0~1. 둔화 저항. */
	UPROPERTY(EditDefaultsOnly, Category = "방어 파생")
	float SlowResist = 0.f;

	/** 0~1. 행동 불가 저항. */
	UPROPERTY(EditDefaultsOnly, Category = "방어 파생")
	float CCResist = 0.f;

	// ── 유지력 ──────────────────────────────────────────────

	/** 기본 공격 흡혈. */
	UPROPERTY(EditDefaultsOnly, Category = "유지력")
	float Lifesteal = 0.f;

	/** 모든 피해 흡혈. */
	UPROPERTY(EditDefaultsOnly, Category = "유지력")
	float OmniLifesteal = 0.f;

	/** 받는 회복량 증폭. */
	UPROPERTY(EditDefaultsOnly, Category = "유지력")
	float HealAmp = 0.f;

	/** 비전투 시 추가 회복. */
	UPROPERTY(EditDefaultsOnly, Category = "유지력")
	float OutOfCombatRegen = 0.f;

	// ── 모드 보정 ───────────────────────────────────────────

	/** 특정 모드에서만 붙는 피해 증가. 보통 0. */
	UPROPERTY(EditDefaultsOnly, Category = "모드 보정")
	float ModeDamageUp = 0.f;

	/** 특정 모드에서만 붙는 피해 감소. 보통 0. */
	UPROPERTY(EditDefaultsOnly, Category = "모드 보정")
	float ModeDamageDown = 0.f;
};
