// F19-05 조각 — 대상 회복 · 대상 보호막 (레니 Q 아군 · E 아군 · Argument 70 단일안)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_Support.generated.h"

/** 양 = 고정(레벨별) + 시전자 실험체 레벨 × PerCharLevel + 시전자 스킬 증폭 × SkillAmpRatio */
USTRUCT()
struct FERSupportAmount
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TArray<float> Base;

	UPROPERTY(EditDefaultsOnly)
	TArray<float> PerCharLevel;

	UPROPERTY(EditDefaultsOnly)
	TArray<float> SkillAmpRatio;

	float Evaluate(const FERSkillContext& Ctx) const;
};

/** 맞은 대상 회복 — 회복 통로(IncomingHealing · 회복량 증폭 한 곳) */
UCLASS(DisplayName = "회복")
class ETERNALRETURN_API UERSkillFragment_Heal : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	FERSupportAmount Amount;

	/** 회복받는 대상마다 이 소리 키 (레니 Q Recovery — Pres.Sfx.SkillAlly.Q) */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "Pres.Sfx"))
	FGameplayTag HitSfx;

	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const override;
	virtual FString GetDebugName() const override { return TEXT("회복"); }
	virtual bool IsHitEffect() const override { return true; }
};

/** 맞은 대상 보호막 (ERShield) */
UCLASS(DisplayName = "보호막")
class ETERNALRETURN_API UERSkillFragment_Shield : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	FERSupportAmount Amount;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float Duration = 2.5f;

	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const override;
	virtual FString GetDebugName() const override { return TEXT("보호막"); }
	virtual bool IsHitEffect() const override { return true; }
};
