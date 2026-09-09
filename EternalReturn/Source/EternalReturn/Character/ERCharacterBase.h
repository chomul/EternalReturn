// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"   // FOnAttributeChangeData
#include "ERCharacterBase.generated.h"

class UAbilitySystemComponent;
class UERCharacterData;
class UGameplayEffect;
class UCameraComponent;
class USpringArmComponent;

/**
 * 모든 실험체의 베이스.
 *
 * ASC 는 이 클래스가 소유하지 않는다 — AERPlayerState 에 있다.
 * 다만 ASC 의 Avatar(월드에 실제로 서 있는 액터)는 이 폰이므로,
 * 초기화 시점을 아는 것도 이 폰이다.
 *
 * ⚠ 스탯·스킬·인벤토리 멤버를 여기에 넣지 않는다.
 *   각 기능이 자기 것을 들고 와서 붙는다.
 */
UCLASS()
class AERCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AERCharacterBase();

	/**
	 * IAbilitySystemInterface — PlayerState 의 ASC 를 그대로 돌려준다.
	 * 호출부가 "ASC 가 PlayerState 에 있는지 Pawn 에 있는지"를 몰라도 되게 하는 것이
	 * 이 인터페이스의 존재 이유다. 야생동물은 자기 Pawn 의 ASC 를 돌려주게 된다.
	 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** [서버] 컨트롤러가 이 폰을 소유한 직후 */
	virtual void PossessedBy(AController* NewController) override;

	/** [클라] PlayerState 가 복제되어 도착한 뒤 */
	virtual void OnRep_PlayerState() override;

	/**
	 * 카메라 오프셋을 리그에 반영한다. **컨트롤러가 부른다.**
	 *
	 * ⭐ 리그는 여기(캐릭터)가 갖고, 잠금 상태·오프셋은 컨트롤러가 갖는다.
	 *   컨트롤러는 폰과 수명이 달라서 부활해도 상태가 살아남는다.
	 *   근거: Docs/4_Argument/8_카메라_각도거리_소유주체.md 파트 2
	 *
	 * ⚠ 로컬 전용이다. 복제하지 않는다.
	 */
	void SetCameraTargetOffset(const FVector& Offset);

	/** 카메라가 보는 방향(월드 Yaw). 가장자리 스크롤이 이걸로 화면->월드 축을 계산한다. */
	float GetCameraYaw() const { return CameraYaw; }

	/**
	 * ⚠⚠ 카메라 Yaw. **0 이 아니다.**
	 *
	 * 원작은 월드 축에 대해 비스듬히 본다 - 같은 도로를 원작은 대각선으로,
	 * Yaw 0 인 우리는 직각으로 봤다. 근거: Docs/4_Argument/8_카메라_각도거리_소유주체.md
	 *
	 * ⭐ **찾아낸 과정** (Docs/4_Argument/8_카메라_각도거리_소유주체.md 파트 1-B):
	 *
	 *   0    -> 도로가 화면에서 **수평**. 원작은 대각선이다              -> Yaw != 0
	 *   -45  -> 도로가 **좌상->우하**. 원작은 좌하->우상이다             -> 부호 반대
	 *   +45  -> 기울기는 맞는데 **장면 전체가 정반대**                   -> 180 도 부족
	 *   -135 -> 기울기 유지 + 장면 방향 정상                            <- 현재
	 *
	 * ⚠⚠ **기울기만 보고 Yaw 를 정하면 180 도 틀린 채로 맞았다고 착각한다.**
	 *   지면의 **선은 방향이 없어서** tan 의 주기(180 도)만큼 같은 기울기가 나온다:
	 *     tan(월드각 - Yaw - 180) = tan(월드각 - Yaw)
	 *   기울기 외에 **장면의 좌우 배치**(어디에 잔디가 있고 어디에 건물이 있는지)를
	 *   같이 봐야 180 도 오류를 잡는다.
	 *
	 * ⚠ 크기(135)는 여전히 **미확정**이다. 화면 기울기는
	 *     tan(화면각) = tan(월드각 - Yaw) x sin(피치)
	 *   라서 Yaw 와 피치에 **동시에** 의존한다. 피치를 먼저 확정해야 풀린다.
	 */
	static constexpr float CameraYaw = -135.f;

protected:
	/**
	 * ⭐ 서버와 클라 양쪽에서 각각 불러야 한다.
	 *   한쪽만 부르면 그쪽에서만 어빌리티가 동작한다 —
	 *   "서버에선 되는데 클라에선 안 된다"의 가장 흔한 원인이다.
	 *   부활로 폰이 새로 생기면 Avatar 가 바뀌므로 다시 불린다.
	 */
	void InitAbilityActorInfo();

	/**
	 * 데이터 애셋에서 이 실험체의 1레벨 스탯을 읽어 초기화 GE 로 넣는다.
	 *
	 * ⚠ 서버 전용. 클라는 복제로 받는다.
	 * ⚠ InitAbilityActorInfo 가 끝난 뒤에만 부른다 - 그 전이면 조용히 실패한다.
	 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void InitDefaultStats();

	/**
	 * MoveSpeed 어트리뷰트를 CharacterMovementComponent 에 반영한다.
	 *
	 * ⭐ **Tick 으로 매 프레임 동기화하지 않는다.** F02-06 의 변경 델리게이트를 구독해
	 *   값이 **변할 때만** 갱신한다 (CLAUDE.md §2).
	 *
	 * ⚠ 서버·클라 **양쪽에서** 구독한다. 각자 자기 CMC 를 갱신해야
	 *   둔화(F06-02)가 양쪽에서 같이 보인다.
	 */
	void BindMoveSpeed();
	void OnMoveSpeedChanged(const FOnAttributeChangeData& Data);
	void ApplyMoveSpeed(float MetersPerSecond);

	/** 구독 해제용. 안 풀면 dangling 델리게이트가 남는다. */
	FDelegateHandle MoveSpeedHandle;

	/**
	 * 이 캐릭터가 어느 실험체인지.
	 *
	 * ⚠ 실험체 하나당 애셋 하나다. 안 쓰는 실험체는 로드되지 않는다.
	 *   근거: Docs/4_Argument/4_스탯데이터_저장방식.md
	 */
	UPROPERTY(EditDefaultsOnly, Category = "스탯")
	TObjectPtr<UERCharacterData> CharacterData;

	/**
	 * 초기 스탯을 넣는 Instant GameplayEffect.
	 *
	 * ⚠ 애셋이다(에디터에서 만든다). 모디파이어 33줄의 이름·순서·SetByCaller 키는
	 *   ERAttributeInit::ValidateInitEffect 가 검사해서 어긋나면 로그로 알린다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "스탯")
	TSubclassOf<UGameplayEffect> InitStatsEffect;

	/**
	 * ⭐ 초기 스탯을 이미 넣었는가.
	 *
	 * InitAbilityActorInfo 는 여러 번 불릴 수 있다(PossessedBy 재호출 등).
	 * 빗장이 없으면 그때마다 Override 로 다시 박혀 **전투 중에 체력이 만피로 돌아간다.**
	 */
	bool bDefaultStatsApplied = false;

	/**
	 * 탑다운 카메라 팔.
	 *
	 * 캐릭터가 어느 쪽을 보든 시점은 고정이다 — 절대 회전을 쓰고
	 * 폰 컨트롤 회전을 따라가지 않는다. 벽에 닿아도 카메라를 당기지 않는다
	 * (탑다운에서 시야가 갑자기 좁아지면 조작이 끊긴다).
	 *
	 * ⚠ 카메라는 로컬 전용이다. 복제하지 않는다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> TopDownCamera;
};
