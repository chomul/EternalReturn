// F11.5 조각 — 리캐스트 창 (F07-07 재키 Q 적중 시 · F11-05 C 단검 망토 → 단검)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_Recast.generated.h"

class UERSkillData;

/**
 * 발동 뒤 Window 초 동안 Recast.Slot.X 태그를 연다 (UERRecastWindowEffect). 재입력은 쿨다운을 무시하고 발동 — 창 소비 (Argument 19 ③A).
 * RecastSkill 이 있으면 재입력 실행은 그 데이터 (자기 이동 · 판정 · 적중 조각 전부). 없으면 같은 데이터.
 * ⚠ 리캐스트로 발동한 실행에서는 다시 열지 않는다 — 무한 재사용.
 */
UCLASS(DisplayName = "리캐스트")
class ETERNALRETURN_API UERSkillFragment_Recast : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** true = 적중했을 때만 (재키 Q). false = 항상 (단검 망토). */
	UPROPERTY(EditDefaultsOnly)
	bool bOnHitOnly = true;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.01"))
	float Window = 3.f;

	/** 레벨별 창 길이 — 있으면 Window 대신 (재키 R 8/9/10 = R 지속 · Argument 65 R1) */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> WindowByLevel;

	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UERSkillData> RecastSkill;

	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const override;
	virtual const UERSkillData* GetRecastExecData() const override { return RecastSkill; }
	virtual FString GetDebugName() const override { return TEXT("리캐스트"); }
};
