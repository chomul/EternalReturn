// F19-03 조각 — 처치 시 쿨다운 초기화 · 자기 상태 · 지속 연장 · 재사용 창 다시 열기 (재키 P · E · R · Argument 65 R1)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_OnKill.generated.h"

/**
 * 패시브가 듣는 처치 이벤트 (막타 — F14 전 임시 "처치 관여") 마다:
 *   ① ResetCooldownTags 쿨다운 GE 제거 (재키 E) ② 자신에게 SelfTag N초 (재키 아드레날린)
 *   ③ ExtendWhileTag 가 있으면 그 태그를 가진 효과 전부 +ExtendSeconds (재키 R 버프) · RecastTag 창도 같이 늘리고, 없으면 (학살을 썼다) 남은 시간만큼 **다시 연다**
 */
UCLASS(DisplayName = "처치 시")
class ETERNALRETURN_API UERSkillFragment_OnKill : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** 이 쿨다운 태그의 GE 를 지운다 (재키 E — Cooldown.Slot.E) */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "Cooldown"))
	FGameplayTagContainer ResetCooldownTags;

	/** 자신에게 이 태그를 SelfTagDuration 초 (재키 State.Adrenaline) */
	UPROPERTY(EditDefaultsOnly)
	FGameplayTag SelfTag;

	UPROPERTY(EditDefaultsOnly)
	TArray<float> SelfTagDuration;

	/** 이 태그가 있을 때만 연장 (재키 Mode.Chainsaw) */
	UPROPERTY(EditDefaultsOnly)
	FGameplayTag ExtendWhileTag;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float ExtendSeconds = 0.f;

	/** 같이 늘리고 · 없으면 다시 여는 리캐스트 창 태그 (재키 Recast.Slot.R — 학살) */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "Recast"))
	FGameplayTag RecastTag;

	virtual void OnKillDealt(FERSkillContext& Ctx, AActor* Victim) const override;
	virtual FString GetDebugName() const override { return TEXT("처치 시"); }
};
