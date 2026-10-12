// F14 S4 조각 — N초 집중 뒤 스스로 빈사에서 일어난다 (빈사 스킬 R 자가 제세동 · Argument 79)

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_SelfRevive.generated.h"

class UERSkillFragment_SelfRevive;

/** 집중 런타임 상태 — 어빌리티 인스턴스에 붙는다. */
UCLASS()
class UERSelfReviveState : public UERSkillFragmentState
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<const UERSkillFragment_SelfRevive> Frag;
	/** 집중 태그 GE (State.DownedSkill.SelfReviving + State.Block.Movement) — 끝나면 지운다 */
	FActiveGameplayEffectHandle ChannelHandle;
	/** 성공해서 끝났다 — 쿨다운 반환 안 함 */
	bool bSucceeded = false;

	UFUNCTION() void OnChannelDone();
	UFUNCTION() void OnInterruptedByCC();
	UFUNCTION() void OnNoLongerDowned();

	/** 집중 GE 를 지운다. 두 번 불려도 안전 */
	void ClearChannel();
};

/**
 * 발동하면 어빌리티를 활성으로 둔 채 ChannelSeconds 집중 → AERPlayerState::Revive(자기) (체력 = 최대 × ReviveHPRatio · 원작 25%).
 * 집중 중: 이동 · 다른 빈사 스킬 막힘 (원작 "어떠한 행동도 불가능"). 빈사 체력 감소는 그대로 (사용자 2026-10-12 — 1.19 판의 "집중 중 감소 멈춤" 안 씀).
 * 실패 (재입력 · CC — 어빌리티 취소) → 쿨다운의 FailRefund 만큼 반환 (원작 80% · 20초 → 16초). 실패 뒤 잠금 없음 (사용자 2026-10-12).
 * 그 사이 아군이 소생시키면 그냥 끝 (반환 없음 — 실패가 아니다).
 * ⚠ 피해로 끊기는지 (미확인) — 안 끊긴다.
 */
UCLASS(DisplayName = "자가 소생 (집중)")
class ETERNALRETURN_API UERSkillFragment_SelfRevive : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** 집중 초 (원작 20) */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.1"))
	float ChannelSeconds = 20.f;

	/** 실패 시 쿨다운 반환 비율 (원작 0.8) */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float FailRefund = 0.8f;

	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual void OnEnd(FERSkillContext& Ctx, bool bCancelled) const override;
	virtual bool KeepsAbilityActive() const override { return true; }
	virtual bool CancelOnReinput() const override { return true; }
	virtual FString GetDebugName() const override { return TEXT("자가 소생"); }
};
