// F20 조각 — 버프가 있는 동안 내 적중마다 (전술 라이트 윙 · 스트라이더 · Argument 80)

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "ERSkillFragment_WhileBuff.generated.h"

class UAbilitySystemComponent;
class UERGameplayAbility;
class UERSkillData;
class UERSkillFragment_WhileBuff;
struct FGameplayEventData;

/** 듣는 중 상태 — 어빌리티 인스턴스에 붙는다. 어빌리티는 바로 끝나도 버프 태그가 남아 있는 동안 ASC 적중 이벤트를 듣는다. */
UCLASS()
class UERWhileBuffState : public UERSkillFragmentState
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<const UERSkillFragment_WhileBuff> Frag;
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	TWeakObjectPtr<const UObject> Skill;
	/** HitSkill 을 돌릴 어빌리티 (ApplyOnTargets) */
	TWeakObjectPtr<UERGameplayAbility> Ability;
	int32 Level = 1;
	FDelegateHandle HitHandle;
	FDelegateHandle TagHandle;

	void Bind(UAbilitySystemComponent* InASC);
	void Unbind();
	void OnHit(const FGameplayEventData* Payload);
	void OnTagChanged(const FGameplayTag Tag, int32 Count);

	/** bOnce 의 HitSkill — 같은 프레임에 맞은 대상을 모았다가 다음 틱에 가장 가까운 한 명 (스트라이더 "광역기로 발동시켜도 제일 가까운 대상") */
	TArray<TWeakObjectPtr<AActor>> Pending;
	bool bResolveQueued = false;
	void ResolveOnce();
};

/**
 * WhileTag (같은 스킬의 자기 버프가 GrantTag 로 거는 태그) 가 있는 동안 **내 적중**(Event.Hit.Dealt · 서버) 마다:
 *   ① 맞은 대상에 HitEffect (라이트 윙 평타 둔화) ② ExtendSeconds 만큼 WhileTag 버프 연장 (라이트 윙 Lv2 "적 실험체 평타 적중 시 +0.5초").
 * 태그가 빠지면 듣기를 끊는다. 패시브가 아니라서 UERPassiveAbility 의 적중 훅(OnHitDealt)을 못 쓴다 — 그래서 ASC 이벤트를 직접 듣는다.
 */
UCLASS(DisplayName = "버프 중 적중 시")
class ETERNALRETURN_API UERSkillFragment_WhileBuff : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "State"))
	FGameplayTag WhileTag;

	/** 평타 적중만 (Damage.Type.BasicAttack) */
	UPROPERTY(EditDefaultsOnly)
	bool bBasicAttackOnly = true;

	/** 맞은 대상에 거는 효과 — ERCC::ApplyCC (둔화면 SlowPercent) */
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> HitEffect;

	UPROPERTY(EditDefaultsOnly)
	TArray<float> HitDuration;

	UPROPERTY(EditDefaultsOnly)
	TArray<float> HitSlowPercent;

	UPROPERTY(EditDefaultsOnly)
	TArray<float> HitMagnitude;

	/** 적중마다 WhileTag 버프 연장 초 (레벨별 · 0 = 안 함) */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> ExtendSeconds;

	/** 연장은 적 **실험체** 를 맞혔을 때만 (야생동물 X) */
	UPROPERTY(EditDefaultsOnly)
	bool bExtendOnPlayersOnly = true;

	/**
	 * 맞은 대상에게 이 데이터의 적중 조각 (피해 · 적중 효과 — ApplyOnTargets · 레벨 · 근/원거리 조건 그대로).
	 * 전술 스트라이더 "버프 중 적에게 피해를 주면 추가 스킬 피해 + Lv2 둔화" (Argument 80). 비우면 안 함
	 */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UERSkillData> HitSkill;

	/**
	 * 첫 적중 한 번만 — 발동하면 듣기를 끝낸다 (버프 태그는 남는다). HitSkill 피해가 다시 적중 이벤트를 내도 안 돈다.
	 * 같은 프레임에 여럿 맞았으면 HitSkill 은 시전자에게 **가장 가까운 한 명** (스트라이더 · 나무위키 본문 · Argument 80 D6 c)
	 */
	UPROPERTY(EditDefaultsOnly)
	bool bOnce = false;

	/** 적 **실험체** 적중만 (야생동물 X) — 스트라이더 "적 실험체에게 피해를 입힐 경우" (Argument 80 D6 b) */
	UPROPERTY(EditDefaultsOnly)
	bool bPlayersOnly = false;

	/** HitSkill 이 발동하면 WhileTag 버프 남은 시간을 이 초로 다시 (레벨별 · 0 = 안 함) — 스트라이더 "추가 피해 후 5초 동안 이동 속도 증가" (Argument 80 D6 T1) */
	UPROPERTY(EditDefaultsOnly)
	TArray<float> RefreshOnTrigger;

	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual FString GetDebugName() const override { return TEXT("버프 중 적중 시"); }
};
