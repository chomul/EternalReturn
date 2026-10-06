// F19-04 조각 — 이벤트가 오면 다음 평타 강화 장전 (시셀라 P 윌슨과 하나가 될 때 · Argument 68)

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_EventAttackBuff.generated.h"

/**
 * 패시브 전용. EventTag 가 오면 **이 스킬(패시브 DA)** 의 피해 · 적중 조각을 다음 평타에 장전 (만료 없음).
 * 장전된 강화를 평타가 **쓰면** 이 스킬의 쿨다운(Cooldowns)이 돈다 (UERBasicAttackAbility) — 쿨 중에 온 이벤트는 무시
 * (사용자 2026-10-06 "강화 평타를 쓰고 2초간 쿨타임 · 그 후 또 윌슨 주우면 장전").
 */
UCLASS(DisplayName = "이벤트 때 다음 평타 강화")
class ETERNALRETURN_API UERSkillFragment_EventAttackBuff : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** 들을 이벤트 — 시셀라 Event.Wilson.Joined */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "Event"))
	FGameplayTag EventTag;

	virtual FGameplayTag GetPassiveEventTag() const override { return EventTag; }
	virtual void OnPassiveEvent(FERSkillContext& Ctx, const FGameplayEventData& Payload) const override;
	virtual FString GetDebugName() const override { return TEXT("이벤트 때 다음 평타 강화"); }
};
