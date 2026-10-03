// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Combat/ERTargetingTypes.h"
#include "ERSkillData.generated.h"

class UERSkillFragment;
class UERSkillShapeBase;
class UERSkillDelivery;

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
class UERSkillData;

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

/**
 * 시전 시 **자기에게** 거는 버프 하나 (F11-05 B 권총 D). 적중과 무관하게 발동 시 적용. GE 애셋은 HasDuration.
 * Duration → SetByCaller.CCDuration · Magnitude → SetByCaller.OnHitMagnitude (의미는 GE 가 정한다 — 이속 배율 1.4) ·
 * Charges > 0 이면 State.ConsumeOnAttack 태그를 달아 기본 공격 적중마다 1씩 줄고 0 이면 사라진다 ("다음 2회").
 */
USTRUCT(BlueprintType)
struct FERSelfEffect
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> Effect;

	/** 레벨별 지속 초. 0 = 애셋의 지속시간 그대로. */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> Duration;

	/** 레벨별 크기 (SetByCaller.OnHitMagnitude). 0 = 안 넣는다. */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> Magnitude;

	/** 기본 공격 적중 N회로 소비. 0 = 시간으로만. 권총 D 공속 버프 = 2. */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	int32 Charges = 0;

	/** 레벨별 시작 지연 초. 0 = 즉시. 권총 D 공속 버프 = 1 ("이동이 끝난 후"). 그 사이 시전자가 죽으면 안 건다. */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> StartDelay;
};

/**
 * 모드 (F11-05 D 저격총). Duration > 0 이면 발동 뒤 어빌리티가 끝나지 않고 Duration 초 동안 **활성**으로 남아
 * 기본 공격 슬롯을 AttackData 로 갈아끼운다 (AERPlayerState::SetModeAttack). 발수가 Shots−1 이면 FinalAttackData 로 한 번 더 교체,
 * Shots 를 다 쏘면 · 시간이 다 되면 · (bCancelOnMove) 이동이 수락되면 · CC(State.Block.Skill) 가 오면 해제 → 평타 복구.
 * 한 발도 안 쐈으면 해제 시 남은 쿨다운의 UnusedCooldownRefund 만큼 돌려준다 (원문 "미사용 해제 시 쿨 50% 반환").
 * 근거: Docs/4_Argument/27_저격모드_표현과_발수전환.md (①A + ②A)
 */
USTRUCT()
struct FERSkillMode
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float Duration = 0.f;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "1", EditCondition = "Duration > 0"))
	int32 Shots = 3;

	/** 모드 중 기본 공격 슬롯에 들어갈 데이터 (Ability.Slot.Attack · 포인트 ✘). 레벨은 모드 스킬(D)의 레벨을 따른다. */
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "Duration > 0"))
	TObjectPtr<UERSkillData> AttackData;

	/** 마지막 발 (없으면 AttackData 그대로). */
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "Duration > 0"))
	TObjectPtr<UERSkillData> FinalAttackData;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "Duration > 0"))
	bool bCancelOnMove = true;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", ClampMax = "1", EditCondition = "Duration > 0"))
	float UnusedCooldownRefund = 0.f;

	/** 모드 동안 카메라 거리 배율 — 저격 사거리가 다 보이게 줌아웃 (원작 확인 2026-09-21 · 배율은 자체값). 1 = 그대로. 로컬 플레이어만. 부드럽게 보간 (PC). */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.5", ClampMax = "3", EditCondition = "Duration > 0"))
	float CameraZoomScale = 1.f;

	/** 모드 동안 화면 중심을 **진입 시 조준 방향**으로 이만큼(m) 민다 — 오른쪽 위를 보고 켰으면 캐릭터는 왼쪽 아래 (사용자 확인 2026-09-21). 0 = 중앙 유지. */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", EditCondition = "Duration > 0"))
	float CameraAimOffset = 0.f;

	/** 모드 평타의 조준을 **진입 시 바라본 방향** ± 이 각도(°) 안으로 제한 (저격: 30 — 사용자 확인 2026-09-21). 0 = 제한 없음. 서버가 ResolveAim 에서 자른다. */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", ClampMax = "180", EditCondition = "Duration > 0"))
	float AimHalfAngleDeg = 0.f;

	/** 진입 때 진행 중인 이동을 멈춘다 (저격: 켬 — 사용자 확인 2026-09-21 "이동 중에 쓰면 안 멈추고 쏜다"). 로컬 PC 의 경로 추적을 끊는다 — 이동은 클라가 구동한다 (E07). */
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "Duration > 0"))
	bool bStopMovementOnEnter = true;

	/**
	 * 연출 모드 태그 (F12.5-03 · Argument 42 S1) — 진입 때 서버가 **복제 loose 태그**로 붙이고 해제 때 뗀다.
	 * 각 머신의 연출 컴포넌트가 이 태그로 모드 세트(모드 자세 · 동작 덮어쓰기 · 해제 동작)를 찾는다. 비면 연출 모드 없음 (판정은 그대로).
	 */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "Mode", EditCondition = "Duration > 0"))
	FGameplayTag PresentationMode;
};

/** 야생동물 · 보스 AI 가 이 스킬을 언제 쓰나 (F12.6-03 · Argument 51 K1). 실험체는 입력으로 쓰니 None. 쿨다운은 GAS 가 거른다. */
UENUM()
enum class EERAIUse : uint8
{
	None,
	/** 전투 중 대상이 AIUseRange 안이면 — 평타보다 먼저 시도 (쿨이면 평타). 멧돼지 돌진 · 곰 강타 · 들개 깨물기 */
	InRange,
	/** 같은 무리의 다른 개체가 죽으면 (자기 대상 · 조준 없음). 늑대 울부짖기 */
	OnAllyDeath,
	/** 움직이는 동안 (추적 · 전투 · 귀환) 쿨이 돌 때마다 자기 자리에 (위클라인 유해 물질 — 지나간 경로에 독 · F12.6-06) */
	WhileMoving,
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
	/** 지정 대상(AimActor) 의 **건너편** SelfMoveDistance 로 **순간이동** (지형 통과 ○ — RootMotion 아님, 텔레포트) — 단검 D (F11-05 C). 대상 없으면 실패 */
	BlinkBehindTarget,
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

	/**
	 * 자기 원(SelfRadius · DualRadius)의 중심을 **조준 방향 앞으로** 이만큼(m) 옮긴다. 0 = 시전자 중심 (지금까지와 같다).
	 * 곰 지면 강타 — 앞발이 내려친 자리를 중심으로 한 원 (F12.6-04 · 사용자 2026-09-30). 반경은 RangeMax.
	 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", EditCondition = "Shape == ESkillTargeting::SelfRadius || Shape == ESkillTargeting::DualRadius"))
	float ForwardOffset = 0.f;

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

	/** Projectile 발 수 — 조준 방향을 가운데로 SpreadAngleDeg 만큼씩 부채처럼 편다. 1 = 한 발 (지금까지와 같다). 위클라인 트리플렛 코드 = 3 (F12.6-06). 같은 대상은 한 번만 맞는다. */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "1", EditCondition = "Shape == ESkillTargeting::Projectile"))
	int32 ProjectileCount = 1;

	/** 발 사이 각도(도). ProjectileCount > 1 일 때. `[자체]` */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", ClampMax = "180", EditCondition = "Shape == ESkillTargeting::Projectile"))
	float SpreadAngleDeg = 15.f;

	/**
	 * 투사체 속도 (m/s). **0 보다 크면 날아가는 투사체 액터**를 쏜다 — 날아가는 동안 피할 수 있고 적중은 도착 때 (F19-01 · Argument 54 P1).
	 * 0 = 지금처럼 발사 순간 선 판정 (트리플렛 · 저격 D 그대로). 카티야 Q 26 · R 40.
	 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", EditCondition = "Shape == ESkillTargeting::Projectile || Shape == ESkillTargeting::Trapezoid"))
	float ProjectileSpeed = 0.f;

	/** 쏠 투사체 클래스 — 모습(메시 · 이펙트)만 다른 BP 자식을 고른다. 비우면 C++ 기본 `AERProjectileBase`. */
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "ProjectileSpeed > 0"))
	TSubclassOf<class AERProjectileBase> ProjectileClass;

	/** Trapezoid (카티야 R 스캔) — 시전자 쪽 · 먼 쪽 변의 전체 폭 (m). */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", EditCondition = "Shape == ESkillTargeting::Trapezoid"))
	float TrapezoidNearWidth = 0.f;
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", EditCondition = "Shape == ESkillTargeting::Trapezoid"))
	float TrapezoidFarWidth = 0.f;
	/** Trapezoid 길이 (m) — **조준점(커서)이 가운데** · 시전자 쪽이 좁은 변. 0 이면 RangeMax. RangeMax 는 조준점을 당기는 사거리. */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", EditCondition = "Shape == ESkillTargeting::Trapezoid"))
	float TrapezoidLength = 0.f;

	/** 판정 결과 최대 수 (가까운 순). 0 = 제한 없음. 카티야 R = 3. */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	int32 MaxTargets = 0;

	/** 실험체만 (야생동물은 대상도 아니고 투사체도 지나친다) — 카티야 R "적 실험체". */
	UPROPERTY(EditDefaultsOnly)
	bool bPlayersOnly = false;

	/**
	 * Trapezoid + ProjectileSpeed > 0 — 결과 대상마다 **따라가는 탄**을 이 간격(초)으로 순서대로 (카티야 R "가까운 순서대로 한 발씩").
	 * 간격 `[자체]` (원작 미확인). 쏘기 전에 대상이 ShotCancelDistance(m) 보다 멀면 그 대상은 건너뛴다 (원작 30m).
	 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", EditCondition = "Shape == ESkillTargeting::Trapezoid"))
	float ShotInterval = 0.15f;
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", EditCondition = "Shape == ESkillTargeting::Trapezoid"))
	float ShotCancelDistance = 30.f;

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

/** 스킬 "누구를" (Argument 57 S3.1) — 모양 · 발사와 따로. 늘지 않는 축이라 구조체. */
USTRUCT()
struct FERSkillTargets
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	ETargetTeamFilter Team = ETargetTeamFilter::Enemy;

	/** 실험체만 — 야생동물 제외 (카티야 R · 사용자 2026-10-01) */
	UPROPERTY(EditDefaultsOnly)
	bool bPlayersOnly = false;

	/** 최대 대상 수 (가까운 순). 0 = 제한 없음. 비관통 직선 = 1 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	int32 MaxTargets = 0;

	/**
	 * 대상 하나 조준 보조 (m) — 커서 아래가 유효한 대상이 아니면 조준점 반경 안 가장 가까운 대상. 0 = 끔.
	 * ⚠ 자체 결정값. 원작의 클릭 허용 오차는 (미확인).
	 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float AimAssistRadius = 0.f;
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

	/** ⚠ 옛 칸 (S3.1 이전) — 읽기 전용 이관 원본. PostLoad 가 Area · Targets · Delivery 로 옮긴다. 쓰지 않는다 · 전부 저장 확인 뒤 삭제 */
	UPROPERTY()
	FERSkillShape Shape;

	// ── 어디를 · 누구를 · 어떻게 (Argument 57 S3.1) ───────────────
	/** 어디를 — 판정 모양 */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "판정")
	TObjectPtr<UERSkillShapeBase> Area;

	/** 누구를 */
	UPROPERTY(EditDefaultsOnly, Category = "판정")
	FERSkillTargets Targets;

	/** 어떻게 — 즉시 / 투사체 */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "판정")
	TObjectPtr<UERSkillDelivery> Delivery;

	/** 광역인가 — 흡혈 치유 감소 (F03-05). 단일 = 대상 하나 모양 · MaxTargets 1 · 비관통 한 발 투사체 (옛 규칙과 같은 결과) */
	bool IsAoE() const;
	/** 시전자에서 닿는 최대 거리(m) — 조준점 당기기 · AI 사용 거리 · 미리보기. 모양 없으면 0 */
	float GetMaxReach() const;
	/** 최소 거리(m) */
	float GetMinReach() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	// ── 이관 (S3.1) — 옛 Shape → Area · Targets · Delivery. 데이터 버전이 낮으면 PostLoad 가 메모리에서 옮긴다 ──
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	/** 옛 칸 → 새 칸. 이관 로그 한 줄을 돌려준다. ER.Skill.Resave 가 저장 대상을 고를 때 bShapeMigratedOnLoad 를 본다. */
	FString MigrateLegacyShape();
	/** 이번 로드에서 이관했다 (저장 안 됨) */
	bool bShapeMigratedOnLoad = false;

	/** 레벨별 배열에서 값 하나. 비면 0, 레벨이 배열보다 크면 마지막 값. 레벨은 1부터. Cooldowns · Costs 도 이걸 쓴다. */
	static float LevelValue(const TArray<float>& Values, int32 Level);
	/**
	 * 음수를 자르지 않는 판 — GE 크기(SetByCaller.OnHitMagnitude)용. 치유 감소(HealAmp −0.4) 처럼 **깎는** 효과가 음수다.
	 * ⚠ LevelValue 는 0 으로 자른다 → −0.4 가 0 이 되어 크기가 안 넘어갔다 (F12.6-04 들개 깨물기 · 로그 "GetMagnitude … not yet been set").
	 */
	static float LevelValueSigned(const TArray<float>& Values, int32 Level);

	// ── 적중 시 CC (F07-07) ────────────────────────────────────
	// ApplySkillDamage 가 피해 직후 ERCC::ApplyCC 로 건다 (F06 그대로 — 저항 · 면역 · 둔화 재계산 전부 거기서).

	// ── 다음 기본 공격 강화 (F07-07) ──────────────────────────
	// 켜면 발동 시 자기에게 State.NextAttackBuff 를 건다. 다음 평타가 적중하면 **이 스킬의 피해·적중 효과**를 얹고 소비한다.
	// 카티야 P · 재키 W · 시셀라 Q · 권총 D — 4곳 공용 (역기획서 §8).

	// ── 리캐스트 윈도우 (F07-07) ──────────────────────────────
	// 적중 시 Recast.Slot.* 를 RecastWindow 초 동안 건다. 그동안 이 슬롯은 쿨다운을 무시하고 한 번 더 발동된다.
	// ⚠ 자체 결정값: 재발동은 쿨다운을 새로 걸지 않는다(첫 시전의 쿨이 그대로) · 코스트는 든다. 원작 (미확인).

	// ── 자기 이동 (F07-06) ─────────────────────────────────────
	// [4] 발동 시 판정(ExecuteSkill)보다 **먼저** 시작한다. 돌진 끝에 맞히는 스킬은 파생이 시점을 정한다.
	// ⚠ 자체 결정값: 이동 중 이동 입력은 RootMotion 이 덮고(엔진), 스킬 입력은 RecoveryTime 으로 막는다 →
	//   돌진 스킬은 RecoveryTime ≥ SelfMoveDuration 으로 **데이터에서** 맞춘다. 이동 중 CC 는 끊지 않는다 (§5.1 [4] 원자적).

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

	// ── AI (F12.6-03 · 야생동물 · 보스) ─────────────────────────
	/** AI 가 언제 쓰나. None = AI 가 안 쓴다 (평타 슬롯은 AI 가 따로 친다). */
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	EERAIUse AIUse = EERAIUse::None;

	/** InRange — 대상이 이 거리(m, 시전자 몸 끝 → 대상 표면 · 평타 사거리와 같은 잼) 안이면 쓴다 `[자체]`. 0 = 모양 GetMaxReach. */
	UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "0", EditCondition = "AIUse == EERAIUse::InRange"))
	float AIUseRange = 0.f;

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
	 * R = [6, 11, 16] — 원작 **[확인]** (사용자 2026-09-19 "현재 기준이 맞다").
	 */
	UPROPERTY(EditDefaultsOnly, Category = "레벨", meta = (EditCondition = "bUsesSkillPoints"))
	TArray<int32> MinCharacterLevel;

	// ── 쿨다운 태그 (F11-04) ──────────────────────────────────
	/**
	 * 비어 있으면 슬롯 태그(Cooldown.Slot.X). D 는 **무기 계열 태그**(Cooldown.Weapon.Hammer)를 넣는다 — 무기를 바꿔도 각 무기의 쿨다운이 따로 보존된다
	 * (Docs/4_Argument/25 방안 B · 사용자 확인 2026-09-19). ⚠ D 는 bIgnoreCooldownReduction 도 켠다 — 원작 확인: 무기 스킬은 쿨다운 감소를 안 받는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "쿨다운", meta = (Categories = "Cooldown"))
	FGameplayTag CooldownTagOverride;

	// ── F11-05 A (망치 · 도끼) — 필드 순서 유지, 끝에 ──────────────

	// ── F11-05 B (권총 · 방망이) ──────────────────────────────

	// ── F11-05 C (암기 · 투척 · 단검) ─────────────────────────

	// ─────────────────────────────────────────────────────────
	// ⭐ 기능 조각 (F11.5 · Docs/4_Argument/28 B) — 피해 · 적중 효과 · 이동 · 버프 · 넉백 · 2차 · 리캐스트 · 강화 · 장판 · 모드는 전부 여기.
	//   새 기능 = GAS/Fragment/ 에 조각 클래스 하나. 이 클래스와 UERGameplayAbility 는 안 건드린다.
	//   순서 = 배열 순서 (같은 훅 안에서). 권장: 자기이동 → 자기버프 → 장판/모드 · 피해 → 적중효과 → 넉백 → 2차 → 리캐스트/강화.
	// ─────────────────────────────────────────────────────────
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "조각")
	TArray<TObjectPtr<UERSkillFragment>> Fragments;

	/** 첫 번째 T 조각. 없으면 nullptr. (평타 강화가 강화 스킬의 피해 조각을 찾을 때 · 어빌리티가 리캐스트/모드 조각을 볼 때) */
	template <class T>
	const T* FindFragment() const
	{
		for (const TObjectPtr<UERSkillFragment>& F : Fragments)
		{
			if (const T* Typed = Cast<T>(F.Get())) { return Typed; }
		}
		return nullptr;
	}
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
		FERGrantedSkillHandles& OutHandles, int32 LevelOverride = -1);   // LevelOverride >= 0 이면 InitialLevel 대신 (모드 평타 — E21)

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

	/**
	 * [서버] 포인트가 아닌 스킬(bUsesSkillPoints=false — D)의 스펙 레벨을 **직접** 맞춘다 (F11-03). 0 = 잠김.
	 * MaxLevel 로 클램프 · 같으면 아무것도 안 한다. 포인트 스킬에 부르면 Warning + false.
	 */
	bool SetSkillLevel(UAbilitySystemComponent* ASC, const FGameplayTag& SlotTag, int32 NewLevel, const TCHAR* Reason);
}
