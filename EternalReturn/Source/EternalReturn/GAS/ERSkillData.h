// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Combat/ERTargetingTypes.h"
#include "ERSkillData.generated.h"

class UAbilitySystemComponent;
class UERGameplayAbility;
class UGameplayEffect;

/**
 * 스킬 하나의 데이터. **로직(어빌리티 클래스)과 수치를 여기서 묶는다.**
 *
 * ⭐ **어빌리티 = 로직, 이 애셋 = 수치.** 같은 어빌리티 클래스(예: 투사체)를
 *   여러 캐릭터가 이 애셋만 바꿔 공유한다 — Lyra 의 RangedWeapon / WeaponInstance 와 같은 구조.
 *
 * ⭐ **캐릭터도 무기도 이 애셋을 든다.** P/Q/W/E/R 은 UERCharacterData 가,
 *   D 는 무기 데이터(F11)가 갖는다. 역기획서 §1.2 — "D 스킬의 데이터 출처는 실험체 테이블이 아니다".
 *
 * ⚠ **필드는 필요한 기능이 생길 때 붙인다.** 슬롯·클래스(F07-01) + 쿨다운(F07-02) + 코스트·레벨 규칙(F07-03).
 *   판정 계수는 F07-05 에서 추가한다.
 *   ⚠ 역기획서 §2.2 — 수치는 전부 **레벨별 TArray<float>** 여야 한다. 스칼라 금지.
 *   (레니 E 는 쿨 9초 고정, 시셀라 E 는 0.5초 단위라 "기본값 + 레벨당 증가" 로는 표현이 안 된다)
 *
 * 근거: Docs/4_Argument/15_스킬데이터_위치.md (방안 B)
 */
/** 스킬이 무엇을 지불하는가. 역기획서 §3 — 마나는 없다. 기력(VP) 아니면 체력(시셀라). */
UENUM()
enum class ESkillCostType : uint8
{
	None,
	VP,
	HP,
};

/** 피해 채널. 기본 공격은 별도(평타 어빌리티). ERDamageExecution 이 태그로 방어력·치명타 적용을 가른다 (F03). */
UENUM()
enum class ESkillDamageType : uint8
{
	/** 피해 없음 (카티야 W 같은 유틸리티 · 자기 이동) */
	None,
	/** 방어력 O / 치명타 X */
	Skill,
	/** 방어력 O / 치명타 O — 평타 (UERBasicAttackAbility). 흡혈(Lifesteal)은 이 채널만 */
	BasicAttack,
	/** 고정 피해(True damage) — 방어력 X / 치명타 X. UHT 가 "True" 라는 이름을 금지해서 Fixed */
	Fixed,
};

/** 시전자 자기 이동 (F07-06). ERForcedMove::ApplySelfMove 로 실행 — 넉백과 같은 RootMotionSource 경로. */
UENUM()
enum class ESkillSelfMove : uint8
{
	None,
	/** 조준 방향으로 SelfMoveDistance — 재키 Q · 다니엘 E (돌진) */
	TowardAim,
	/** ⚠ 조준 **반대**로 SelfMoveDistance — 카티야 E (백스텝). 뒤집는 것을 빠뜨리기 쉽다 */
	AwayFromAim,
	/** 클램프된 조준점까지 (거리 = 조준점까지, SelfMoveDistance 무시) — 재키 E (위치 지정 도약) */
	ToAimPoint,
};

/**
 * 판정 형상 — F04 `FTargetQuery` 중 **디자이너가 정하는 필드만**. 단위는 F04 와 같이 **미터**.
 * 런타임 값(Origin · Direction · Instigator · DesignatedTarget)은 어빌리티가 조준 데이터로 채운다.
 * ⚠ 레벨업해도 안 변한다 — 사거리·반경·속도는 6인 전 스킬에서 레벨별 값이 확인되지 않음 (역기획서 §2.2).
 */
USTRUCT()
struct FERSkillShape
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	ESkillTargeting Shape = ESkillTargeting::SelfRadius;

	UPROPERTY(EditDefaultsOnly)
	ETargetTeamFilter TeamFilter = ETargetTeamFilter::Enemy;

	/** 사거리 상한(m). 조준점이 이보다 멀면 서버가 이 거리로 당긴다 (사거리 핵 방지). SelfRadius 는 반경. */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float RangeMax = 3.f;

	/** 사거리 하한(m). 시셀라 Q 는 1m 하한이 있다. 0 = 없음. */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float RangeMin = 0.f;

	/** GroundCircle 반경 · DualRadius 외곽 반경 (m) */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float RadiusOuter = 0.f;

	/** DualRadius 중앙 반경 (m) — 레니 W 1.25 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float RadiusInner = 0.f;

	/** Cone 전체 각도 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", ClampMax = "360"))
	float AngleDeg = 0.f;

	/** Projectile 굵기 (m) */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float ProjectileRadius = 0.25f;

	/** Projectile 관통. 관통이면 광역으로 본다 (아래 IsAoE). */
	UPROPERTY(EditDefaultsOnly)
	bool bPenetrate = false;

	/**
	 * SingleTarget 조준 보조 (m). 커서 아래에 유효한 대상이 없으면 조준점 반경 안에서 **가장 가까운 대상**을 대신 잡는다.
	 * 0 = 끔 (캡슐을 정확히 찍어야 함). 평타 · 단일 대상 스킬의 조작감용 — 판정 자체가 아니라 **대상 선택**만 돕는다.
	 * ⚠ 자체 결정값. 원작의 클릭 허용 오차는 (미확인).
	 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", EditCondition = "Shape == ESkillTargeting::SingleTarget"))
	float AimAssistRadius = 0.f;

	/**
	 * 광역인가 — 흡혈 치유 감소 조건 (F03-05, `Damage.Shape.AoE`).
	 * ⭐ 자체 결정값: SingleTarget 과 **비관통** Projectile 만 단일, 나머지는 광역.
	 */
	bool IsAoE() const;
};

UCLASS(BlueprintType, Const)
class ETERNALRETURN_API UERSkillData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 어느 슬롯인가. Ability.Slot.* 중 하나. 입력이 이 태그로 어빌리티를 찾는다. */
	UPROPERTY(EditDefaultsOnly, Category = "슬롯", meta = (Categories = "Ability.Slot"))
	FGameplayTag SlotTag;

	/** 로직. ⭐ 여러 스킬이 같은 클래스를 써도 된다 — 수치는 이 애셋이 갖는다. */
	UPROPERTY(EditDefaultsOnly, Category = "슬롯")
	TSubclassOf<UERGameplayAbility> AbilityClass;

	/**
	 * 기본 쿨다운(초), **스킬 레벨별**. [0] 이 1레벨.
	 *
	 * ⭐ 비워 두면 쿨다운이 없다 (카티야 P · 다니엘 P).
	 * ⚠ 배열이다 — 레니 E 는 9초 고정, 시셀라 E 는 0.5초 단위라 "기본 + 레벨당" 으로 못 쓴다 (역기획서 §2.2).
	 *   레벨이 배열보다 크면 마지막 값을 쓴다 (GetCooldown).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "쿨다운", meta = (ClampMin = "0"))
	TArray<float> Cooldowns;

	/**
	 * 스킬 가속을 무시한다. 아야 P 의 "기본 공격마다 −1초(고정값)" 처럼 원작에 쿨감 미적용 쿨이 있다
	 * (스킬 프레임워크 역기획서 §4.3). 켜면 Cooldowns 값이 그대로 최종 쿨다운이다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "쿨다운")
	bool bIgnoreCooldownReduction = false;

	/** 해당 레벨의 기본 쿨다운. 배열이 비었으면 0. 레벨은 1부터. */
	float GetCooldown(int32 Level) const;

	// ── 코스트 ────────────────────────────────────────────────
	// 역기획서 §3: 6인 중 기력 소모량이 확인된 스킬이 없다. 체계만 두고 수치는 나중에 채운다.

	UPROPERTY(EditDefaultsOnly, Category = "코스트")
	ESkillCostType CostType = ESkillCostType::None;

	/** 레벨별 코스트. ⭐ 시셀라 W 는 [50/60/70/80/90] — 레벨업하면 **늘어난다** (§2.2). */
	UPROPERTY(EditDefaultsOnly, Category = "코스트", meta = (ClampMin = "0", EditCondition = "CostType != ESkillCostType::None"))
	TArray<float> Costs;

	/** 해당 레벨의 코스트. CostType 이 None 이거나 배열이 비었으면 0. */
	float GetCost(int32 Level) const;

	// ── 판정 · 피해 (F07-05) ───────────────────────────────────
	// ⭐ 어빌리티는 계수만 SetByCaller 로 넘긴다. 스탯을 읽어 곱하는 것은 ERDamageExecution 하나뿐이다
	//   (Docs/4_Argument/5_추가공격력_산출방식.md). 체력 비례 계수(최대/현재/잃은)는 그걸 쓰는 스킬이 생길 때 붙인다.

	UPROPERTY(EditDefaultsOnly, Category = "판정")
	FERSkillShape Shape;

	UPROPERTY(EditDefaultsOnly, Category = "피해")
	ESkillDamageType DamageType = ESkillDamageType::None;

	/** 레벨별 고정 피해. 카티야 Q [40/80/120/160/200] */
	UPROPERTY(EditDefaultsOnly, Category = "피해", meta = (EditCondition = "DamageType != ESkillDamageType::None"))
	TArray<float> BaseDamage;

	/** 레벨별 공격력 계수 (1.0 = 100%) */
	UPROPERTY(EditDefaultsOnly, Category = "피해", meta = (EditCondition = "DamageType != ESkillDamageType::None"))
	TArray<float> APRatio;

	/** 레벨별 **추가** 공격력 계수 — 재키 W 20~60% */
	UPROPERTY(EditDefaultsOnly, Category = "피해", meta = (EditCondition = "DamageType != ESkillDamageType::None"))
	TArray<float> BonusAPRatio;

	/** 레벨별 스킬 증폭 계수 */
	UPROPERTY(EditDefaultsOnly, Category = "피해", meta = (EditCondition = "DamageType != ESkillDamageType::None"))
	TArray<float> SkillAmpRatio;

	/** 레벨별 배열에서 값 하나. 비면 0, 레벨이 배열보다 크면 마지막 값. 레벨은 1부터. Cooldowns · Costs 도 이걸 쓴다. */
	static float LevelValue(const TArray<float>& Values, int32 Level);

	// ── 적중 시 CC (F07-07) ────────────────────────────────────
	// ApplySkillDamage 가 피해 직후 ERCC::ApplyCC 로 건다 (F06 그대로 — 저항 · 면역 · 둔화 재계산 전부 거기서).

	/** 적중한 대상에게 거는 CC GE 애셋 (GE_CC_Slow · GE_CC_Stun …). 비면 없음. */
	UPROPERTY(EditDefaultsOnly, Category = "적중 효과")
	TSubclassOf<UGameplayEffect> OnHitEffect;

	/** 레벨별 지속(초). 재키 W 0.85 · 매그너스 E 0.7~1.3 */
	UPROPERTY(EditDefaultsOnly, Category = "적중 효과", meta = (EditCondition = "OnHitEffect != nullptr"))
	TArray<float> OnHitDuration;

	/** 레벨별 둔화 감소율 (0.6 = 60%). 둔화 GE 일 때만 의미 있다. 재키 W 0.6~0.8 · 카티야 E 0.5~0.7 */
	UPROPERTY(EditDefaultsOnly, Category = "적중 효과", meta = (EditCondition = "OnHitEffect != nullptr"))
	TArray<float> OnHitSlowPercent;

	// ── 다음 기본 공격 강화 (F07-07) ──────────────────────────
	// 켜면 발동 시 자기에게 State.NextAttackBuff 를 건다. 다음 평타가 적중하면 **이 스킬의 피해·적중 효과**를 얹고 소비한다.
	// 카티야 P · 재키 W · 시셀라 Q · 권총 D — 4곳 공용 (역기획서 §8).

	UPROPERTY(EditDefaultsOnly, Category = "다음 평타 강화")
	bool bGrantsNextAttackBuff = false;

	/** 대기 만료(초). 0 = 만료 없음. 카티야 P 5 · 재키 W (미확인 → 0) */
	UPROPERTY(EditDefaultsOnly, Category = "다음 평타 강화", meta = (ClampMin = "0", EditCondition = "bGrantsNextAttackBuff"))
	float NextAttackBuffDuration = 0.f;

	// ── 리캐스트 윈도우 (F07-07) ──────────────────────────────
	// 적중 시 Recast.Slot.* 를 RecastWindow 초 동안 건다. 그동안 이 슬롯은 쿨다운을 무시하고 한 번 더 발동된다.
	// ⚠ 자체 결정값: 재발동은 쿨다운을 새로 걸지 않는다(첫 시전의 쿨이 그대로) · 코스트는 든다. 원작 (미확인).

	UPROPERTY(EditDefaultsOnly, Category = "리캐스트")
	bool bRecastOnHit = false;

	/** 윈도우(초). 재키 Q 3 */
	UPROPERTY(EditDefaultsOnly, Category = "리캐스트", meta = (ClampMin = "0.01", EditCondition = "bRecastOnHit"))
	float RecastWindow = 3.f;

	// ── 자기 이동 (F07-06) ─────────────────────────────────────
	// [4] 발동 시 판정(ExecuteSkill)보다 **먼저** 시작한다. 돌진 끝에 맞히는 스킬은 파생이 시점을 정한다.
	// ⚠ 자체 결정값: 이동 중 이동 입력은 RootMotion 이 덮고(엔진), 스킬 입력은 RecoveryTime 으로 막는다 →
	//   돌진 스킬은 RecoveryTime ≥ SelfMoveDuration 으로 **데이터에서** 맞춘다. 이동 중 CC 는 끊지 않는다 (§5.1 [4] 원자적).

	UPROPERTY(EditDefaultsOnly, Category = "자기 이동")
	ESkillSelfMove SelfMove = ESkillSelfMove::None;

	/** 이동 거리 (m). ToAimPoint 는 무시. 카티야 E 4 · 다니엘 E 3 · 재키 Q 1.5 */
	UPROPERTY(EditDefaultsOnly, Category = "자기 이동", meta = (ClampMin = "0", EditCondition = "SelfMove != ESkillSelfMove::None && SelfMove != ESkillSelfMove::ToAimPoint"))
	float SelfMoveDistance = 0.f;

	/** 이동 시간 (초). 카티야 E 0.3 */
	UPROPERTY(EditDefaultsOnly, Category = "자기 이동", meta = (ClampMin = "0.01", EditCondition = "SelfMove != ESkillSelfMove::None"))
	float SelfMoveDuration = 0.3f;

	// ── 시전 시간 (F07-04) ─────────────────────────────────────
	// ⚠ 스칼라다 — 6인 전 스킬에서 레벨별로 변하는 시전 시간이 없다 (역기획서 §2.2 "레벨업해도 안 변하는 것").
	// ⚠ 몽타주 길이로 정의하지 않는다 — 데디케이티드 서버는 애니메이션을 평가하지 않는다 (§4.2). 이 값이 진실이다.

	/** 선딜(캐스팅·채널링) 초. 0 = 즉발. 이 구간에서 CC(State.Block.Skill) 로 취소된다 — 쿨다운은 남고 코스트는 안 든다 (§5.1). */
	UPROPERTY(EditDefaultsOnly, Category = "시전", meta = (ClampMin = "0"))
	float CastTime = 0.f;

	/** 후딜 초. 0 = 없음. 이 구간에서 다른 스킬 발동이 막히고, 이동 입력이 후딜을 끝낸다(애니메이션 캔슬, §5.1). */
	UPROPERTY(EditDefaultsOnly, Category = "시전", meta = (ClampMin = "0"))
	float RecoveryTime = 0.f;

	/**
	 * 선딜 중 이동 입력이 시전을 **취소**하는가. false 면 선딜 중 이동이 **차단**된다(State.Block.Movement).
	 * ⚠ 자체 결정값 — 원작은 "채널링 중 이동 가능 여부 (미확인)" (역기획서 §5). 스킬마다 정한다 (§5.1 bCancelableByMove).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "시전", meta = (EditCondition = "CastTime > 0"))
	bool bMoveCancelsCast = false;

	// ── 레벨 규칙 ─────────────────────────────────────────────
	// ⭐ **슬롯이 아니라 스킬이 자기 규칙을 든다.** "R 은 3" 이 아니다 — 궁이 4레벨인 실험체,
	//   E 가 1레벨로 시작하는 실험체가 있다. 코드에 "슬롯이 R 이면" 분기를 두지 않는다.
	// ⚠ 실험체 레벨 조건(R 해금)은 **맨 아래** MinCharacterLevel (F10-03).

	/** 부여 시 레벨. 0 = 미습득(시전 불가). P 나 "1레벨로 시작하는 E" 는 1. */
	UPROPERTY(EditDefaultsOnly, Category = "레벨", meta = (ClampMin = "0"))
	int32 InitialLevel = 0;

	/** 최대 레벨. Q/W/E 대개 5, R 대개 3. Cooldowns · Costs 배열 길이가 이보다 짧으면 부여 때 경고. */
	UPROPERTY(EditDefaultsOnly, Category = "레벨", meta = (ClampMin = "1"))
	int32 MaxLevel = 5;

	/** 스킬 포인트로 올릴 수 있는가. D(무기 숙련도, F11) 는 false. ⭐ P 도 true — 1레벨로 시작하고 나머지는 포인트로 찍는다 (사용자 확인 2026-09-18). */
	UPROPERTY(EditDefaultsOnly, Category = "레벨")
	bool bUsesSkillPoints = true;

	// ── 실험체 레벨 조건 (F10-03) ──────────────────────────────
	// ⭐ 코드에 "R 이면 6레벨" 분기가 없다 — 스킬이 자기 조건을 든다 (위 레벨 규칙과 같은 결정).
	// ⚠ "실험체 레벨에 따른 자동 강화" 는 없다 — P 도 포인트로 찍는다. 자동으로 오르는 건 D(무기 숙련도, F11)뿐이고 그건 숙련도 축이다.

	/**
	 * [i] = 스킬 Lv.(i+1) 을 **포인트로 찍는 데** 필요한 실험체 레벨. 비어 있거나 짧으면 그 레벨은 제한 없음.
	 * ⚠ 자체 결정값 — R 해금 시점 원작 (미확인) (스킬 역기획서 §2 표 · F07-03 조사 실패). 출발값 R = [6, 11, 16] (LoL 관례).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "레벨", meta = (EditCondition = "bUsesSkillPoints"))
	TArray<int32> MinCharacterLevel;

};

/**
 * 부여한 스킬의 핸들. **회수할 때 쓴다.**
 *
 * ⭐ Lyra 의 FLyraAbilitySet_GrantedHandles 와 같은 목적 (LyraAbilitySet.cpp:32-61).
 *   무기 교체(F11)로 D 슬롯을 갈아 끼울 때, 이전 것을 정확히 빼야 한다.
 *
 * ⚠ USTRUCT 라 UPROPERTY 로 들고 있어야 한다. 핸들 자체는 UObject 가 아니지만
 *   구조체를 액터 멤버로 둘 때 리플렉션이 필요하다.
 */
USTRUCT()
struct FERGrantedSkillHandles
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
};

namespace ERSkill
{
	/**
	 * [서버] 스킬 목록을 ASC 에 부여한다.
	 *
	 * ⭐ 각 어빌리티 스펙에 세 가지를 심는다:
	 *   - SourceObject      = 그 UERSkillData   -> 어빌리티가 GetSkillData() 로 꺼낸다
	 *   - DynamicAbilityTags = SlotTag           -> 입력이 스펙을 순회하며 HasTagExact 로 찾는다 (E10)
	 *   - Level             = InitialLevel       -> 0 이면 미습득. 포인트로 올린다 (LevelUpSkill)
	 *   (Lyra 와 같은 자리 — LyraAbilitySet.cpp:96-98)
	 *
	 * ⚠ 서버에서만 동작한다. 클라가 부르면 아무 일도 안 한다 (CLAUDE.md §8).
	 * ⚠ 잘못된 항목(빈 클래스 · 빈 슬롯)은 **로그를 남기고 건너뛴다.** 전체를 실패시키지 않는다.
	 */
	void GrantSkills(UAbilitySystemComponent* ASC, const TArray<TObjectPtr<UERSkillData>>& Skills,
		FERGrantedSkillHandles& OutHandles);

	/** [서버] GrantSkills 로 부여한 것을 전부 회수한다. */
	void TakeSkills(UAbilitySystemComponent* ASC, FERGrantedSkillHandles& Handles);

	/**
	 * 슬롯 태그 -> 그 슬롯의 쿨다운 태그 (Ability.Slot.Q -> Cooldown.Slot.Q).
	 * ⚠ 문자열을 조합하지 않는다 (CLAUDE.md §8). 모르는 슬롯이면 빈 태그 + 로그.
	 */
	FGameplayTag CooldownTagForSlot(const FGameplayTag& SlotTag);

	/** 슬롯 태그 -> 리캐스트 태그 (Ability.Slot.Q -> Recast.Slot.Q). 평타는 리캐스트가 없다 → 빈 태그. */
	FGameplayTag RecastTagForSlot(const FGameplayTag& SlotTag);

	/**
	 * [서버] 슬롯의 스킬 레벨을 1 올린다. 성공하면 true.
	 *
	 * 검증(전부 그 스펙의 UERSkillData 기준): 슬롯 스펙 존재 · bUsesSkillPoints · Level < MaxLevel · MinCharacterLevel[Level] <= CharacterLevel (F10-03).
	 * ⚠ 포인트 잔량은 여기서 보지 않는다 — 포인트는 PlayerState 의 것이다 (AERPlayerState::ServerLevelUpSkill).
	 * ⚠ 실패 사유는 로그로 남긴다. 클라 피드백(Client RPC)은 UI 작업 때.
	 */
	bool LevelUpSkill(UAbilitySystemComponent* ASC, const FGameplayTag& SlotTag, int32 CharacterLevel);
}
