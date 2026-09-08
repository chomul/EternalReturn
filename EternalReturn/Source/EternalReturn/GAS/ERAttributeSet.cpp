// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERAttributeSet.h"
#include "GAS/ERStatCapSettings.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UERAttributeSet::UERAttributeSet()
{
	// 값은 여기서 정하지 않는다. 실험체별 초기값은 데이터 테이블을 읽어
	// Instant GameplayEffect 로 넣는다 - 그래야 클램프 경로를 탄다.
}

void UERAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	const UERStatCapSettings& Caps = UERStatCapSettings::Get();

	// ── 체력·기력: 0 하한, 최대치 상한 ──────────────────────
	//
	// ⭐ 하한은 언제나 0 이다. "시셀라 R 은 100 아래로 안 떨어진다" 같은 규칙을
	//   여기 넣지 않는다. 그런 하한이 데미지 경로에 섞이면, 실수로 하나 남았을 때
	//   **죽어야 할 때 안 죽는다.** 하한이 필요한 스킬이 스스로 피해량을 깎아서 넘긴다.
	//   근거: Docs/4_Argument/3_어트리뷰트셋_구조.md D절
	if (Attribute == GetHPAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHP());
	}
	else if (Attribute == GetVPAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxVP());
	}
	else if (Attribute == GetMaxHPAttribute() || Attribute == GetMaxVPAttribute())
	{
		// 최대치가 0 이면 나눗셈·비율 계산이 전부 깨진다.
		NewValue = FMath::Max(NewValue, 1.f);
	}

	// ── 원작 확인값 ─────────────────────────────────────────
	else if (Attribute == GetAttackSpeedAttribute())
	{
		// ⚠ 돌격소총 "과열"은 이 상한을 무시한다. 그 예외는 무기 작업에서 붙인다.
		//   지금은 예외 경로가 없으므로 항상 클램프된다.
		NewValue = FMath::Clamp(NewValue, 0.f, Caps.MaxAttackSpeed);
	}

	// ── 100%에 닿으면 게임이 깨지는 것들 (전부 자체 결정값) ──
	else if (Attribute == GetCritChanceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, Caps.MaxCritChance);
	}
	else if (Attribute == GetDamageDownAttribute()
		|| Attribute == GetBasicAtkDamageDownAttribute()
		|| Attribute == GetSkillDamageDownAttribute()
		|| Attribute == GetModeDamageDownAttribute())
	{
		// ⭐ 1.0 을 넘기면 맞을 때마다 체력이 차오른다. 밸런스가 아니라 게임이 깨지는 문제다.
		NewValue = FMath::Clamp(NewValue, 0.f, Caps.MaxDamageDown);
	}
	else if (Attribute == GetDefPenPercentAttribute())
	{
		// 1.0 이면 방어력 스탯 자체가 무의미해진다.
		NewValue = FMath::Clamp(NewValue, 0.f, Caps.MaxDefPenPercent);
	}
	else if (Attribute == GetSlowResistAttribute() || Attribute == GetCCResistAttribute())
	{
		// 1.0 이면 CC 완전 면역이 된다.
		NewValue = FMath::Clamp(NewValue, 0.f, Caps.MaxResist);
	}
	else if (Attribute == GetMoveSpeedAttribute())
	{
		// ⭐ 하한이 없으면 둔화가 쌓여 이동 속도 0 -> 영구 행동 불능이 된다.
		NewValue = FMath::Clamp(NewValue, Caps.MinMoveSpeed, Caps.MaxMoveSpeed);
	}

	// SkillHaste 는 클램프하지 않는다 - 원작에서 상한 없음이 확인됐다.
	// ⚠ "쿨감 30% 상한"은 구버전 서술이다. 그걸 따라 캡을 넣으면 안 된다.
}

void UERAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UERAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UERAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 여기는 서버에서만 불린다 (GE 실행은 서버 권위).
	if (Data.EvaluatedData.Attribute != GetIncomingDamageAttribute())
	{
		return;
	}

	const float Damage = GetIncomingDamage();

	// ⭐ 통로를 반드시 비운다. 안 비우면 다음 피해에 이전 값이 남아 중복으로 들어간다.
	SetIncomingDamage(0.f);

	if (Damage <= 0.f)
	{
		return;
	}

	// 하한은 0 이다. "시셀라 R 은 100 아래로 안 떨어진다" 같은 규칙은 여기 없다 —
	// 하한이 필요한 스킬이 피해량을 미리 깎아서 넘긴다.
	// 실제 클램프는 PreAttributeChange 가 한다.
	SetHP(GetHP() - Damage);

	// 사망 정의는 "0 **이하**"다. 0 미만이 아니다 - 클램프가 이미 0 에서 잘라내므로
	// 0 미만은 애초에 나올 수 없고, 미만으로 쓰면 아무도 죽지 않는다.
	// (원작이 0 에서 죽는지 0 미만에서 죽는지는 미확인 - 자체 결정값이다)
	//
	// ⭐ bOutOfHealth 로 한 번만 쏜다. 이미 0 인 대상에 피해가 또 들어와도 다시 쏘지 않는다.
	if (GetHP() <= 0.f && !bOutOfHealth)
	{
		// 마지막으로 때린 액터. 없을 수 있다(환경 피해).
		AActor* Instigator = Data.EffectSpec.GetEffectContext().GetOriginalInstigator();
		OnOutOfHealth.Broadcast(Instigator);
	}

	// 듣는 쪽이 회복시켰을 수 있으므로 브로드캐스트 뒤에 다시 읽는다.
	bOutOfHealth = (GetHP() <= 0.f);
}

void UERAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	// ⚠ 이 함수는 **클라에서도 불린다.** 복제로 MaxHP 가 내려오면 그 경로로 들어온다:
	//   OnRep_MaxHP -> SetBaseAttributeValueFromReplication (GameplayEffect.cpp:3471)
	//     -> OnAttributeAggregatorDirty (:3228 이 IsNetSimulating 을 처리한다)
	//       -> InternalUpdateNumericalAttribute (:3671) -> PostAttributeChange
	//
	// SetHP() 는 SetNumericAttributeBase 로 직행해서 권위 검사가 없다 (AttributeSet.h:446).
	// 가드가 없으면 클라가 자기 베이스 값을 멋대로 고친다. 클라는 복제로 받기만 한다.
	const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	// 최대치가 줄면 현재값이 그 위에 남아 있을 수 있다. 따라 내린다.
	//
	// ⚠ 늘어날 때는 건드리지 않는다. 최대 체력 버프가 현재 체력까지 채우면
	//   버프가 곧 회복기가 되어 밸런스가 달라진다.
	//   (원작이 어느 쪽인지는 미확인 - 자체 결정값이다)
	//
	// ⚠ 비율 유지(NewMax/OldMax 배)도 아니다. 단순 클램프다.
	if (Attribute == GetMaxHPAttribute() && NewValue < OldValue)
	{
		if (GetHP() > NewValue)
		{
			SetHP(NewValue);
		}
	}
	else if (Attribute == GetMaxVPAttribute() && NewValue < OldValue)
	{
		if (GetVP() > NewValue)
		{
			SetVP(NewValue);
		}
	}

	// 부활·회복으로 체력이 다시 0 보다 커지면 빗장을 푼다. 안 풀면 두 번째 죽음이 안 알려진다.
	if (bOutOfHealth && GetHP() > 0.f)
	{
		bOutOfHealth = false;
	}
}

void UERAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// ⚠ 어트리뷰트를 추가하면 여기에도 반드시 등록한다.
	//   빠뜨리면 컴파일도 되고 에러도 없는데 값만 안 온다.
	//
	// REPNOTIFY_Always 인 이유: 기본 동작은 값이 바뀐 경우에만 OnRep 을 부르는데,
	//   어트리뷰트는 예측이 틀려 서버 값으로 되돌아올 때 같은 값이 다시 올 수 있다.
	//   그 롤백을 클라가 알아채야 한다.
	//
	// ⭐ IncomingDamage(Meta)는 여기 없다. 서버에서 한 번 쓰고 버리는 값이고,
	//   보내면 다른 플레이어가 받을 피해량이 노출된다.

#define ER_REP(PropertyName) \
	DOREPLIFETIME_CONDITION_NOTIFY(UERAttributeSet, PropertyName, COND_None, REPNOTIFY_Always)

	ER_REP(HP);
	ER_REP(MaxHP);
	ER_REP(HPRegen);
	ER_REP(VP);
	ER_REP(MaxVP);
	ER_REP(VPRegen);

	ER_REP(AttackPower);
	ER_REP(Defense);
	ER_REP(AttackSpeed);
	ER_REP(MoveSpeed);
	ER_REP(Sight);
	ER_REP(AttackRange);

	ER_REP(CritChance);
	ER_REP(CritDamageUp);
	ER_REP(SkillAmp);
	ER_REP(BasicAtkAmp);
	ER_REP(DefPenPercent);
	ER_REP(DefPenFlat);
	ER_REP(DamageUp);
	ER_REP(FinalDamageUpPercent);
	ER_REP(FinalDamageUpFlat);
	ER_REP(SkillHaste);

	ER_REP(DamageDown);
	ER_REP(BasicAtkDamageDown);
	ER_REP(SkillDamageDown);
	ER_REP(SlowResist);
	ER_REP(CCResist);

	ER_REP(Lifesteal);
	ER_REP(OmniLifesteal);
	ER_REP(HealAmp);
	ER_REP(OutOfCombatRegen);

	ER_REP(ModeDamageUp);
	ER_REP(ModeDamageDown);

#undef ER_REP
}

// ⭐ 일반 OnRep 처럼 값만 대입하면 안 된다.
//   GAMEPLAYATTRIBUTE_REPNOTIFY 가 GAS 내부 캐시를 갱신한다.
//   빠뜨리면 컴파일도 되고 값도 보이는데 예측·이펙트 스택 계산이 조용히 어긋난다.
#define ER_ONREP(PropertyName) \
	void UERAttributeSet::OnRep_##PropertyName(const FGameplayAttributeData& OldValue) \
	{ \
		GAMEPLAYATTRIBUTE_REPNOTIFY(UERAttributeSet, PropertyName, OldValue); \
	}

ER_ONREP(HP)
ER_ONREP(MaxHP)
ER_ONREP(HPRegen)
ER_ONREP(VP)
ER_ONREP(MaxVP)
ER_ONREP(VPRegen)

ER_ONREP(AttackPower)
ER_ONREP(Defense)
ER_ONREP(AttackSpeed)
ER_ONREP(MoveSpeed)
ER_ONREP(Sight)
ER_ONREP(AttackRange)

ER_ONREP(CritChance)
ER_ONREP(CritDamageUp)
ER_ONREP(SkillAmp)
ER_ONREP(BasicAtkAmp)
ER_ONREP(DefPenPercent)
ER_ONREP(DefPenFlat)
ER_ONREP(DamageUp)
ER_ONREP(FinalDamageUpPercent)
ER_ONREP(FinalDamageUpFlat)
ER_ONREP(SkillHaste)

ER_ONREP(DamageDown)
ER_ONREP(BasicAtkDamageDown)
ER_ONREP(SkillDamageDown)
ER_ONREP(SlowResist)
ER_ONREP(CCResist)

ER_ONREP(Lifesteal)
ER_ONREP(OmniLifesteal)
ER_ONREP(HealAmp)
ER_ONREP(OutOfCombatRegen)

ER_ONREP(ModeDamageUp)
ER_ONREP(ModeDamageDown)

#undef ER_ONREP
