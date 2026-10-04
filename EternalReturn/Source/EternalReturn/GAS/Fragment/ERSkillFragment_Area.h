// F11.5 조각 — 장판 (F11-05 C 암기 마름쇠)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "GAS/ERSkillData.h"
#include "ERSkillFragment_Area.generated.h"

/**
 * 판정 대신 조준점에 AERSkillAreaActor 를 스폰 — Duration 초 동안 TickInterval 마다 반경 Radius 안의 적에게
 * 이 데이터의 적중 조각(피해 · 적중 효과)을 준다. 같은 대상은 맞을 때마다 피해가 (1 − DamageDecay)^n (Ctx.DamageScale), 적중 효과는 그대로.
 * ⚠ 반경 · 주기는 자체 결정값 (원문 "일정 범위" · "밟을 때마다").
 */
UCLASS(DisplayName = "장판")
class ETERNALRETURN_API UERSkillFragment_Area : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.1"))
	float Duration = 6.f;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.1"))
	float Radius = 1.5f;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.1"))
	float TickInterval = 1.f;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float DamageDecay = 0.f;

	/**
	 * 야생동물 · 보스 장판이 누군가를 맞히면 **그 사람의 팀으로 전투 진입** (F12.6-06 위클라인 유해 물질 "밟으면 어그로").
	 * 이미 전투 중이면 무시 (먼저 때린 팀 우선 · 역기획서 §6.5).
	 */
	UPROPERTY(EditDefaultsOnly)
	bool bAggroOnHit = false;

	/**
	 * 시전자가 장판 **안에 있는 동안** 시전자에게 거는 효과 (F12.6-06 신경 가스: 위클라인 공속 +20% · 이속 +40%).
	 * 펄스마다 TickInterval × 1.5 초로 다시 건다 → 나가면 곧 풀린다. Duration 칸은 무시.
	 */
	UPROPERTY(EditDefaultsOnly)
	TArray<FERSelfEffect> CasterEffectsInside;

	/** 시전자에 붙어 따라다닌다 — 시전자 발밑에서 시작 (매그너스 W 17대 1 · Argument 61 M1) */
	UPROPERTY(EditDefaultsOnly)
	bool bFollowCaster = false;

	/**
	 * 장판이 있는 동안 시전자의 스킬 모션을 유지한다 — 이동해도 안 끊고(몽타주 Loop 섹션이 반복), 장판이 사라지면 End 섹션으로 (매그너스 W · Argument 63 M1).
	 * 시전자 ASC 에 State.AnimHold (복제 loose 태그) 를 건다.
	 */
	UPROPERTY(EditDefaultsOnly)
	bool bHoldCasterAnim = false;

	/** 펄스마다 맞은 대상에 타격음 큐 (매그너스 W "도는 동안 타격음" · Audio/Magnus.md). 끄면 소리 없이 피해만 (독가스 · 마름쇠 — 지금까지 동작) */
	UPROPERTY(EditDefaultsOnly)
	bool bHitCuePerPulse = false;

	/**
	 * 펄스 수를 정한다 (0 = 끔 · 지금까지처럼 Duration 동안 TickInterval 마다). >0 이면 **시전 순간** 펄스 수 = BasePulses + 추가 방어력 / BonusDefensePerPulse (내림)
	 * → 간격 = Duration / 펄스 수 · 그 수만큼 치고 사라진다. 매그너스 W: 11 · 35 (게임 툴팁 · 추가 방어력 = 최종 − 기본, 근성 % 포함 — 사용자 2026-10-03)
	 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	int32 BasePulses = 0;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0", EditCondition = "BasePulses > 0"))
	float BonusDefensePerPulse = 0.f;

	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual FString GetDebugName() const override { return TEXT("장판"); }
};
