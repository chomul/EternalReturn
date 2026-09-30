// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ERAnimInstance.generated.h"

class UAnimMontage;

/**
 * 메인 AnimBP 템플릿(`ABP_ERCharacter_Base` · `ABP_ERWildlife_Base`)의 부모 (F12.5-02 · Argument 40 ⑥ N2).
 *
 * ⭐ AnimGraph 가 읽을 **상태 값은 여기서 계산한다** (CLAUDE.md §7 — 상태 계산은 C++ · BP 는 연출).
 *   게임 스레드에서는 **읽기만**(NativeUpdateAnimation), 계산은 워커 스레드(NativeThreadSafeUpdateAnimation) —
 *   엔진 권장: "gather data in this step and ... the bulk of the work ... in NativeThreadSafeUpdateAnimation" (AnimInstance.h:1307-1308).
 * ⭐ AnimGraph 는 아래 변수를 **직접** 읽는다 (Fast Path — 공식 문서 Animation Optimization). BP 쪽 Update 그래프는 비워 둔다.
 * ⚠ 무기 레이어(`ABPL_*`)는 계산이 없어 이 클래스를 쓰지 않는다 — 부모 AnimInstance.
 * ⭐ 04 상태 포즈 — 사망(`bDead` · 야생동물). 채집은 몽타주 (Argument 47).
 * ⭐ 규칙 (Argument 48): 상태(이동 · 모드 · 시체) = 상태머신 / 명령(평타 · 스킬 · 채집 …) = 몽타주 · **움직이면 몽타주를 끊는 곳은 여기 한 곳**. 기절 · 속박은 애셋이 없어 포즈가 없다 (2026-09-29 확인).
 * ⭐ 모드(저격 · 전기톱 …)도 **상태**다 (Argument 36 원칙 2 · Argument 42 ④ MB) — `bInMode` 로 상태머신이 진입 · 유지 · 해제를 돈다.
 *   그 애니(`ModeStartAnim` …)는 **동작표(DA)의 모드 줄**을 연출 컴포넌트가 해석해 넣는다 (Argument 42 ⑥ A2) — 상태머신은 Sequence Player 로 직접 읽는다.
 */
UCLASS()
class ETERNALRETURN_API UERAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** [게임 스레드] 모드 태그(`Mode.*`)가 붙었나 — 연출 컴포넌트가 태그를 구독해 넘긴다 (Argument 42 ④ MB). 다음 업데이트에 반영. */
	void SetInMode(bool bNewInMode) { bPendingInMode = bNewInMode; }

	/** [게임 스레드] 모드 애니 4개 (동작표의 모드 줄 · Argument 42 ⑥ A2). 모드가 끝나도 **지우지 않는다** — 해제(End) 상태가 아직 튼다. */
	void SetModeAnims(UAnimSequenceBase* Start, UAnimSequenceBase* Idle, UAnimSequenceBase* Run, UAnimSequenceBase* End)
	{
		PendingModeAnims[0] = Start; PendingModeAnims[1] = Idle; PendingModeAnims[2] = Run; PendingModeAnims[3] = End;
		bPendingModeAnims = true;
	}


	/** [게임 스레드] 사망 — 쓰러지는 애니 (`Pres.Anim.Death`). bSkipToEnd = 늦게 relevant 된 클라: 쓰러지는 과정 없이 **누운 채로** (마지막 프레임부터). */
	void SetDead(UAnimSequenceBase* Anim, bool bSkipToEnd) { PendingDeathAnim = Anim; bPendingDeathSkipToEnd = bSkipToEnd; bPendingDead = true; }

	/** [게임 스레드] 쉬는 자세 (F12.6-02 야생동물 경계 · 잠) + 애니 6개. null 이면 그 단계가 없는 종 — 상태머신이 bHas* 로 건너뛴다. */
	void SetRest(bool bInBeware, bool bInSleep, bool bSkipIntro, UAnimSequenceBase* BewareStart, UAnimSequenceBase* BewareLoop, UAnimSequenceBase* BewareEnd,
		UAnimSequenceBase* SleepStart, UAnimSequenceBase* SleepLoop, UAnimSequenceBase* Wake)
	{
		bPendingBeware = bInBeware; bPendingSleep = bInSleep; bPendingRestSkipIntro = bSkipIntro;
		PendingRestAnims[0] = BewareStart; PendingRestAnims[1] = BewareLoop; PendingRestAnims[2] = BewareEnd;
		PendingRestAnims[3] = SleepStart; PendingRestAnims[4] = SleepLoop; PendingRestAnims[5] = Wake;
		bPendingRest = true;
	}

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	/** 수평 속도 (cm/s). 이동 블렌드 · 상태 전이용. 속도 버프 · 둔화가 그대로 들어온다 (CMC MaxWalkSpeed ← MoveSpeed 어트리뷰트). */
	UPROPERTY(BlueprintReadOnly, Category = "이동")
	float GroundSpeed = 0.f;

	/** GroundSpeed > MoveThreshold. 대기 ↔ 뛰기 전이 조건. */
	UPROPERTY(BlueprintReadOnly, Category = "이동")
	bool bIsMoving = false;

	/** 특수 모드 중 (저격 D · 재키 R …). 모드별 애니는 무기 레이어의 ModeStart/Idle/Run/End 함수가 준다. */
	UPROPERTY(BlueprintReadOnly, Category = "모드")
	bool bInMode = false;

	/** 모드 애니 — 상태머신의 Mode 상태 안 Sequence Player 가 **직접** 읽는다 (Fast Path). 동작표 모드 줄에서 온다. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "모드")
	TObjectPtr<UAnimSequenceBase> ModeStartAnim;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "모드")
	TObjectPtr<UAnimSequenceBase> ModeIdleAnim;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "모드")
	TObjectPtr<UAnimSequenceBase> ModeRunAnim;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "모드")
	TObjectPtr<UAnimSequenceBase> ModeEndAnim;

	/** 사망 — 상태머신 Dead 상태 (한 번 들어가면 안 나온다). Sequence Player: 반복 끔 · Start Position = DeathStartPosition. */
	UPROPERTY(BlueprintReadOnly, Category = "상태")
	bool bDead = false;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "상태")
	TObjectPtr<UAnimSequenceBase> DeathAnim;
	/** 0 = 처음부터 쓰러진다 · 애니 길이 = 누운 마지막 프레임 (늦게 relevant). */
	UPROPERTY(BlueprintReadOnly, Category = "상태")
	float DeathStartPosition = 0.f;

	/**
	 * 쉬는 자세 (F12.6-02 · 야생동물) — 상태머신: 대기 ↔ 경계(시작 → 반복 → 끝) · 대기 ↔ 잠(시작 → 반복 → 깸). 움직이면 이동이 이긴다.
	 * ⭐ 전이 조건은 **변수 하나씩** — 조합은 여기서 미리 (Fast Path · F12.5 채집 bShowGather 와 같은 이유). 애니가 없는 단계는 건너뛰게 계산돼 있다
	 *   (들개 · 늑대 · 곰은 경계 시작 · 끝이 없다 · 늦게 받으면 시작 없이 반복부터).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "쉬는 자세")
	bool bBeware = false;
	UPROPERTY(BlueprintReadOnly, Category = "쉬는 자세")
	bool bSleep = false;
	/** 대기 → 경계 시작 */
	UPROPERTY(BlueprintReadOnly, Category = "쉬는 자세")
	bool bBewareIntro = false;
	/** 대기 → 경계 반복 (시작 애니 없음 · 늦게 받음) */
	UPROPERTY(BlueprintReadOnly, Category = "쉬는 자세")
	bool bBewareDirect = false;
	/** 경계 시작 · 반복 → 경계 끝 */
	UPROPERTY(BlueprintReadOnly, Category = "쉬는 자세")
	bool bBewareOutro = false;
	/** 경계 시작 · 반복 → 대기 (끝 애니 없음) */
	UPROPERTY(BlueprintReadOnly, Category = "쉬는 자세")
	bool bBewareQuit = false;
	/** 대기 → 잠 시작 */
	UPROPERTY(BlueprintReadOnly, Category = "쉬는 자세")
	bool bSleepIntro = false;
	/** 대기 → 잠 반복 (시작 애니 없음 · 늦게 받음 — 이미 자고 있던 곰) */
	UPROPERTY(BlueprintReadOnly, Category = "쉬는 자세")
	bool bSleepDirect = false;
	/** 잠 시작 · 반복 → 깸 */
	UPROPERTY(BlueprintReadOnly, Category = "쉬는 자세")
	bool bSleepOutro = false;
	/** 잠 시작 · 반복 → 대기 (깸 애니 없음) */
	UPROPERTY(BlueprintReadOnly, Category = "쉬는 자세")
	bool bSleepQuit = false;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "쉬는 자세")
	TObjectPtr<UAnimSequenceBase> BewareStartAnim;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "쉬는 자세")
	TObjectPtr<UAnimSequenceBase> BewareLoopAnim;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "쉬는 자세")
	TObjectPtr<UAnimSequenceBase> BewareEndAnim;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "쉬는 자세")
	TObjectPtr<UAnimSequenceBase> SleepStartAnim;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "쉬는 자세")
	TObjectPtr<UAnimSequenceBase> SleepLoopAnim;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "쉬는 자세")
	TObjectPtr<UAnimSequenceBase> WakeAnim;

	/** 이 속도(cm/s) 를 넘으면 움직이는 것으로 본다 [자체] — 도착 직전 미세 속도에서 뛰기가 깜빡이지 않게. */
	UPROPERTY(EditDefaultsOnly, Category = "이동", meta = (ClampMin = "0"))
	float MoveThreshold = 5.f;

	/** 움직여서 동작 몽타주를 끊을 때 블렌드 아웃 (초) [자체] — 지금까지 이동 끊기(StopActionAnim)와 같은 값. */
	UPROPERTY(EditDefaultsOnly, Category = "이동", meta = (ClampMin = "0"))
	float MoveStopBlendOut = 0.2f;

private:
	/** 게임 스레드에서 복사해 둔 속도 — 워커 스레드는 액터를 만지지 않고 이것만 읽는다. */
	FVector GatheredVelocity = FVector::ZeroVector;

	/** SetInMode 가 쓴 값 — 워커 스레드 계산 중에 바뀌지 않게 게임 스레드 업데이트에서 옮긴다. */
	bool bPendingInMode = false;

	/** SetModeAnims 가 쓴 값 — 게임 스레드 업데이트에서 위 UPROPERTY 로 옮긴다. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> PendingModeAnims[4];
	bool bPendingModeAnims = false;

	/** 이동 끊기 감시 중인 몽타주 · 시작 뒤 한 번 멈췄나 (Argument 48 ③ M1). */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> WatchedMontage;
	bool bWatchedSeenStill = false;

	/** SetDead 가 쓴 값 — 게임 스레드 업데이트에서 옮긴다. */
	bool bPendingDead = false;
	bool bPendingDeathSkipToEnd = false;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> PendingDeathAnim;

	/** SetRest 가 쓴 값 — 게임 스레드 업데이트에서 옮긴다. */
	bool bPendingRest = false;
	bool bPendingBeware = false;
	bool bPendingSleep = false;
	bool bPendingRestSkipIntro = false;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> PendingRestAnims[6];
};
