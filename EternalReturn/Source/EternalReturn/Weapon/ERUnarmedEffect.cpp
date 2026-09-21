// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapon/ERUnarmedEffect.h"
#include "GAS/ERGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UERUnarmedEffect::UERUnarmedEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// 5.3+ 는 부여 태그를 컴포넌트로 든다 (InheritableOwnedTagsContainer 는 deprecated).
	// ⚠ FindOrAddComponent 는 NewObject 라 **생성자에서 쓰면 Fatal** ("NewObject with empty name can't be used to create default subobjects").
	//   생성자에서는 CreateDefaultSubobject 로 만들어 GEComponents 에 넣는다 (E16).
	UTargetTagsGameplayEffectComponent* Tags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	FInheritedTagContainer Granted;
	Granted.AddTag(ERTags::State_Unarmed);
	Tags->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(Tags);
}
