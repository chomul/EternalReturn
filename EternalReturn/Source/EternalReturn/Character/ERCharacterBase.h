// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayCueInterface.h"  // 연출 큐 (F12.5-05) — 인터페이스라 전방 선언 불가
#include "GameplayEffectTypes.h"   // FOnAttributeChangeData
#include "GAS/ERSkillData.h"        // FERGrantedSkillHandles (USTRUCT 라 전방 선언 불가)
#include "ERCharacterBase.generated.h"

class UAbilitySystemComponent;
class UERCharacterData;
class UGameplayEffect;
class UCameraComponent;
class USpringArmComponent;
class UERPresentationComponent;
class UERForcedMoveComponent;
class UERRideComponent;
class AERPlayerState;

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
class AERCharacterBase : public ACharacter, public IAbilitySystemInterface, public IGameplayCueInterface
{
	GENERATED_BODY()

public:
	/**
	 * ⚠ FObjectInitializer 를 받는 이유 — **CMC 클래스를 교체하기 위해서다.**
	 *
	 *   ACharacter 의 CharacterMovement 는 부모 생성자가 이미 만들어 둔
	 *   기본 서브오브젝트라, SetDefaultSubobjectClass 로 **생성자에서만**
	 *   클래스를 바꿀 수 있다. 런타임에는 바꿀 방법이 없다.
	 *   이동 차단(F06)이 UERCharacterMovementComponent 에 들어 있다.
	 */
	AERCharacterBase(const FObjectInitializer& ObjectInitializer);

	/**
	 * IAbilitySystemInterface — PlayerState 의 ASC 를 그대로 돌려준다.
	 * 호출부가 "ASC 가 PlayerState 에 있는지 Pawn 에 있는지"를 몰라도 되게 하는 것이
	 * 이 인터페이스의 존재 이유다. 야생동물은 자기 Pawn 의 ASC 를 돌려주게 된다.
	 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** IGameplayCueInterface — 연출 큐(`GameplayCue.Pres.*`)를 연출 컴포넌트로 (F12.5-05 · Argument 49). 큐 애셋 없이 C++ 로 받는다 (GameplayCueManager.cpp:222). */
	using IGameplayCueInterface::HandleGameplayCue;
	virtual void HandleGameplayCue(UObject* Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) override;

	/** 이 캐릭터의 실험체 데이터. 장착 검사(F08-02) 등이 읽는다. 초기화 전엔 nullptr 일 수 있다. */
	const UERCharacterData* GetCharacterData() const { return CharacterData; }

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

	/**
	 * 카메라 거리 배율 (F11-05 D 저격 모드 — "사거리가 다 보이게 줌아웃", 사용자 확인 2026-09-21). 1 = 기본(2050). 로컬 전용.
	 * ⚠ 즉시 바뀐다 — 부드러운 줌은 연출(F17)에서.
	 */
	void SetCameraZoomScale(float Scale);

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
	 * ⚠ 크기(130)는 여전히 **미확정**이다. 화면 기울기는
	 *     tan(화면각) = tan(월드각 - Yaw) x sin(피치)
	 *   라서 Yaw 와 피치에 **동시에** 의존한다. 피치를 먼저 확정해야 풀린다.
	 */
	static constexpr float CameraYaw = -130.f;

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
	 * [서버] CharacterData 의 스킬을 ASC 에 부여한다.
	 *
	 * ⚠ InitDefaultStats 와 같은 빗장(bSkillsGranted)이 있다 — InitAbilityActorInfo 가
	 *   여러 번 불리므로 없으면 스킬이 **중복 부여**되어 Q 가 두 번 나간다.
	 * ⚠ 서버 전용. 클라는 ASC 가 스펙을 복제해서 받는다.
	 */
	void GrantSkills();

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
	 * 연출 연결 (F12.5-01) — 기본 표 · 스킨 · 장착 무기를 연출 컴포넌트에 넣고, 스킨 · 무기 변경을 구독한다.
	 * ⭐ 서버 · 클라 양쪽 (InitAbilityActorInfo 에서). 서버도 애니를 골라 재생하고, 클라는 메시 · 소리를 바꾼다.
	 */
	void InitPresentation(AERPlayerState* PS);
	void ApplyPresentationSkin();
	void ApplyPresentationWeapon();

	FDelegateHandle SkinChangedHandle;
	FDelegateHandle EquippedChangedHandle;

	/** 연출 해석기 (Argument 39). 복제 없음 — 각 머신이 같은 스킨 · 무기로 같은 답을 낸다. */
	UPROPERTY(VisibleAnywhere, Category = "연출")
	TObjectPtr<UERPresentationComponent> Presentation;

	/** 강제 이동 받기 — 전파 · 벽 감시 · 벽 충돌 알림 (Argument 44 K1 · 야생동물과 공용). */
	UPROPERTY(VisibleAnywhere, Category = "전투")
	TObjectPtr<UERForcedMoveComponent> ForcedMove;

	/** 탑승 이동 (매그너스 R 바이크 · Argument 62 V3) — 탈 때만 틱 */
	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UERRideComponent> Ride;

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

	/** ⭐ 스킬을 이미 부여했는가. bDefaultStatsApplied 와 같은 이유의 빗장. */
	bool bSkillsGranted = false;

	/**
	 * 부여한 스킬의 핸들. 회수할 때 쓴다 (F11 무기 교체 · 사망 시 정리).
	 *
	 * ⚠ USTRUCT 라 UPROPERTY 로 든다. 안 그러면 핸들 배열이 GC 추적 밖이다.
	 */
	UPROPERTY()
	FERGrantedSkillHandles GrantedSkills;

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
