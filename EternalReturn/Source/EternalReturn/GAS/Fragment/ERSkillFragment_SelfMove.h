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

	virtual bool CanExecute(const FERSkillContext& Ctx, FString& OutReason) const override;
	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual FString GetDebugName() const override { return TEXT("자기 이동"); }

private:
	/** 블링크 전제 — 통과면 nullptr. */
	const TCHAR* BlinkFailReason(const FERSkillContext& Ctx) const;
};
