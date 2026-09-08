// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ERAttributeSet.generated.h"

/**
 * 어트리뷰트 하나에 접근자 4종을 만들어 준다.
 * (AttributeSet.h 주석이 제시하는 관례 그대로다)
 */
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * 모든 액터가 공유하는 스탯.
 *
 * 실험체·야생동물·보스가 전부 이 세트를 쓴다. 캐릭터 전용 필드를 넣지 않는다.
 *
 * ⭐ 에키온의 VF 게이지처럼 **한 캐릭터만 쓰는 리소스는 여기 넣지 않는다.**
 *   별도 AttributeSet 을 만들어 ASC 에 **병렬로 추가**한다
 *   (ASC 는 SpawnedAttributes 배열로 여러 세트를 지원한다).
 *
 * ⚠ **이 클래스를 상속해서 캐릭터별 세트를 만들지 마라.**
 *   ASC 의 세트 조회가 IsA() 라(AbilitySystemComponent.cpp GetAttributeSubobject),
 *   부모와 자식을 둘 다 등록하면 배열 순서에 따라 아무거나 반환된다.
 *   컴파일도 되고 에러도 없이 조용히 틀린 세트를 쓴다.
 *   근거: Docs/4_Argument/3_어트리뷰트셋_구조.md E절
 *
 * ⚠ **어트리뷰트 이름을 나중에 바꾸지 마라.** GameplayEffect 애셋이 이름으로 참조한다.
 *   바꾸면 애셋이 조용히 끊긴다 (CLAUDE.md §3).
 *
 * ⚠ 값은 코드에서 직접 Set 하지 않는다. 초기화도 GameplayEffect 로 한다 —
 *   직접 세팅하면 클램프(PreAttributeChange)와 PostGameplayEffectExecute 를 우회한다.
 */
UCLASS()
class UERAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UERAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * 최종값(Current)이 바뀌기 직전. **여기서 클램프만 한다.**
	 *
	 * 엔진 주석: "meant to enforce things like Health = Clamp(Health, 0, MaxHealth)
	 * and NOT things like trigger this extra thing if damage is applied" (AttributeSet.h)
	 *
	 * ⚠ 사망 처리·이펙트를 여기 넣지 않는다. 그건 PostGameplayEffectExecute 다.
	 */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/**
	 * Base 값이 바뀌기 직전. 같은 클램프를 건다.
	 *
	 * Current 만 자르면 Base 에 100 을 넘긴 값이 남아, 버프가 빠질 때 그 값이 튀어나온다.
	 * ⚠ 엔진 주석이 여기서 게임플레이 이벤트를 부르지 말라고 명시한다.
	 */
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	/**
	 * Base 값을 바꾸는 GE 가 **실행된 직후**. IncomingDamage 를 체력 감소로 바꾼다.
	 *
	 * ⚠ 엔진 주석: "only called during an execute ... **It is not called during an
	 * application of a GameplayEffect, such as a 5 second +10 movement speed buff**"
	 * 즉 즉발 피해에서만 불린다 — 정확히 우리가 원하는 동작이다.
	 */
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	/** 최대치가 바뀐 뒤 현재값을 맞춘다. 클램프가 아니라 **반응**이라 여기 있다. */
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	/**
	 * 체력이 0 이 됐다. **사망 처리는 여기서 하지 않는다** — 알리기만 한다.
	 *
	 * 이 세트는 야생동물·보스도 쓴다. 여기서 액터를 죽이면 재사용이 안 된다.
	 * 듣는 쪽(캐릭터·야생동물)이 각자 반응한다.
	 *
	 * 인자는 마지막 피해를 준 액터. 킬 로그·경험치 분배가 쓴다. 없을 수 있다(환경 피해).
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnOutOfHealth, AActor* /*Instigator*/);
	mutable FOnOutOfHealth OnOutOfHealth;

private:
	/**
	 * ⭐ OnOutOfHealth 를 **한 번만** 쏘기 위한 빗장.
	 *
	 * 이게 없으면 체력이 이미 0 인 대상에 피해가 또 들어올 때마다 델리게이트가 다시 나간다.
	 * 같은 프레임에 투사체 여러 개가 닿거나 도트 피해가 겹치면 바로 발생한다.
	 * 킬 로그가 두 번 찍히고 경험치가 두 번 나간다.
	 *
	 * 부활해서 체력이 0 보다 커지면 PostAttributeChange 가 다시 내린다.
	 * 참고: Lyra 도 같은 방식이다 (Docs/6_Lyra참조/01_어트리뷰트셋.md)
	 */
	bool bOutOfHealth = false;

	/** 클램프 규칙 한 곳. PreAttributeChange 와 PreAttributeBaseChange 가 같은 규칙을 쓴다. */
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

public:

	// ── 기초 ────────────────────────────────────────────────
	/** 현재 체력. 0 이 되면 사망 델리게이트가 나간다 (사망 처리는 듣는 쪽이 한다). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HP, Category = "Vital")
	FGameplayAttributeData HP;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, HP)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHP, Category = "Vital")
	FGameplayAttributeData MaxHP;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, MaxHP)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HPRegen, Category = "Vital")
	FGameplayAttributeData HPRegen;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, HPRegen)

	/**
	 * 현재 기력.
	 * ⚠ 6인 중 기력을 쓰는 스킬이 없고 수치도 전부 미확인이다. 자리만 만들어 둔다.
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_VP, Category = "Vital")
	FGameplayAttributeData VP;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, VP)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxVP, Category = "Vital")
	FGameplayAttributeData MaxVP;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, MaxVP)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_VPRegen, Category = "Vital")
	FGameplayAttributeData VPRegen;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, VPRegen)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower, Category = "Base")
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, AttackPower)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Defense, Category = "Base")
	FGameplayAttributeData Defense;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, Defense)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackSpeed, Category = "Base")
	FGameplayAttributeData AttackSpeed;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, AttackSpeed)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeed, Category = "Base")
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, MoveSpeed)

	/** 시야 반경. 팀 시야 시스템이 읽는다. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Sight, Category = "Base")
	FGameplayAttributeData Sight;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, Sight)

	/** 기본 공격 사거리. 무기 계열이 결정한다. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackRange, Category = "Base")
	FGameplayAttributeData AttackRange;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, AttackRange)

	// ── 공격 파생 ───────────────────────────────────────────
	/** ⚠ 치명타는 기본 공격 채널에만 적용된다. 스킬에는 절대 붙지 않는다. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritChance, Category = "Offense")
	FGameplayAttributeData CritChance;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, CritChance)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritDamageUp, Category = "Offense")
	FGameplayAttributeData CritDamageUp;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, CritDamageUp)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SkillAmp, Category = "Offense")
	FGameplayAttributeData SkillAmp;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, SkillAmp)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BasicAtkAmp, Category = "Offense")
	FGameplayAttributeData BasicAtkAmp;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, BasicAtkAmp)

	/** ⚠ 관통은 퍼센트를 먼저, 고정을 나중에 적용한다. 순서가 바뀌면 결과가 달라진다. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DefPenPercent, Category = "Offense")
	FGameplayAttributeData DefPenPercent;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, DefPenPercent)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DefPenFlat, Category = "Offense")
	FGameplayAttributeData DefPenFlat;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, DefPenFlat)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageUp, Category = "Offense")
	FGameplayAttributeData DamageUp;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, DamageUp)

	/**
	 * ⚠ 최종 피해 추가(%)의 해석이 확인되지 않았다 — (1+x) 인지 x 자체인지.
	 *   해석에 따라 피해량이 배 단위로 달라진다. 데미지 계산에서 적용을 보류 중이다.
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FinalDamageUpPercent, Category = "Offense")
	FGameplayAttributeData FinalDamageUpPercent;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, FinalDamageUpPercent)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FinalDamageUpFlat, Category = "Offense")
	FGameplayAttributeData FinalDamageUpFlat;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, FinalDamageUpFlat)

	/** 스킬 가속. 쿨다운 환산은 Cooldown GameplayEffect 가 한다. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SkillHaste, Category = "Offense")
	FGameplayAttributeData SkillHaste;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, SkillHaste)

	// ── 방어 파생 ───────────────────────────────────────────
	/** ⚠ 100%를 넘기면 맞을 때마다 체력이 차오른다. 상한을 반드시 건다. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageDown, Category = "Defense")
	FGameplayAttributeData DamageDown;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, DamageDown)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BasicAtkDamageDown, Category = "Defense")
	FGameplayAttributeData BasicAtkDamageDown;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, BasicAtkDamageDown)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SkillDamageDown, Category = "Defense")
	FGameplayAttributeData SkillDamageDown;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, SkillDamageDown)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SlowResist, Category = "Defense")
	FGameplayAttributeData SlowResist;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, SlowResist)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CCResist, Category = "Defense")
	FGameplayAttributeData CCResist;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, CCResist)

	// ── 유지력 ──────────────────────────────────────────────
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Lifesteal, Category = "Sustain")
	FGameplayAttributeData Lifesteal;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, Lifesteal)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_OmniLifesteal, Category = "Sustain")
	FGameplayAttributeData OmniLifesteal;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, OmniLifesteal)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealAmp, Category = "Sustain")
	FGameplayAttributeData HealAmp;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, HealAmp)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_OutOfCombatRegen, Category = "Sustain")
	FGameplayAttributeData OutOfCombatRegen;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, OutOfCombatRegen)

	// ── 모드 보정 ───────────────────────────────────────────
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ModeDamageUp, Category = "Mode")
	FGameplayAttributeData ModeDamageUp;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, ModeDamageUp)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ModeDamageDown, Category = "Mode")
	FGameplayAttributeData ModeDamageDown;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, ModeDamageDown)

	// ── Meta ────────────────────────────────────────────────
	/**
	 * [Meta] 이번 한 번의 피해량. 저장되는 값이 아니라 한 번 쓰고 버리는 통로다.
	 *
	 * 데미지 ExecutionCalculation 이 여기에 결과를 쓰고,
	 * PostGameplayEffectExecute 가 읽어서 HP 에서 뺀 뒤 **0 으로 비운다.**
	 *
	 * ⭐ 복제하지 않는다. 서버에서만 존재한다 —
	 *   보내면 다른 플레이어가 받을 피해량이 노출된다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UERAttributeSet, IncomingDamage)

protected:
	UFUNCTION() void OnRep_HP(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHP(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HPRegen(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_VP(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxVP(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_VPRegen(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_AttackPower(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Defense(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_AttackSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Sight(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_AttackRange(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CritChance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CritDamageUp(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SkillAmp(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_BasicAtkAmp(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DefPenPercent(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DefPenFlat(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DamageUp(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_FinalDamageUpPercent(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_FinalDamageUpFlat(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SkillHaste(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DamageDown(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_BasicAtkDamageDown(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SkillDamageDown(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SlowResist(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CCResist(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Lifesteal(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_OmniLifesteal(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HealAmp(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_OutOfCombatRegen(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ModeDamageUp(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ModeDamageDown(const FGameplayAttributeData& OldValue);
};
