// F19-02 조각 — 자기 중첩 (매그너스 P 근성 · R 즉시 근성)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_SelfStacks.generated.h"

class UGameplayEffect;
class UERSkillData;

/**
 * 자기에게 **중첩 GE** 를 쌓는다 (Argument 61 S1 — GAS 스택). 중첩당 크기는 레벨별 · 최대가 되면 다른 GE 하나 더.
 *
 * 쌓는 때 (둘 다 써도 된다):
 *   - **적중 이벤트** (패시브 어빌리티 · `OnHitDealt`) — 평타 적중 StacksOnBasicHit · 스킬 적중 StacksOnSkillHit (매그너스 P: 1 · 2)
 *   - **발동 순간** (`OnExecute`) — StacksOnExecute (레벨별 · 매그너스 R: 4/7/10). StackSource 의 조각 설정 · 그 스킬 레벨로 쌓는다
 * 쌓을 때마다 지금 레벨 크기로 **다시 건다** → 레벨이 오르면 다음 적중부터 새 값 (Argument 61 S1).
 * 영구 — 지우는 곳 없음 (사용자 2026-10-03 "죽어도 초기화 X").
 *
 * GE 애셋 규약: 무한 · 스택 AggregateByTarget · 상한 = MaxStacks · 모디파이어 크기 SetByCaller.OnHitMagnitude
 *   중첩 GE: 방어력 **Multiply** — 조각이 1 + PerStack 를 넣고 GAS 가 (크기 − 1) × 중첩 + 1 로 합친다
 */
UCLASS(DisplayName = "자기 중첩")
class ETERNALRETURN_API UERSkillFragment_SelfStacks : public UERSkillFragment
{
	GENERATED_BODY()

public:
	/** 중첩 GE (GE_Magnus_ToughBody) */
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> StackEffect;

	/** 중첩당 비율 (레벨별) — 0.02 = 2% */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> PerStack;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "1"))
	int32 MaxStacks = 10;

	/** 최대 중첩일 때 거는 GE (GE_Magnus_ToughBodyMax — 체력 재생) · 크기 레벨별 */
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> MaxStackEffect;

	UPROPERTY(EditDefaultsOnly)
	TArray<float> MaxStackMagnitude;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	int32 StacksOnBasicHit = 0;

	UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0"))
	int32 StacksOnSkillHit = 0;

	/** 적 **실험체**를 맞혔을 때만 (야생동물 제외 — 툴팁 "적 실험체에게") */
	UPROPERTY(EditDefaultsOnly)
	bool bPlayersOnly = true;

	/** 발동 순간 쌓기 (레벨별 · 이 스킬 레벨) — 매그너스 R 4/7/10 */
	UPROPERTY(EditDefaultsOnly)
	TArray<int32> StacksOnExecute;

	/** 발동 쌓기가 쓰는 중첩 설정 · 레벨의 주인 (매그너스 R → DA_Skill_Magnus_P). 비면 이 조각 · 이 스킬 */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UERSkillData> StackSource;

	virtual void OnHitDealt(FERSkillContext& Ctx, AActor* Target, const FGameplayTagContainer& HitTags) const override;
	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual FString GetDebugName() const override { return TEXT("자기 중첩"); }

private:
	/** Cfg 설정 · CfgLevel 크기로 N 더한다 (상한 MaxStacks). */
	static void AddStacks(FERSkillContext& Ctx, const UERSkillFragment_SelfStacks& Cfg, int32 CfgLevel, int32 N, const TCHAR* Why);
};
