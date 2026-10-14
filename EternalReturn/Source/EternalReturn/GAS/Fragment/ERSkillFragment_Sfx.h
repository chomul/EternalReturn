// F19-04 조각 — 정해진 순간에 소리 키 하나 (시셀라 R 시작 · 카운트 · Audio/Sissela.md 16 · 17)

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_Sfx.generated.h"

class USoundBase;

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

	/** 실행 순간 소리 **파일** — 캐릭터와 무관한 스킬 (전술 스킬 · Argument 82 A). 큐에 애셋을 실어 보낸다 */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<USoundBase> ExecuteSound;

	/** 맞은 대상마다 그 자리 소리 파일 (판정 뒤 · 투사체는 도착 때) */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<USoundBase> HitSound;

	virtual void OnCastStart(FERSkillContext& Ctx) const override;
	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const override;
	virtual bool IsHitEffect() const override { return HitSound != nullptr; }   // 투사체 · 펄스 · 추가 피해 데이터의 적중에도 (ApplyOnTargets)
	virtual FString GetDebugName() const override { return TEXT("소리"); }
};
