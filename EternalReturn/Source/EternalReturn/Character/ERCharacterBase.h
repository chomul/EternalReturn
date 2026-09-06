<<<<<<< HEAD
// Copyright Epic Games, Inc. All Rights Reserved.
=======
// Fill out your copyright notice in the Description page of Project Settings.
>>>>>>> 6100863f0a6ea62466e1603dfe2b39fdbd5c6654

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
<<<<<<< HEAD
#include "AbilitySystemInterface.h"
#include "ERCharacterBase.generated.h"

class UAbilitySystemComponent;

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
=======
#include "ERCharacterBase.generated.h"

UCLASS()
class ETERNALRETURN_API AERCharacterBase : public ACharacter
>>>>>>> 6100863f0a6ea62466e1603dfe2b39fdbd5c6654
{
	GENERATED_BODY()

public:
<<<<<<< HEAD
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

protected:
	/**
	 * ⭐ 서버와 클라 양쪽에서 각각 불러야 한다.
	 *   한쪽만 부르면 그쪽에서만 어빌리티가 동작한다 —
	 *   "서버에선 되는데 클라에선 안 된다"의 가장 흔한 원인이다.
	 *   부활로 폰이 새로 생기면 Avatar 가 바뀌므로 다시 불린다.
	 */
	void InitAbilityActorInfo();
=======
	// Sets default values for this character's properties
	AERCharacterBase();

	// Called every frame.
	virtual void Tick(float DeltaSeconds) override;

	/** Returns TopDownCameraComponent subobject **/
	FORCEINLINE class UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent; }
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

protected:

	virtual void BeginPlay() override;

private:
	/** Top down camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* TopDownCameraComponent;

	/** Camera boom positioning the camera above the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;	
	
>>>>>>> 6100863f0a6ea62466e1603dfe2b39fdbd5c6654
};
