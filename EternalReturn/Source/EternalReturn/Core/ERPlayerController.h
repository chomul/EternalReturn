// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ERPlayerController.generated.h"

class UERInputConfig;
struct FInputActionValue;

/**
 * 소유 클라이언트와 서버에만 존재한다.
 *
 * AController 생성자가 bOnlyRelevantToOwner = true 로 시작하므로
 * (Controller.cpp:51) 남의 컨트롤러는 내 클라에 오지 않는다.
 * 즉 "나만 아는 것"을 두기 좋은 자리다.
 *
 * ⭐ 입력과 클릭 이동을 **폰이 아니라 여기** 둔 이유 (F05-01/02):
 *   1. 클릭 이동의 "목적지" 는 컨트롤러 수준의 개념이다. 폰이 죽어도 남는다
 *   2. 부활해도 재바인딩이 필요 없다 (폰에 두면 부활마다 다시 붙여야 한다)
 *   ⚠ Lyra 는 폰 쪽(ULyraHeroComponent)에 두는데, 그건 1인칭 슈터라
 *     입력이 폰 상태와 밀착해서다. 탑다운 클릭 이동은 반대다.
 */
UCLASS()
class AERPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AERPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	/** 입력 액션 묶음. 에디터에서 지정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "입력")
	TObjectPtr<UERInputConfig> InputConfig;

private:
	/** 우클릭 — 커서 아래 지점으로 이동한다. */
	void OnMoveToCursor();

	/**
	 * 목적지를 서버에 알린다.
	 *
	 * ⭐ **서버가 목적지를 검증한다.** 네비 위가 아니거나 터무니없이 멀면 거부한다.
	 *   근거: Docs/1_Task/F05_입력_카메라_이동/02_클릭이동_서버권위.md
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetDestination(const FVector& Destination);

	/** 실제 이동 시작. 서버·클라 각자 부른다. */
	void StartMoveTo(const FVector& Destination);

	/**
	 * 클릭 지점을 **실제로 갈 수 있는 지점**으로 바꾼다.
	 *
	 * ⚠ 커서 트레이스는 ECC_Visibility 라 **보이는 모든 것**에 맞는다.
	 *   상자·벽을 클릭하면 그 표면 좌표가 나오는데, 거기는 갈 수 없다.
	 *
	 * ⭐ **서버와 클라가 같은 함수를 쓴다.** 각자 계산해도 결과가 같아야
	 *   둘이 다른 곳으로 가지 않는다. (서버는 클라가 보낸 값을 믿지 않고 다시 계산한다)
	 *
	 * @return 갈 수 있는 지점을 찾으면 true
	 */
	bool ResolveNavigableDestination(const FVector& ClickPoint, FVector& OutDestination) const;

	/**
	 * 네비메시 투영 반경 (cm).
	 *
	 * ⚠ Z 가 넉넉해야 상자 위를 클릭했을 때 아래 바닥을 찾는다.
	 *   너무 크면 다른 층 바닥으로 튈 수 있다 - 다층 구조가 생기면 다시 본다 (미확인).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "입력")
	FVector NavProjectionExtent = FVector(300.f, 300.f, 500.f);

	/** 위 투영이 실패했을 때 아래로 훑을 거리 (cm). 높은 벽을 클릭한 경우다. */
	UPROPERTY(EditDefaultsOnly, Category = "입력", meta = (ClampMin = "0.0"))
	float GroundTraceDownDistance = 3000.f;

	// ═══════════════════════════════════════════════════════════
	//  카메라 (F05-03)
	// ═══════════════════════════════════════════════════════════
	//
	// ⭐ **리그는 캐릭터가, 상태는 여기가 갖는다** (방안 C).
	//   근거: Docs/4_Argument/8_카메라_각도거리_소유주체.md 파트 2
	//
	//   캐릭터에 두면 부활할 때마다 잠금 상태·스크롤 위치가 날아가고,
	//   컨트롤러에 리그를 통째로 옮기면 F01 에서 만든 멀쩡한 것을 버리게 된다.
	//   컨트롤러는 폰과 수명이 달라서 상태를 두기 좋은 자리다.
	//
	// ⚠ 카메라는 **로컬 전용**이다. 복제하지 않는다.

	/** 잠금이면 카메라가 캐릭터를 따라간다. 해제하면 제자리에 남는다. */
	bool bCameraLocked = true;

	/** 해제 상태에서 캐릭터로부터 얼마나 밀려나 있는가 (월드, cm). */
	FVector FreeCameraOffset = FVector::ZeroVector;

	void OnToggleCameraLock();

	/** 마우스가 화면 가장자리에 있으면 오프셋을 민다. 해제 상태에서만 돈다. */
	void UpdateEdgeScroll(float DeltaSeconds);

	/** 현재 오프셋을 캐릭터의 SpringArm 에 반영한다. */
	void ApplyCameraOffset();

	/** 가장자리로 판정할 화면 가장자리 두께 (픽셀). */
	UPROPERTY(EditDefaultsOnly, Category = "카메라", meta = (ClampMin = "1.0"))
	float EdgeScrollMargin = 24.f;

	/** 가장자리 스크롤 속도 (cm/s). */
	UPROPERTY(EditDefaultsOnly, Category = "카메라", meta = (ClampMin = "1.0"))
	float EdgeScrollSpeed = 2000.f;

	/**
	 * 캐릭터에서 최대 얼마까지 밀 수 있나 (cm).
	 * ⚠ 없으면 무한히 밀려나 캐릭터를 잃어버린다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "카메라", meta = (ClampMin = "0.0"))
	float MaxCameraOffset = 3000.f;

	/**
	 * ⚠ 가장자리 스크롤을 끌 수 있어야 한다.
	 *   창 모드·듀얼 모니터에서 **의도치 않게 발동**한다 (역기획서 §10).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "카메라")
	bool bEdgeScrollEnabled = true;

	/**
	 * 목적지 검증에 쓰는 최대 거리(cm).
	 *
	 * ⚠ 화면에 보이는 범위보다 넉넉해야 한다. 너무 좁으면 정상 클릭이 거부된다.
	 * 자체 결정값 — 카메라 거리가 확정되면 다시 본다 (미확인).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "입력", meta = (ClampMin = "100.0"))
	float MaxClickDistance = 5000.f;
};
