// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ERWilson.generated.h"

class ACharacter;

/**
 * 윌슨 — 시셀라에게서 **떨어져 있는 동안만** 있는 액터 (F19-04 · Argument 68 W1).
 *
 * - Q 가 착지하면 서버가 Drop (이미 떨어져 있으면 옮긴다) · 시셀라 ASC 에 `State.Wilson.Away` (복제 루즈 태그)
 * - 서버 타이머(0.1초)로 거리: 줍기 반경 안 → 합침 · 복귀 거리 밖 → 합침 (줍기 · 거리)
 *   E (시셀라가 끌려옴) · W 는 그 스킬이 Join 을 부른다
 * - 합침 = 태그 제거 + `Event.Wilson.Joined` (패시브가 강화 평타 장전) + 파괴
 * - 붙어 있을 때는 **액터 없음** — 보이는 윌슨은 시셀라 연출 부착 (무기 · 소품 부착 작업 · Argument 64 미룸)
 * - 콜리전 없음 (판정은 거리) · 풀링 없음 (Q 한 번에 1개 — Argument 68 결정 보충)
 *
 * 모습은 BP 파생에서 메시를 붙인다 (Q 데이터가 클래스를 고른다). C++ 클래스만이면 안 보인다 — ER.Skill.DebugDraw 1 이면 구.
 */
UCLASS()
class ETERNALRETURN_API AERWilson : public AActor
{
	GENERATED_BODY()

public:
	AERWilson();

	/** [서버] Sissela 의 윌슨을 Location 에 내려놓는다 — 이미 떨어져 있으면 그 액터를 옮긴다. Class 가 비면 이 C++ 클래스 */
	static AERWilson* Drop(ACharacter* Sissela, const FVector& Location, TSubclassOf<AERWilson> Class = nullptr);

	/** 이 시셀라의 떨어진 윌슨 (없으면 null = 붙어 있음) */
	static AERWilson* FindFor(const AActor* Sissela);

	/**
	 * [서버] 시셀라를 윌슨 자리로 끌어온다 (E 가 시셀라를 맞힘) — 도착하면 합침 ("E").
	 * 끌려가는 중 Q 가 윌슨을 더 던지면 새 자리까지 다시 끈다 (사용자 2026-10-06 결정 7 · HoldPullForThrow → Drop)
	 */
	void PullOwner(float SpeedMps);

	/** [서버] Q 를 던졌다 — 끌려가는 중이면 도착 합침을 미루고 착지(Drop)를 기다린다 */
	void HoldPullForThrow();

	bool IsPullingOwner() const { return bPullingOwner; }

	/** [서버] Q 로 날아가는 중 — 숨기고 줍기 · 거리 확인을 멈춘다. 착지(Drop)하면 풀린다 */
	void SetInFlight(bool bInFlight);

	/** [서버] 합친다 — Reason 은 로그용 ("줍기" · "거리" · "E" · "W"). 패시브가 강화 평타 장전 (쿨 중이면 무시) */
	void Join(const TCHAR* Reason);

	/** 이만큼 다가가면 줍는다 (m) [자체 — 원작 "걸어가서 줍기" 반경 미확인] */
	UPROPERTY(EditDefaultsOnly, Category = "윌슨", meta = (ClampMin = "0"))
	float PickupRadius = 1.f;

	/** 이만큼 멀어지면 돌아온다 (m) [자체 — 옛 가이드 9.25m · 사용자 2026-10-06] */
	UPROPERTY(EditDefaultsOnly, Category = "윌슨", meta = (ClampMin = "0"))
	float ReturnRange = 9.25f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void CheckDistance();
	void SetAwayTag(bool bAway);

	FTimerHandle CheckTimer;
	FTimerHandle PullTimer;
	float PullSpeedMps = 15.f;
	bool bPullingOwner = false;
	bool bFlying = false;
	bool bAwayTagged = false;
	bool bJoined = false;
};
