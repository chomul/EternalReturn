// F11.5 조각 — 모드 (F11-05 D 저격총 · Argument 27 ①A + ②A)

#pragma once

#include "CoreMinimal.h"
#include "GAS/Fragment/ERSkillFragment.h"
#include "GAS/ERSkillData.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "ERSkillFragment_Mode.generated.h"

class UERSkillFragment_Mode;
class UERGameplayAbility;

/** 모드 런타임 상태 — 어빌리티 인스턴스에 붙는다. 조각(애셋)은 상태를 못 든다. */
UCLASS()
class UERSkillModeState : public UERSkillFragmentState
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<const UERSkillFragment_Mode> Frag;
	bool bActive = false;
	int32 Shots = 0;
	FDelegateHandle EndedHandle;

	UFUNCTION() void OnExpired();
	UFUNCTION() void OnCancelledByCC();
	UFUNCTION() void OnCancelledByMove(FGameplayEventData Payload);
	void OnAnyAbilityEnded(const FAbilityEndedData& Data);

	/** 평타 복구 · 구독 해제 · 미사용 쿨 반환. 두 번 불려도 안전. */
	void Cleanup(const TCHAR* Reason);
	/** Cleanup + 어빌리티 종료. */
	void Exit(const TCHAR* Reason);

	UERGameplayAbility* GetAbility() const;
};

/**
 * 발동 뒤 어빌리티를 Duration 동안 **활성**으로 남기고 기본 공격 슬롯을 AttackData 로 갈아끼운다 (AERPlayerState::SetModeAttack).
 * 발수 Shots−1 에 FinalAttackData 로 교체, 다 쏘면 · 시간 · (bCancelOnMove) 이동 · CC 에 해제 → 평타 복구. 0발이면 쿨 UnusedCooldownRefund 반환.
 * 카메라 줌/오프셋은 OnLocalExecute (소유 클라), 조준 ±각 제한은 PS 가 들고 ResolveAim 이 자른다.
 * 원작 확인 (사용자 2026-09-21): 모드 중 D 재입력/좌클릭 = 사격 · 우클릭 = 해제 · 줌아웃 · 좌우 30°.
 */
UCLASS(DisplayName = "모드")
class ETERNALRETURN_API UERSkillFragment_Mode : public UERSkillFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	FERSkillMode Mode;

	virtual void OnExecute(FERSkillContext& Ctx) const override;
	virtual void OnLocalExecute(FERSkillContext& Ctx) const override;
	virtual void OnEnd(FERSkillContext& Ctx, bool bCancelled) const override;
	virtual bool KeepsAbilityActive() const override { return Mode.Duration > 0.f; }
	virtual FString GetDebugName() const override { return TEXT("모드"); }
};
