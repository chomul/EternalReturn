// F11.5 조각 — 다음 기본 공격 강화 (F07-07 재키 W · 카티야 P)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_NextAttackBuff.generated.h"

/**
 * 사용 후 State.NextAttackBuff 를 건다 — 다음 평타가 이 스킬의 피해 · 적중 조각을 얹고 소비한다 (UERBasicAttackAbility · Argument 19 ②A).
 * 적중 여부와 무관. Duration 0 = 만료 없음.
 */
UCLASS(DisplayName = "다음 평타 강화")
class ETERNALRETURN_API UERSkillFragment_NextAttackBuff : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float Duration = 0.f;

	/**
	 * 강화 내용(피해 · 적중 조각)을 **다른 스킬 DA** 에서 — 레벨은 그 스킬의 지금 레벨 (카티야 Q · E → P 잿빛 사신 · F19-01).
	 * 비우면 이 스킬 자신 (지금까지와 같다 — 재키 W). 그 스킬을 아직 안 배웠으면 강화를 걸지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<const UERSkillData> BuffSkill;

	/**
	 * 평타 초기화 (재키 W · 사용자 2026-10-05 "평타 치다가 W 누르면 평타가 캔슬되면서 W 강화 평타가 나감") — [서버] 강화를 건 **바로 뒤**
	 * **평타를 치고 있을 때만** (평타 진행 중 · 평타 간격 안): 진행 중 평타를 끊고 · 평타 쿨다운을 지우고 · 곧바로 평타를 친다 (강화를 소비).
	 * 대상 = 치던 대상(마지막 평타 대상) → 없으면 커서 아래 적. 평타 중이 아니면 강화만 걸고 끝 — 다음 평타가 강화 (사용자 2026-10-05).
	 */
	UPROPERTY(EditDefaultsOnly)
	bool bAttackNow = false;

	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const override;

	/** [서버] 강화 걸기 본체 — Source 의 피해 · 적중 조각을 다음 평타에 (Duration 0 = 만료 없음). 다른 조각도 부른다 (시셀라 P 윌슨 합침 · Argument 68) */
	static bool Grant(FERSkillContext& Ctx, const UERSkillData* Source, int32 SourceLevel, float InDuration);
	virtual FString GetDebugName() const override { return TEXT("다음 평타 강화"); }
};
