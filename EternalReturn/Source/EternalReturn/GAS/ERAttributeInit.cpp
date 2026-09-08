// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERAttributeInit.h"
#include "GAS/ERAttributeTypes.h"
#include "GAS/ERGameplayTags.h"
#include "EternalReturn.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

namespace
{
	/**
	 * ⭐ 초기화 GE 의 모디파이어 목록 — **이 순서가 곧 애셋의 순서다.**
	 *
	 * 적용(ApplyStatRow)과 검사(ValidateInitEffect)가 **이 함수 하나**를 같이 쓴다.
	 * 목록이 두 벌이 되면 언젠가 어긋나므로 나누지 않는다.
	 *
	 * ⚠ 맨 앞 네 줄의 순서를 바꾸지 마라.
	 *   MaxHP / MaxVP 가 HP / VP 보다 **앞**이어야 한다. 모디파이어는 배열 순서대로
	 *   즉시 적용되므로(GameplayEffect.cpp:3004), 뒤집히면 HP 클램프가 아직 0 인
	 *   MaxHP 를 보고 **체력을 0 으로 잘라버린다.**
	 *
	 * ⚠ HP / VP 는 데이터 애셋에 필드가 없다. 시작 체력·기력은 항상 최대치다 (C-4 절).
	 *
	 * ⚠ 키는 FName 이 아니라 **GameplayTag** 다. 애셋에서 편집 가능한 건 DataTag 뿐이고
	 *   DataName 은 VisibleDefaultsOnly 라 에디터에서 못 바꾼다 (GameplayEffect.h:253-255).
	 */
	void BuildInitMagnitudes(const FERCharStats& Row, TArray<TPair<FGameplayTag, float>>& Out)
	{
		Out.Reset(33);
		auto Add = [&Out](const FGameplayTag& Key, float Value)
		{
			Out.Emplace(Key, Value);
		};

		// ── 최대치가 먼저 ───────────────────────────────────
		Add(ERTags::SetByCaller_MaxHP, Row.MaxHP);
		Add(ERTags::SetByCaller_MaxVP, Row.MaxVP);

		// ── 그 다음 현재치를 최대치로 채운다 ────────────────
		Add(ERTags::SetByCaller_HP, Row.MaxHP);
		Add(ERTags::SetByCaller_VP, Row.MaxVP);

		// ── 기초 ────────────────────────────────────────────
		Add(ERTags::SetByCaller_HPRegen, Row.HPRegen);
		Add(ERTags::SetByCaller_VPRegen, Row.VPRegen);
		Add(ERTags::SetByCaller_AttackPower, Row.AttackPower);
		Add(ERTags::SetByCaller_Defense, Row.Defense);
		Add(ERTags::SetByCaller_AttackSpeed, Row.AttackSpeed);
		Add(ERTags::SetByCaller_MoveSpeed, Row.MoveSpeed);
		Add(ERTags::SetByCaller_Sight, Row.Sight);
		Add(ERTags::SetByCaller_AttackRange, Row.AttackRange);

		// ── 공격 파생 ───────────────────────────────────────
		Add(ERTags::SetByCaller_CritChance, Row.CritChance);
		Add(ERTags::SetByCaller_CritDamageUp, Row.CritDamageUp);
		Add(ERTags::SetByCaller_SkillAmp, Row.SkillAmp);
		Add(ERTags::SetByCaller_BasicAtkAmp, Row.BasicAtkAmp);
		Add(ERTags::SetByCaller_DefPenPercent, Row.DefPenPercent);
		Add(ERTags::SetByCaller_DefPenFlat, Row.DefPenFlat);
		Add(ERTags::SetByCaller_DamageUp, Row.DamageUp);
		Add(ERTags::SetByCaller_FinalDamageUpPercent, Row.FinalDamageUpPercent);
		Add(ERTags::SetByCaller_FinalDamageUpFlat, Row.FinalDamageUpFlat);
		Add(ERTags::SetByCaller_SkillHaste, Row.SkillHaste);

		// ── 방어 파생 ───────────────────────────────────────
		Add(ERTags::SetByCaller_DamageDown, Row.DamageDown);
		Add(ERTags::SetByCaller_BasicAtkDamageDown, Row.BasicAtkDamageDown);
		Add(ERTags::SetByCaller_SkillDamageDown, Row.SkillDamageDown);
		Add(ERTags::SetByCaller_SlowResist, Row.SlowResist);
		Add(ERTags::SetByCaller_CCResist, Row.CCResist);

		// ── 유지력 ──────────────────────────────────────────
		Add(ERTags::SetByCaller_Lifesteal, Row.Lifesteal);
		Add(ERTags::SetByCaller_OmniLifesteal, Row.OmniLifesteal);
		Add(ERTags::SetByCaller_HealAmp, Row.HealAmp);
		Add(ERTags::SetByCaller_OutOfCombatRegen, Row.OutOfCombatRegen);

		// ── 모드 보정 ───────────────────────────────────────
		Add(ERTags::SetByCaller_ModeDamageUp, Row.ModeDamageUp);
		Add(ERTags::SetByCaller_ModeDamageDown, Row.ModeDamageDown);
	}
}

bool ERAttributeInit::ValidateInitEffect(const UGameplayEffect* Effect)
{
#if UE_BUILD_SHIPPING
	return true;
#else
	if (!Effect)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[초기스탯] GE 가 null 이다."));
		return false;
	}

	// 기대 목록은 값과 무관하므로 기본값으로 뽑는다 - 여기서는 키와 순서만 본다.
	const FERCharStats Dummy;
	TArray<TPair<FGameplayTag, float>> Expected;
	BuildInitMagnitudes(Dummy, Expected);

	bool bOk = true;

	if (Effect->DurationPolicy != EGameplayEffectDurationType::Instant)
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[초기스탯] %s 의 Duration Policy 가 Instant 가 아니다. 베이스 값이 안 박힌다."),
			*GetNameSafe(Effect));
		bOk = false;
	}

	if (Effect->Modifiers.Num() != Expected.Num())
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[초기스탯] %s 의 모디파이어가 %d 개다. %d 개여야 한다."),
			*GetNameSafe(Effect), Effect->Modifiers.Num(), Expected.Num());
		bOk = false;
	}

	const int32 Count = FMath::Min(Effect->Modifiers.Num(), Expected.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FGameplayModifierInfo& Mod = Effect->Modifiers[Index];
		const FGameplayTag& ExpectedTag = Expected[Index].Key;
		// 태그 "SetByCaller.MaxHP" 에서 접두사를 떼면 어트리뷰트 이름이다. 로그에만 쓴다.
		static const FString TagPrefix = TEXT("SetByCaller.");
		const FString ExpectedName = ExpectedTag.GetTagName().ToString().RightChop(TagPrefix.Len());

		// ① 어트리뷰트 이름과 순서
		const FString ActualName = Mod.Attribute.GetName();
		if (ActualName != ExpectedName)
		{
			UE_LOG(LogEternalReturn, Error,
				TEXT("[초기스탯] %s Modifiers[%d] 의 어트리뷰트가 '%s' 다. '%s' 여야 한다. (순서 포함)"),
				*GetNameSafe(Effect), Index, *ActualName, *ExpectedName);
			bOk = false;
		}

		// ② 연산 - Override 여야 한다. Add 면 기존 값에 더해져 부활 재초기화가 망가진다.
		if (Mod.ModifierOp != EGameplayModOp::Override)
		{
			UE_LOG(LogEternalReturn, Error,
				TEXT("[초기스탯] %s Modifiers[%d] (%s) 의 연산이 Override 가 아니다."),
				*GetNameSafe(Effect), Index, *ExpectedName);
			bOk = false;
		}

		// ③ SetByCaller 키
		if (Mod.ModifierMagnitude.GetMagnitudeCalculationType() != EGameplayEffectMagnitudeCalculation::SetByCaller)
		{
			UE_LOG(LogEternalReturn, Error,
				TEXT("[초기스탯] %s Modifiers[%d] (%s) 의 Magnitude 가 Set By Caller 가 아니다."),
				*GetNameSafe(Effect), Index, *ExpectedName);
			bOk = false;
			continue;
		}

		// ⚠ DataName 이 아니라 DataTag 를 본다. 애셋에서 편집 가능한 건 DataTag 뿐이고,
		//   DataTag 가 유효하면 런타임도 그쪽을 쓴다 (GameplayEffect.cpp:1122).
		const FGameplayTag& DataTag = Mod.ModifierMagnitude.GetSetByCallerFloat().DataTag;
		if (!DataTag.IsValid())
		{
			UE_LOG(LogEternalReturn, Error,
				TEXT("[초기스탯] %s Modifiers[%d] 의 Set By Caller > Data Tag 가 비어 있다. '%s' 를 넣어야 한다."),
				*GetNameSafe(Effect), Index, *ExpectedTag.ToString());
			bOk = false;
		}
		else if (DataTag != ExpectedTag)
		{
			UE_LOG(LogEternalReturn, Error,
				TEXT("[초기스탯] %s Modifiers[%d] 의 Data Tag 가 '%s' 다. '%s' 여야 한다."),
				*GetNameSafe(Effect), Index, *DataTag.ToString(), *ExpectedTag.ToString());
			bOk = false;
		}
	}

	if (bOk)
	{
		UE_LOG(LogEternalReturn, Verbose,
			TEXT("[초기스탯] %s 검사 통과 (모디파이어 %d)."), *GetNameSafe(Effect), Count);
	}
	return bOk;
#endif
}

bool ERAttributeInit::ApplyStatRow(UAbilitySystemComponent* ASC,
	TSubclassOf<UGameplayEffect> InitEffectClass,
	const FERCharStats& Row)
{
	if (!ASC)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[초기스탯] ASC 가 null 이다."));
		return false;
	}

	// 클라가 자기 베이스 값을 박으면 서버와 어긋난다. 클라는 복제로 받는다.
	if (!ASC->IsOwnerActorAuthoritative())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[초기스탯] 서버가 아닌 곳에서 불렸다. %s"),
			*GetNameSafe(ASC->GetOwnerActor()));
		return false;
	}

	if (!InitEffectClass)
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[초기스탯] 초기화 GE 클래스가 비어 있다. %s 의 Init Stats Effect 를 채워야 한다."),
			*GetNameSafe(ASC->GetOwnerActor()));
		return false;
	}

	// 애셋이 기대한 모양인지 먼저 본다. 어긋나도 적용은 계속한다 -
	// 로그를 남기는 게 목적이지 게임을 멈추는 게 목적이 아니다.
	ValidateInitEffect(InitEffectClass->GetDefaultObject<UGameplayEffect>());

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(ASC->GetOwnerActor());

	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(InitEffectClass, 1.f, Context);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[초기스탯] Spec 생성 실패. %s"),
			*GetNameSafe(ASC->GetOwnerActor()));
		return false;
	}

	TArray<TPair<FGameplayTag, float>> Magnitudes;
	BuildInitMagnitudes(Row, Magnitudes);
	for (const TPair<FGameplayTag, float>& Entry : Magnitudes)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(Entry.Key, Entry.Value);
	}

	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);

	UE_LOG(LogEternalReturn, Log,
		TEXT("[초기스탯] %s 적용 — MaxHP=%.0f MaxVP=%.0f AttackPower=%.0f Defense=%.0f MoveSpeed=%.2f"),
		*GetNameSafe(ASC->GetOwnerActor()), Row.MaxHP, Row.MaxVP, Row.AttackPower, Row.Defense, Row.MoveSpeed);

	return true;
}
