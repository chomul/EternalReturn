// F19-04 조각 — 정해진 순간에 소리 키 하나 (시셀라 R 시작 · 카운트 · Audio/Sissela.md 16 · 17)

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_Sfx.generated.h"

/**
 * 판정 큐(공격 · 타격)로는 못 잡는 순간의 소리 — 선딜 시작(누른 순간) · 실행(선딜이 끝난 순간). 시전자 자리 · 모든 클라 (GameplayCue.Pres.Sfx).
 * 시셀라 R: 누를 때 Skill04_Start (CastStartSfx) · 집중이 끝나 카운트가 시작될 때 Skill04_Count (ExecuteSfx) — 시전 큐는 끈다 (bNoCastCue)
 */
UCLASS(DisplayName = "소리")
class ETERNALRETURN_API UERSkillFragment_Sfx : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** 선딜 시작 (누른 순간 · CastTime > 0 스킬만) */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "Pres.Sfx"))
	FGameplayTag CastStartSfx;

	/** 실행 (선딜이 끝난 순간 · 판정 전) */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "Pres.Sfx"))
	FGameplayTag ExecuteSfx;

	virtual void OnCastStart(FERSkillContext& Ctx) const override;
	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual FString GetDebugName() const override { return TEXT("소리"); }
};
