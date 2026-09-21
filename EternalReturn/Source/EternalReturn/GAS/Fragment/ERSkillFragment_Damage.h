// F11.5 조각 — 피해 (F07-05 계수 · F11-05 도끼 최대체력 · 단검 고정 · 데드아이 잃은체력 · 도끼 회복)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "GAS/ERSkillData.h"
#include "ERSkillFragment_Damage.generated.h"

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

	UPROPERTY(EditDefaultsOnly)
	TArray<float> BonusAPRatio;

	UPROPERTY(EditDefaultsOnly)
	TArray<float> SkillAmpRatio;

	/** 대상 **최대** 체력 비례 (도끼 D). */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> MaxHPRatio;

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
