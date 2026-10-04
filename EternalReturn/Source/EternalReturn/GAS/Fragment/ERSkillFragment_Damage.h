// F11.5 조각 — 피해 (F07-05 계수 · F11-05 도끼 최대체력 · 단검 고정 · 데드아이 잃은체력 · 도끼 회복)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "GAS/ERSkillData.h"
#include "ERSkillFragment_Damage.generated.h"

/** 순차 사격의 한 발 값 (카티야 R 1 · 2 · 3발) — 레벨별 배열. 비운 칸은 조각의 기본값을 쓴다. */
USTRUCT(BlueprintType)
struct FERShotDamage
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TArray<float> BaseDamage;

	UPROPERTY(EditDefaultsOnly)
	TArray<float> APRatio;
};

/**
 * 판정 대상에게 피해. 계수만 넘기고 곱하는 건 ERDamageExecution (Docs/4_Argument/5).
 * 형상(광역) 태그는 Ctx.ShapeOwner 의 Shape 를 따른다 — 평타 강화는 평타의 형상, 리캐스트·2차는 그 데이터의 형상.
 */
UCLASS(DisplayName = "피해")
class ETERNALRETURN_API UERSkillFragment_Damage : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	ESkillDamageType DamageType = ESkillDamageType::Skill;

	/** 레벨별. 역기획서 §2.2 — 스칼라 금지. */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> BaseDamage;

	UPROPERTY(EditDefaultsOnly)
	TArray<float> APRatio;

	/**
	 * **사거리 끝** 값 — 날아가는 투사체가 날아간 거리 비율(0~1)로 BaseDamage → 이 값 **직선 보간** (카티야 Q 40 → 60 · 사용자 2026-10-01 "직선 비례").
	 * 비우면 보간 없음. 즉시 판정 · 장판엔 영향 없음 (날아간 거리가 없다).
	 */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> BaseDamageFar;

	/** 사거리 끝 공격력 계수 — APRatio → 이 값 (카티야 Q 0.7 → 1.05). 기본 · 계수를 **각각** 보간 (역기획서 §3 Q). */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> APRatioFar;

	/** **발마다 다른 값** — 순차 사격(카티야 R)의 n 번째 발이면 [n] 을 쓴다. 사격할수록 세진다 (게임 툴팁 · 사용자 2026-10-01). 비우면 안 씀. */
	UPROPERTY(EditDefaultsOnly)
	TArray<FERShotDamage> ShotValues;

	UPROPERTY(EditDefaultsOnly)
	TArray<float> BonusAPRatio;

	UPROPERTY(EditDefaultsOnly)
	TArray<float> SkillAmpRatio;

	/** 대상 **최대** 체력 비례 (도끼 D). */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> MaxHPRatio;

	/** 대상 **현재** 체력 비례 — 이 스킬 피해에 더한다 (방어 적용 · 매그너스 R 30% · 재키 Q). Execution `Data.Damage.CurHPRatio` (ERDamageExecution.cpp 3-b). */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> CurHPRatio;

	/** 대상 **현재** 체력 비례 **고정 피해** — 별도 스펙 (Damage.Type.True · 방어 무시). 단검 D 8%. */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> FixedCurHPRatio;

	/** 대상 잃은 체력 비율 × 이 값 만큼 최종 피해 증가 (데드아이 1.0 = 최대 2배). */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> TargetLostHPScaleMax;

	/** 실제 깎인 HP 의 비율만큼 시전자 회복 (도끼 D 0.6). 흡혈 통로 · 감쇠 없음 `[자체]`. */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> HealFromDamageRatio;

	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const override;
	virtual FString GetDebugName() const override { return TEXT("피해"); }
	virtual bool IsHitEffect() const override { return true; }

	/** 피해 적용 본체 — 장판 · 벽 충돌 · 평타 강화가 ApplyOnTargets 를 거쳐 여기로 온다. */
	void Apply(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const;
};
