<<<<<<< HEAD
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ERPlayerState.h"
#include "AbilitySystemComponent.h"

AERPlayerState::AERPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// ⭐ Mixed: 소유 클라에는 전체 정보, 나머지 클라에는 최소 정보만 보낸다.
	//   ASC 기본값은 Full 이다(AbilitySystemComponent.cpp 생성자). 24명 매치에서
	//   남의 활성 이펙트 전체 목록까지 보내는 건 낭비다.
	//   Minimal 은 쓸 수 없다 — 엔진 주석이 소유자 있는 ASC 에서 동작하지 않는다고 명시한다
	//   (AbilitySystemComponent.h:87). 야생동물(소유 컨트롤러 없음)만 Minimal 을 쓴다.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// ⚠ APlayerState 기본값은 1 이다(PlayerState.cpp:26). 그대로 두면
	//   체력바가 초당 1회 갱신되어 전투가 성립하지 않는다.
	//   ⚠ 100 은 자체 결정 초기값이며 측정하지 않았다. APlayerState 는
	//   bAlwaysRelevant = true 라 24명분이 전원에게 가므로, 봇 24명 + NET ACTIVE +
	//   stat unit 으로 재본 뒤 낮춘다.
	NetUpdateFrequency = 100.f;
}

UAbilitySystemComponent* AERPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
=======
// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/ERPlayerState.h"




>>>>>>> 6100863f0a6ea62466e1603dfe2b39fdbd5c6654
