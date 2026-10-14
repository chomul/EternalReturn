// F20 조각 — 해로운 상태 해제 (전술 무효화 · 라이트 윙 · Argument 80)

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "GAS/ERSkillData.h"
#include "ERSkillFragment_Cleanse.generated.h"

class UAbilitySystemComponent;

/**
 * RemoveTags 를 주는 GE 를 지운다 (KeepTags 를 주는 GE 는 남긴다). 시전자는 발동 때 · 맞은 대상(아군 판정)은 판정 뒤.
 * 하나라도 지웠으면 그 사람에게 IfRemoved (무효화 "해로운 상태 이상이 해제되면 추가로 1초간 이동 속도 30%").
 * ⚠ GE 단위로 지운다 — 기절 GE 의 이동 · 스킬 막기 태그도 같이 빠진다. 넉백(강제 이동)은 GE 가 아니라 원래 안 지워진다.
 */
UCLASS(DisplayName = "해로운 상태 해제")
class ETERNALRETURN_API UERSkillFragment_Cleanse : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** 이 태그(자식 포함)를 주는 GE 를 지운다. 비면 State.CC 전부 */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "State"))
	FGameplayTagContainer RemoveTags;

	/** 이 태그를 주는 GE 는 남긴다 — 무효화: 에어본 (원작 "에어본 · 넉백 · 붙잡힘 · 경직 제외") */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "State"))
	FGameplayTagContainer KeepTags;

	/** 시전자 자신 (발동 때) */
	UPROPERTY(EditDefaultsOnly)
	bool bSelf = true;

	/** 맞은 대상 (판정 뒤 — 아군 원) */
	UPROPERTY(EditDefaultsOnly)
	bool bTargets = false;

	/** 하나라도 지운 사람에게 거는 효과 (Effect · Duration · Magnitude 만 본다) */
	UPROPERTY(EditDefaultsOnly)
	TArray<FERSelfEffect> IfRemoved;

	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const override;
	virtual FString GetDebugName() const override { return TEXT("해로운 상태 해제"); }

private:
	void CleanseOne(FERSkillContext& Ctx, UAbilitySystemComponent* Who) const;
};
