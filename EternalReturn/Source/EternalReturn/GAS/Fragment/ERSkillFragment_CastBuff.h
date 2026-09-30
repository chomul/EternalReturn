// F12.6-06 조각 — 충전(선딜) 동안만 자기 효과 (위클라인 「격리」: 받는 피해 −70% · 초당 400 회복)

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "GAS/ERSkillData.h"
#include "ERSkillFragment_CastBuff.generated.h"

/**
 * 선딜이 시작되면 자기에게 GE 들 → **판정 순간(OnExecute) 또는 끝(OnEnd · CC 취소 포함)에 뗀다.**
 * Duration 칸이 비면 선딜 길이만큼 (안전 — 떼는 걸 놓쳐도 선딜 뒤 끝난다).
 */
UCLASS(DisplayName = "충전 중 자기 효과")
class ETERNALRETURN_API UERSkillFragment_CastBuff : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TArray<FERSelfEffect> Effects;

	virtual void OnCastStart(FERSkillContext& Ctx) const override;
	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual void OnEnd(FERSkillContext& Ctx, bool bCancelled) const override;
	virtual FString GetDebugName() const override { return TEXT("충전 중 자기 효과"); }

private:
	void RemoveAll(FERSkillContext& Ctx, const TCHAR* Why) const;
};

/** 건 효과 핸들 — 어빌리티 인스턴스에 붙는다. */
UCLASS()
class ETERNALRETURN_API UERCastBuffState : public UERSkillFragmentState
{
	GENERATED_BODY()

public:
	TArray<FActiveGameplayEffectHandle> Handles;
};
