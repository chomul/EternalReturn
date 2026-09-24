// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ERWildlifeAIController.generated.h"

class AERWildlifeCharacter;

/** 야생동물 AI 상태 (F12-04). 셋뿐이다 — 역기획서 §6.5. */
UENUM()
enum class EERWildlifeAIState : uint8
{
	/** 자리에 서 있다. 타이머 없음 · 몸 틱 꺼짐 · DormantAll. 피격으로만 깨어난다 (선공 없음). */
	Idle,
	/** 어그로 팀의 가장 가까운 생존자를 쫓아 평타. */
	Combat,
	/** 자리로 돌아간다. 맞아도 돌아서지 않는다 · 도착하면 체력 회복 → Idle. */
	Return,
};

/**
 * 야생동물 AI (F12-04 · Argument 35 A — C++ 상태 머신). **서버에만 있다** (§7.2 — AIController 는 클라에 복제되지 않는다).
 *
 * 원작 규칙 [확인] (사용자 2026-09-24 · 역기획서 §6.5):
 *   - 선공 없음 — 맞기 전엔 가만히 있다 (그래서 감지 · Perception 이 없다)
 *   - 어그로는 **사람이 아니라 팀**에 붙는다 — 나를 때린 팀에서 **가장 가까운 사람**을 매번 다시 고른다
 *   - 다른 팀이 끼어들어도 **먼저 때린 팀 우선** · 무리는 전체가 같이 온다
 *   - 자리에서 **10m** 넘게 끌려가면 귀환 — 귀환 중 맞아도 돌아서지 않고, 도착하면 체력 회복
 * ⭐ 도먼시 · 몸 틱은 상태 전이(SetState) 한 곳에서 — AERWildlifeCharacter::SetBodyActive.
 * ⭐ 공격은 플레이어와 같은 경로 — 평타 슬롯 어빌리티를 TargetData 로 발동 (새 공격 경로 없음).
 */
UCLASS()
class ETERNALRETURN_API AERWildlifeAIController : public AAIController
{
	GENERATED_BODY()

public:
	AERWildlifeAIController();

	/** [서버] 피격 알림 — AERWildlifeCharacter 가 OnDamageTaken 에서 부른다. Attacker = 피해 GE 의 원래 시전자(PlayerState · 폰). */
	void NotifyDamaged(AActor* Attacker);
	/** [서버] 사망 — 판단 · 이동을 멈춘다 (시체 상태는 캐릭터가). */
	void NotifyPawnDied();
	/** [서버] 반경(m) 안 가장 가까운 실험체의 팀으로 전투 진입 — 없으면 아무것도 안 한다. 위클라인 스폰 선공 (DA.SpawnAggroRadius). */
	void AggroNearestInRadius(float Meters);

	/** [서버] 대기 → 전투. 무리 전파 · 디버그(ER.Wild.Aggro)도 여기로. */
	void EnterCombat(int32 TeamId, bool bPropagateToPack, const TCHAR* Reason = nullptr);

	EERWildlifeAIState GetAIState() const { return State; }
	int32 GetAggroTeam() const { return AggroTeam; }
	static const TCHAR* StateName(EERWildlifeAIState InState);

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	void SetState(EERWildlifeAIState NewState, const TCHAR* Reason);
	void Think();
	APawn* PickTarget() const;
	void TryAttack(AActor* Target);
	void Heal();
	AERWildlifeCharacter* GetWildlife() const;

	EERWildlifeAIState State = EERWildlifeAIState::Idle;
	int32 AggroTeam = INDEX_NONE;
	TWeakObjectPtr<AActor> MoveTarget;
	FAIRequestID ReturnRequestId;
	FTimerHandle ThinkTimer;
	bool bStopped = false;
	bool bWarnedNoAttack = false;
};
