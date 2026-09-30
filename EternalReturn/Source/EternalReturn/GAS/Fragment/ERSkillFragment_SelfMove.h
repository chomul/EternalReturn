// F11.5 조각 — 자기 이동 (돌진 · 도약 · 백스텝 · 블링크. F07-06 · F11-05 C 단검)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "GAS/ERSkillData.h"
#include "ERSkillFragment_SelfMove.generated.h"

/**
 * 판정 전에 시전자를 움직인다 (ERForcedMove::ApplySelfMove — 넉백과 같은 RootMotionSource 경로).
 * BlinkBehindTarget 은 텔레포트. 대상이 없거나 사거리 밖이면 CanExecute 가 거부 → 발동 자체가 없던 일 (리캐스트 창 유지 · 사용자 확인 2026-09-20).
 */
UCLASS(DisplayName = "자기 이동")
class ETERNALRETURN_API UERSkillFragment_SelfMove : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	ESkillSelfMove Mode = ESkillSelfMove::TowardAim;

	/** 거리 (m). ToAimPoint 는 무시 (조준점까지). Blink 는 대상 건너편 거리. */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float Distance = 0.f;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.01"))
	float Duration = 0.3f;

	/**
	 * **밀어내며 돌진** — 앞에 있는 캐릭터에 막혀 멈추지 않고 목적지까지 간다 (벽 · 지형은 그대로 막힌다). 부딪힌 캐릭터는 넉백 조각이 밀어낸다.
	 * 멧돼지 돌진 (사용자 2026-09-30 "돌진하면서 다른 걸 다 밀어버린다" · F12.6-04).
	 * 구현: 이동 시간 동안 시전자 캡슐의 Pawn 채널을 Overlap (막힘 없음) · 끝나면 되돌린다. 모든 머신에서 (강제 이동 멀티캐스트).
	 */
	UPROPERTY(EditDefaultsOnly)
	bool bPushThroughPawns = false;

	virtual bool CanExecute(const FERSkillContext& Ctx, FString& OutReason) const override;
	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual FString GetDebugName() const override { return TEXT("자기 이동"); }

private:
	/** 블링크 전제 — 통과면 nullptr. */
	const TCHAR* BlinkFailReason(const FERSkillContext& Ctx) const;
};
