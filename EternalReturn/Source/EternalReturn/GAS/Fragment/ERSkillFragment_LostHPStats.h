// F19-04 조각 — 잃은 체력 비례 체력 재생 · 스킬 증폭 (시셀라 P 삶은 고통이에요 · Argument 68 P1)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_LostHPStats.generated.h"

/**
 * 패시브가 켜질 때(· 레벨이 바뀔 때) 상시 GE 하나 (UERLostHPStatsEffect) — 체력 100% = 최소 · 0% = 최대, 정비례 [자체 · 사용자 2026-10-06].
 * HP 가 바뀌면 GAS 가 크기를 다시 계산한다 (MMC · 스냅샷 없음) — 이 조각은 다시 걸지 않는다.
 * ApplyTimed = 같은 효과를 N초 하나 더 (R "패시브 효과 +100%" — R 작업 때 부른다).
 */
UCLASS(DisplayName = "잃은 체력 비례 스탯")
class ETERNALRETURN_API UERSkillFragment_LostHPStats : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** 초당 체력 재생 — 체력 100% 일 때 (레벨별) · 시셀라 2/4/6 */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> RegenMin;

	/** 체력 0% 일 때 · 시셀라 10/20/30 */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> RegenMax;

	/** 스킬 증폭 — 체력 100% 일 때 · 시셀라 4/7/10 */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> SkillAmpMin;

	/** 체력 0% 일 때 · 시셀라 28/39/50 */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> SkillAmpMax;

	virtual void OnPassiveStart(FERSkillContext& Ctx) const override;
	virtual FString GetDebugName() const override { return TEXT("잃은 체력 비례 스탯"); }

	/** [서버] 같은 효과를 Duration 초 하나 더 — 둘이 더해져 2배 (R 뒤 7/8/9초). Ctx 는 패시브 문맥 (레벨 = 패시브 레벨) */
	void ApplyTimed(FERSkillContext& Ctx, float Duration) const;

private:
	void ApplyEffect(FERSkillContext& Ctx, float Duration) const;
};
