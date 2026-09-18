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

/**
 * 레벨당 스탯 증가 (F10-02). 성장이 **선형**이라 (역기획서 §2.4 실측) `기본값 + 레벨당 × (Lv−1)` — 20행 테이블이 없다.
 *
 * ⭐ **필드가 4개뿐인 이유**: 이동 속도 · 공격 속도 · 나머지는 레벨로 안 오른다 (§2.4). 필드가 없으면 실수로도 못 올린다.
 * ⭐⭐ 레벨업마다 **Instant** GE 로 BaseValue 에 더한다 — 장비(Infinite · CurrentValue)와 반대. Infinite 로 넣으면
 *   레벨분이 전부 "추가 공격력" 으로 잡혀 BonusAPRatio 스킬이 폭주한다 (Docs/4_Argument/5_추가공격력_산출방식.md).
 * ⚠ 실험체마다 다르다 (재키 95 / 4.7 / 3 / 0.077 · 아야 76 / 4.1 / 2.3 / 0.06) — 개별 밸런싱 파라미터라 실험체 애셋에 둔다.
 */
USTRUCT(BlueprintType)
struct FERCharStatGrowth
{
	GENERATED_BODY()

	/** 레벨당 최대 체력. 현재 체력도 같은 양만큼 오른다 (자체 결정값 — LoL 관례, 원작 (미확인)). */
	UPROPERTY(EditDefaultsOnly, Category = "성장")
	float MaxHPPerLevel = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "성장")
	float AttackPowerPerLevel = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "성장")
	float DefensePerLevel = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "성장")
	float HPRegenPerLevel = 0.f;
};

/** 무기 숙련도가 올리는 증폭의 종류 (F10-04). ⭐ 둘 중 하나 — "둘 다 두고 한쪽 0" 은 데이터에서 금지 (역기획서 §8). */
UENUM(BlueprintType)
enum class EERAmpType : uint8
{
	/** 기본 공격 증폭 (BasicAtkAmp). 재키 단검 2.4 · 양손검 2.2 · 도끼 2.1 [확인] */
	BasicAttack,
	/** 스킬 증폭 (SkillAmp). 레니 권총 4.4 · 시셀라 암기 4.0 [확인] */
	Skill,
};

/**
 * 실험체 × 무기군 하나의 숙련도 증폭 계수 (F10-04). UERCharacterData.WeaponProficiencyAmp 의 값 (Docs/4_Argument/23).
 * ⚠ 미확인 조합은 "스킬형 4%대 / 평타형 2%대" 출발값 `[자체]` (역기획서 §3.2 역산).
 */
USTRUCT(BlueprintType)
struct FERWeaponAmp
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	EERAmpType AmpType = EERAmpType::BasicAttack;

	/** 숙련도 레벨당 증폭 (%). 2.4 = 레벨당 +2.4%. 적용값 = AmpPerLevel × 레벨 (Lv.1 부터 1단 — 자체 결정값, 원작 (미확인)). */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float AmpPerLevel = 0.f;
};
