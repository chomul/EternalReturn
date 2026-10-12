// F14 S4 조각 — N초 뒤 내 자리에서 폭발하고 나는 죽는다 (빈사 스킬 Q 분열 폭발 · Argument 79)

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_FuseBlast.generated.h"

class UERSkillFragment_FuseBlast;

/** 도화선 런타임 상태 — 어빌리티 인스턴스에 붙는다 (조각은 애셋이라 상태를 못 든다). */
UCLASS()
class UERFuseBlastState : public UERSkillFragmentState
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<const UERSkillFragment_FuseBlast> Frag;
	/** 도화선 태그 GE (State.DownedSkill.Fuse — 다른 빈사 스킬 막기) */
	FActiveGameplayEffectHandle FuseHandle;

	UFUNCTION() void OnFuseDone();
	/** 도화선 태그를 지운다. 두 번 불려도 안전 */
	void ClearFuse();
};

/**
 * 발동하면 판정을 미루고 어빌리티를 활성으로 둔 채 Delay 초 (UAbilityTask_WaitDelay) → 이 스킬의 모양 · 적중 조각으로 판정 (ExecuteOther —
 * 모양 = 그때 내 자리 원 · 피해 조각 = 고정 150 + 대상 현재 체력 20%) → 아직 빈사면 사망 (AERPlayerState::SelfDestruct · 처치 = 빈사로 만든 사람 K2).
 * ⭐ 취소 없음 (원작 "취소가 불가능") — CC · 이동 · 재입력을 안 듣는다.
 * ⭐ 그 사이 죽으면 폭발 없음 (원작 "체력이 0이 될 경우 즉시 사망하며 분열 폭발 피해를 입힐 수 없습니다") — 사망이 어빌리티를 취소해 태스크가 같이 죽는다.
 * ⚠ 그 사이 아군이 소생시키면: 폭발은 하고 죽지 않는다 `[자체]` (원작 (미확인)).
 */
UCLASS(DisplayName = "도화선 폭발 (자폭)")
class ETERNALRETURN_API UERSkillFragment_FuseBlast : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** 폭발까지 초 (원작 4) */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float Delay = 4.f;

	/** 폭발 뒤 시전자 사망 (빈사일 때만) */
	UPROPERTY(EditDefaultsOnly)
	bool bKillSelf = true;

	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual void OnEnd(FERSkillContext& Ctx, bool bCancelled) const override;
	virtual bool KeepsAbilityActive() const override { return true; }
	virtual FString GetDebugName() const override { return TEXT("도화선 폭발"); }
};
