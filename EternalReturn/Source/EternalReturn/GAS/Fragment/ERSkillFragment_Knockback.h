// F11.5 조각 — 넉백 + 벽 충돌 (F11-05 B 방망이)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_Knockback.generated.h"

class UERSkillData;

/**
 * 판정 대상을 **시전자가 바라보는 방향**으로 민다 (원작 12시즌: 맞는 각도 무관 — 나무위키 방망이). ERForcedMove 경로 · 면역 검사.
 * WallImpactSkill 이 있으면 벽에 부딪힌 대상에게 그 데이터의 적중 조각(피해 · 기절)을 한 번 더 (ApplyOnTargets).
 */
UCLASS(DisplayName = "넉백")
class ETERNALRETURN_API UERSkillFragment_Knockback : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float Distance = 3.f;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.01"))
	float Duration = 0.3f;

	/**
	 * 띄우는 높이 (m). 0 = 수평으로만 민다. >0 이면 포물선 (엔진 JumpForce) — 멧돼지 돌진 "띄우며 멀리 넉백" · Distance 0 이면 제자리 에어본 (F12.6-04).
	 * 에어본 CC(행동 막기)는 함께 거는 적중 효과(GE_CC_Airborne)가 맡는다 — 이 칸은 몸만 띄운다. `[자체]`
	 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float Height = 0.f;

	/** 시전자에게서 **멀어지는 방향**으로 민다 (주변을 밀어내는 폭발 — 위클라인 「격리」 · F12.6-06). 끄면 시전자가 바라보는 방향 (지금까지와 같다). */
	UPROPERTY(EditDefaultsOnly)
	bool bAwayFromCaster = false;

	/** 시전자가 **바라보는 왼쪽**으로 민다 (매그너스 E 강타 — 게임 툴팁 2026-10-03). bAwayFromCaster 보다 먼저 본다. */
	UPROPERTY(EditDefaultsOnly)
	bool bCasterLeft = false;

	/**
	 * **조준점 → 두 번째 조준 점** 방향으로 민다 (스킬 데이터 bAimDirectionOnHold — 누를 때 자리 · 뗄 때 커서 · 레니 R · 사용자 2026-10-07).
	 * 두 번째 점이 없거나 조준점 바로 위면 시전자 → 조준점
	 */
	UPROPERTY(EditDefaultsOnly)
	bool bSecondAimDirection = false;

	/** 시전자도 **조준점에서 이 반경(m) 안**이면 같은 방향 · 거리로 같이 날아간다 (레니 R "레니도 같이 · 트랩 위에 있을 때만" · 사용자 2026-10-07). 0 = 끔 */
	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	float CasterAlsoInRadius = 0.f;

	/** 시전자가 같이 날아갈 때 틀 동작표 키 (레니 R Skill04_Jump → Ability.Slot.R.Execute) */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "Ability.Slot"))
	FGameplayTag CasterAnimKey;

	/** 벽 충돌 소리 키 (대상 자리 · 레니 WallHit → Pres.Sfx.SkillWall.R) */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "Pres.Sfx"))
	FGameplayTag WallSfx;

	/** 벽 충돌 시 대상에게 실행할 데이터 (피해 · 적중 효과 조각만 쓴다). */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UERSkillData> WallImpactSkill;

	virtual void OnTargetsResolved(FERSkillContext& Ctx, const TArray<AActor*>& Targets) const override;
	virtual FString GetDebugName() const override { return TEXT("넉백"); }
};
