// F19-05 조각 — 곰돌이 (레니 P 곰돌이! 공격 · Argument 70 B1)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_Bear.generated.h"

/**
 * 패시브 전용 · 두 이벤트를 듣는다.
 * ① `Event.Ally.SkillHit` (레니 스킬이 아군 실험체에 맞음) → 그 아군에게 곰돌이 GE (`State.Mark.LeniBear` · Duration · 출처 = 레니) — 이미 있으면 새로
 * ② `Event.Mark.Triggered` (표식 = 곰돌이) (곰돌이 아군이 적을 때림 · 어트리뷰트셋) → 곰돌이를 지우고 (한 번 쓰면 사라짐 · 사용자 2026-10-07)
 *    맞은 적에게 **이 패시브 DA 의 피해 조각** (레니가 준 피해) · 레니 Q · W · E 쿨 −CooldownCut
 * 곰돌이는 레니 자신에게는 안 붙는다 (사용자 2026-10-07)
 */
UCLASS(DisplayName = "곰돌이")
class ETERNALRETURN_API UERSkillFragment_Bear : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** 곰돌이 유지 (초) */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.1"))
	float Duration = 5.f;

	/** 곰돌이로 피해를 주면 레니 Q · W · E 쿨 감소 (초 · 레벨별) — 0.5/0.75/1 */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> CooldownCut;

	virtual void GetPassiveEventTags(FGameplayTagContainer& OutTags) const override;
	virtual void OnPassiveEvent(FERSkillContext& Ctx, const FGameplayEventData& Payload) const override;
	virtual FString GetDebugName() const override { return TEXT("곰돌이"); }
};
