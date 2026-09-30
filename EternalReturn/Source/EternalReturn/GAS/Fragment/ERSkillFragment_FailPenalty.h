// F12.6-03 조각 — 스킬 실패 벌칙 (멧돼지 돌진 "좌절": 빗나가거나 시전 중 CC 로 끊기면 기절 + 받는 피해 증가)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "GAS/ERSkillData.h"
#include "ERSkillFragment_FailPenalty.generated.h"

/**
 * 실패하면 **시전자 자신에게** GE 들 (FERSelfEffect — 자기 버프 조각과 같은 칸).
 * 원작: 멧돼지 돌진이 빗나가거나 시전 중 CC 로 끊기면 [좌절] — 기절 + 받는 피해 증가 (역기획서 §1.4 · Argument 51). 수치 (미확인) → DA `[자체]`.
 * ⭐ 막는 축은 GE 가 준다 (기절 GE 의 State.Block.*) — 조각은 "언제" 만 정한다.
 */
UCLASS(DisplayName = "실패 벌칙 (좌절)")
class ETERNALRETURN_API UERSkillFragment_FailPenalty : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** 걸 GE 들 — 예: 좌절 GE (기절 태그 + DamageTakenAmp) */
	UPROPERTY(EditDefaultsOnly)
	TArray<FERSelfEffect> Effects;

	/** 판정 대상 0명이면 (빗나감) */
	UPROPERTY(EditDefaultsOnly)
	bool bOnMiss = true;

	/** 선딜 중 CC 로 끊기면 */
	UPROPERTY(EditDefaultsOnly)
	bool bOnCastInterrupted = true;

	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const override;
	virtual void OnEnd(FERSkillContext& Ctx, bool bCancelled) const override;
	virtual FString GetDebugName() const override { return TEXT("실패 벌칙"); }

private:
	void Apply(FERSkillContext& Ctx, const TCHAR* Why) const;
};
