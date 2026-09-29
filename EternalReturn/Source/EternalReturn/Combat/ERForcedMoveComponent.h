// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ERForcedMoveComponent.generated.h"

class ACharacter;

/**
 * 강제 이동(넉백 · 그랩 · 자기 이동)을 **받는 쪽**의 몫 — 각 머신 전파 · 서버 벽 감시 · 벽 충돌 알림.
 *
 * ⭐ 실험체와 야생동물이 **같은 컴포넌트**를 든다 (Docs/4_Argument/44 K1).
 *   전에는 AERCharacterBase 에 있어서 야생동물은 밀리지 않았다. 이동 계산 자체는 ERForcedMove 네임스페이스.
 *
 * ⚠ 소유자는 ACharacter 여야 한다 (CMC 의 RootMotionSource 를 쓴다).
 * ⚠ 복제 컴포넌트다 — 컴포넌트의 RPC 는 컴포넌트가 복제돼야 간다. 복제 속성은 없다.
 */
UCLASS(ClassGroup = (Combat))
class ETERNALRETURN_API UERForcedMoveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UERForcedMoveComponent();

	/**
	 * 강제 이동을 각 머신에 전파한다.
	 *
	 * ⭐⭐ **왜 Multicast 인가**: CMC 의 `CurrentRootMotion` 은 복제되지 않는다
	 *   (`CharacterMovementComponent.h:2677`, Transient). 서버에서만 소스를 붙이면
	 *   클라는 모르고, 서버가 위치를 교정하면서 **러버밴딩이 난다** (역기획서 §3.5).
	 *   그래서 파라미터를 뿌려 **각 머신이 같은 소스를 자기 CMC 에 붙인다.**
	 *
	 * ⚠ **시작·목표 위치를 그대로 보낸다.** 방향·거리를 보내고 각자 계산하게 하면
	 *   기준 위치가 미세하게 달라 결과가 어긋난다.
	 *
	 * ⚠ Reliable 이다. 놓치면 그 클라만 캐릭터가 안 밀린다.
	 *
	 * 근거: Docs/4_Argument/13_강제이동_구현방식.md (방안 B)
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ForcedMove(const FVector& StartLocation, const FVector& TargetLocation, float Duration);

	/** 강제 이동 중단(벽 충돌)을 전파한다. 서버가 판정하고 각 머신이 소스를 뗀다. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StopForcedMove();

	/**
	 * [서버] 강제 이동 중 벽에 부딪혔다.
	 *
	 * ⭐ **무엇을 할지는 여기가 정하지 않는다.** 추가 피해·기절은 스킬마다 달라서
	 *   (매그너스 E 와 레니 R 의 피해량이 다르다) 구독하는 쪽(넉백 조각)이 정한다.
	 *
	 * 근거: 역기획서 §3.5 — "충돌 시 추가 피해 + 기절 부여 후 NetMulticast 로 연출"
	 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnForcedMoveWallImpact, ACharacter*, const FHitResult&);
	FOnForcedMoveWallImpact OnForcedMoveWallImpact;

	/**
	 * [서버] 강제 이동 중 벽 감시를 시작한다. `ERForcedMove::ApplyForcedMove` 가 부른다.
	 *
	 * ⭐ **Tick 을 이때만 켠다.** 역기획서 §3.5 가 "이동 중 매 틱 지오메트리 트레이스" 를
	 *   요구하는데, 넉백은 0.4초 남짓이라 평소에 Tick 을 돌릴 이유가 없다.
	 *   (`CLAUDE.md` §2 — "Tick 은 정말 매 프레임 필요할 때만")
	 */
	void StartWatch();

	/** ⚠ 강제 이동 중에만 돈다. 평소에는 꺼져 있다 — StartWatch 참조. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	ACharacter* GetCharacter() const;
};
