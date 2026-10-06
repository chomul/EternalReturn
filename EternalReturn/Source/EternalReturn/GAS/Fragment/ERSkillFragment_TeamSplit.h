// F19-05 조각 — 한 번의 판정을 적 · 아군으로 갈라 다른 데이터를 (레니 Q · W · E · Argument 70 T2)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_TeamSplit.generated.h"

class UERSkillData;

UENUM()
enum class EERTeamSplitSelf : uint8
{
	/** 레니 자신은 안 받는다 (대상 목록에 들어 있을 때만 — 터지는 투사체의 "범위 안이면") */
	None        UMETA(DisplayName = "목록에 있을 때만"),
	/** 아군을 하나라도 맞히면 레니도 아군 효과 (W 이속 · E 보호막 — 나무위키) */
	IfAnyAlly   UMETA(DisplayName = "아군을 맞히면 자신도"),
};

/**
 * 판정 팀은 `All` 로 넓게 잡고, 이 조각이 대상을 **적 / 아군**으로 나눠 하위 데이터의 적중 조각을 돌린다 (ApplyOnTargets).
 * 적 = 피해 · 둔화 · 기절 데이터 / 아군 = 회복 · 보호막 · 이속 데이터 — 아군 데이터에는 피해 조각이 없어 아군을 때릴 수 없다.
 * 아군(자신 제외)마다 `Event.Ally.SkillHit` 를 시전자 ASC 로 — 레니 패시브가 곰돌이를 건다 (B1).
 * InnerRadius > 0 이면 조준점에서 그 안의 적은 EnemyInnerSkill (레니 W 중앙 80% 둔화).
 */
UCLASS(DisplayName = "적 · 아군 나누기")
class ETERNALRETURN_API UERSkillFragment_TeamSplit : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** 적에게 (피해 · 적중 효과 조각) */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<const UERSkillData> EnemySkill;

	/** 중앙 안의 적에게 (비우면 EnemySkill) */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<const UERSkillData> EnemyInnerSkill;

	/** 중앙 반경 (m · 조준점 기준) — 0 = 중앙 없음 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float InnerRadius = 0.f;

	/** 아군에게 (회복 · 보호막 · 이속 조각) */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<const UERSkillData> AllySkill;

	UPROPERTY(EditDefaultsOnly)
	EERTeamSplitSelf SelfRule = EERTeamSplitSelf::None;

	/** 아군에게도 타격음 (레니 E — 적 · 아군 둘 다 · 사용자 2026-10-07). 끄면 적에게만 (Q · W) */
	UPROPERTY(EditDefaultsOnly)
	bool bAllyHitCue = false;

	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const override;
	virtual FString GetDebugName() const override { return TEXT("적 · 아군 나누기"); }
	/** 터지는 투사체의 폭발 데이터에서도 돈다 (ApplyOnTargets) */
	virtual bool IsHitEffect() const override { return true; }
};
