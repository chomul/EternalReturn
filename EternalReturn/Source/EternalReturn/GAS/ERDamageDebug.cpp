// Copyright Epic Games, Inc. All Rights Reserved.
//
// ⚠⚠ 임시 검증 코드다. F07(스킬 시스템)이 데미지 GE 의 실제 호출자가 되면 이 파일을 통째로 지운다. ⚠⚠
//
// 왜 만들었나:
//   데미지 공식은 이 프로젝트에서 가장 먼저 맞아야 하는 계산이다.
//   (스탯 문서 §7 - "2순위가 이 문서의 전부다.")
//   그런데 F07 이 오기 전에는 피해 GE 를 발동시킬 수단이 없어, 공식이 틀린 채로
//   그 위에 스킬을 쌓게 된다. 그러면 나중에 원인 범위가 어빌리티까지 넓어진다.
//
// 무엇을 확인하나:
//   Execution 뿐 아니라 IncomingDamage -> 체력 차감(F02-04) 까지 **경로 전체**를 탄다.
//
// 지우는 법: 이 파일만 삭제하면 된다. 다른 파일에 참조가 없다.

#include "GAS/ERDamageExecution.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERStatCapSettings.h"
#include "EternalReturn.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#if !UE_BUILD_SHIPPING

namespace ERDamageDebug
{
	/**
	 * 체력 비례 계수. ER.Damage.Test 의 인자로 받으면 인자가 또 늘어나서 따로 둔다.
	 * ER.Damage.SetProp 으로 세팅한다.
	 */
	float MaxHPRatio  = 0.f;
	float CurHPRatio  = 0.f;
	float LostHPRatio = 0.f;

	/** 커맨드를 친 사람의 폰에서 ASC 를 찾는다. */
	UAbilitySystemComponent* FindLocalASC(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (const IAbilitySystemInterface* Interface = Cast<IAbilitySystemInterface>(Pawn))
		{
			return Interface->GetAbilitySystemComponent();
		}
		return nullptr;
	}

	/**
	 * 데미지 GE 를 코드로 만든다.
	 *
	 * 애셋을 만들지 않는 이유: 이건 임시 코드라 Content/ 에 흔적을 남기지 않는다.
	 * 실제 스킬용 GE 애셋은 F07 에서 따로 만든다.
	 */
	UGameplayEffect* MakeDamageEffect()
	{
		UGameplayEffect* GE = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("GE_ERDebugDamage"));
		GE->DurationPolicy = EGameplayEffectDurationType::Instant;

		FGameplayEffectExecutionDefinition ExecDef;
		ExecDef.CalculationClass = UERDamageExecution::StaticClass();
		GE->Executions.Add(ExecDef);

		return GE;
	}

	void Test(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogEternalReturn, Warning,
				TEXT("[피해테스트] 사용법: ER.Damage.Test <원시피해> <방어력> [basic|skill|true|none] [퍼센트관통] [고정관통]"));
			return;
		}

		UAbilitySystemComponent* ASC = FindLocalASC(World);
		if (!ASC)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[피해테스트] ASC 를 찾지 못했다. 폰이 스폰됐는지 확인한다."));
			return;
		}
		if (!ASC->IsOwnerActorAuthoritative())
		{
			UE_LOG(LogEternalReturn, Error,
				TEXT("[피해테스트] 서버가 아니다. Execution 은 서버에서만 돈다 - 리슨 서버나 단독 실행으로 시험한다."));
			return;
		}

		const float Raw     = FCString::Atof(*Args[0]);
		const float Defense = FCString::Atof(*Args[1]);
		// 채널: basic / skill / true / none. 하위 호환으로 "false" 도 스킬로 받는다.
		// 흡혈 치유 감소를 보려면 "+aoe" / "+wild" 를 이어 붙인다. 예) basic+aoe+wild
		FString Channel = Args.Num() >= 3 ? Args[2].ToLower() : TEXT("skill");
		if (Channel == TEXT("false")) { Channel = TEXT("skill"); }
		const bool bAoE  = Channel.Contains(TEXT("aoe"));
		const bool bWild = Channel.Contains(TEXT("wild"));
		const bool bBoss = Channel.Contains(TEXT("boss"));
		const bool bTrue  = Channel.StartsWith(TEXT("true"));
		const bool bBasic = Channel.StartsWith(TEXT("basic"));
		const bool bNone  = Channel.StartsWith(TEXT("none"));   // 태그 누락 경고 확인용

		// 관통은 선택 인자다. 안 주면 0 이라 기존 호출이 그대로 동작한다.
		const float PenPercent = Args.Num() >= 4 ? FCString::Atof(*Args[3]) : 0.f;
		const float PenFlat    = Args.Num() >= 5 ? FCString::Atof(*Args[4]) : 0.f;

		// 시험값을 강제한다. 디버그 전용이라 GE 를 거치지 않고 직접 넣는다.
		// ⚠ 관통은 **공격자** 스탯인데 여기서는 자기 자신을 때리므로 같은 ASC 에 넣는다.
		ASC->SetNumericAttributeBase(UERAttributeSet::GetDefenseAttribute(), Defense);
		ASC->SetNumericAttributeBase(UERAttributeSet::GetDefPenPercentAttribute(), PenPercent);
		ASC->SetNumericAttributeBase(UERAttributeSet::GetDefPenFlatAttribute(), PenFlat);

		// 체력이 모자라면 결과가 클램프돼 검증이 무의미해진다. 넉넉히 채운다.
		const float NeededHP = FMath::Max(Raw, 1000.f) * 2.f;
		ASC->SetNumericAttributeBase(UERAttributeSet::GetMaxHPAttribute(), NeededHP);
		ASC->SetNumericAttributeBase(UERAttributeSet::GetHPAttribute(), NeededHP);

		const float HPBefore = ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute());
		const float ReadDef  = ASC->GetNumericAttribute(UERAttributeSet::GetDefenseAttribute());
		// ⚠ 클램프를 거친 뒤의 값을 읽는다. DefPenPercent 는 상한 0.8 로 잘린다.
		const float ReadPenPct  = ASC->GetNumericAttribute(UERAttributeSet::GetDefPenPercentAttribute());
		const float ReadPenFlat = ASC->GetNumericAttribute(UERAttributeSet::GetDefPenFlatAttribute());

		// ── 체력 비례 성분 (기대값 쪽) ──────────────────────
		//
		// ⭐ Execution 과 **독립적으로** 다시 계산한다. 같은 코드를 부르면 검증이 아니다.
		const float SrcMaxHP = ASC->GetNumericAttribute(UERAttributeSet::GetMaxHPAttribute());
		const float SrcHP    = ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute());
		const float LostHP   = FMath::Max(SrcMaxHP - SrcHP, 0.f);

		float ExpProp = SrcMaxHP * MaxHPRatio + SrcHP * CurHPRatio + LostHP * LostHPRatio;
		if (ExpProp > 0.f && bBoss)
		{
			ExpProp *= UERStatCapSettings::Get().BossProportionalDamageScale;
		}
		const float RawWithProp = Raw + ExpProp;

		// 기대값 - 코드와 독립적으로 여기서 다시 계산해 대조한다.
		// ⭐ 순서: 퍼센트 관통 먼저, 고정 관통 나중 (계산식 §2.2 4단계).
		float ExpectedEffDef = ReadDef * (1.f - ReadPenPct);
		ExpectedEffDef -= ReadPenFlat;
		ExpectedEffDef = FMath::Max(ExpectedEffDef, 0.f);

		// 증감 계열도 읽어 기대값에 반영한다 (ER.Damage.SetStat 으로 미리 세팅해 둔 값).
		auto Get = [ASC](const FGameplayAttribute& A) { return ASC->GetNumericAttribute(A); };
		const float CritChance    = Get(UERAttributeSet::GetCritChanceAttribute());
		const float CritDamageUp  = Get(UERAttributeSet::GetCritDamageUpAttribute());
		const float DamageUp      = Get(UERAttributeSet::GetDamageUpAttribute());
		const float DamageDown    = Get(UERAttributeSet::GetDamageDownAttribute());
		const float BasicAtkAmp   = Get(UERAttributeSet::GetBasicAtkAmpAttribute());
		const float BasicAtkDown  = Get(UERAttributeSet::GetBasicAtkDamageDownAttribute());
		const float SkillDown     = Get(UERAttributeSet::GetSkillDamageDownAttribute());
		const float ModeUp        = Get(UERAttributeSet::GetModeDamageUpAttribute());
		const float ModeDown      = Get(UERAttributeSet::GetModeDamageDownAttribute());
		const float ModeMul       = 1.f + ModeUp - ModeDown;

		// ⭐ 기대값을 Execution 과 **독립적으로** 다시 계산한다. 같은 함수를 부르면 검증이 아니다.
		float ExpectedNoCrit;
		float ExpectedCrit;
		if (bTrue)
		{
			// ⚠ 고정 피해는 계수도 비례 성분도 안 탄다 (2 단계가 3·3-b 보다 앞).
			ExpectedNoCrit = Raw * ModeMul;
			ExpectedCrit   = ExpectedNoCrit;   // 고정 피해에는 치명타가 없다
		}
		else
		{
			const float AfterDef = RawWithProp * (100.f / (100.f + ExpectedEffDef));
			const float ChannelMul = bBasic ? (1.f + BasicAtkAmp - BasicAtkDown)
			                                : (1.f - SkillDown);
			const float Common = (1.f + DamageUp - DamageDown) * ChannelMul * ModeMul;

			ExpectedNoCrit = AfterDef * Common;
			ExpectedCrit   = bBasic
				? AfterDef * (1.f + (0.75f + CritDamageUp)) * Common
				: ExpectedNoCrit;   // 스킬·고정 피해는 치명타가 붙지 않는다
		}

		// 확률이 0 이나 1 일 때만 결과를 단정할 수 있다. 그 사이면 판정하지 않는다.
		const bool  bCritCertain   = !bBasic || CritChance <= 0.f || CritChance >= 1.f;
		const float Expected = (bBasic && CritChance >= 1.f) ? ExpectedCrit : ExpectedNoCrit;

		UGameplayEffect* GE = MakeDamageEffect();
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(ASC->GetOwnerActor());

		FGameplayEffectSpec Spec(GE, Context, 1.f);
		Spec.SetSetByCallerMagnitude(ERTags::Data_Damage_Base, Raw);
		// 계수는 0 이다. 이 커맨드는 방어력 공식만 본다.
		Spec.SetSetByCallerMagnitude(ERTags::Data_Damage_APRatio, 0.f);
		Spec.SetSetByCallerMagnitude(ERTags::Data_Damage_BonusAPRatio, 0.f);
		Spec.SetSetByCallerMagnitude(ERTags::Data_Damage_SkillAmpRatio, 0.f);

		// 채널 태그. 애셋 태그와 같은 통로로 들어간다 (GameplayEffect.h:1119-1120).
		// none 이면 일부러 태그를 안 붙인다. 경고 로그가 나오는지 보는 용도다.
		if (!bNone)
		{
			Spec.AddDynamicAssetTag(bTrue  ? ERTags::Damage_Type_True
			                      : bBasic ? ERTags::Damage_Type_BasicAttack
			                               : ERTags::Damage_Type_Skill);
		}
		if (bAoE)
		{
			Spec.AddDynamicAssetTag(ERTags::Damage_Shape_AoE);
		}

		// ⚠ 야생동물 판별은 **대상의 태그**다. 자기 자신을 때리므로 자기 ASC 에 붙였다 뺀다.
		if (bWild)
		{
			ASC->AddLooseGameplayTag(ERTags::Actor_Type_Wildlife);
		}
		if (bBoss)
		{
			ASC->AddLooseGameplayTag(ERTags::Actor_Type_Boss);
		}

		// 체력 비례 계수. ER.Damage.SetProp 으로 미리 세팅해 둔 값을 쓴다.
		Spec.SetSetByCallerMagnitude(ERTags::Data_Damage_MaxHPRatio,  ERDamageDebug::MaxHPRatio);
		Spec.SetSetByCallerMagnitude(ERTags::Data_Damage_CurHPRatio,  ERDamageDebug::CurHPRatio);
		Spec.SetSetByCallerMagnitude(ERTags::Data_Damage_LostHPRatio, ERDamageDebug::LostHPRatio);

		ASC->ApplyGameplayEffectSpecToSelf(Spec);

		const float HPAfter = ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute());
		const float Actual  = HPBefore - HPAfter;
		const bool  bMatch  = FMath::IsNearlyEqual(Actual, Expected, 0.01f);

		const TCHAR* ChannelName = bTrue ? TEXT("고정피해") : bBasic ? TEXT("기본공격")
		                         : bNone ? TEXT("태그없음") : TEXT("스킬");

		if (bCritCertain)
		{
			UE_LOG(LogEternalReturn, Log,
				TEXT("[피해테스트] 원시=%.1f 방어력=%.1f 관통=%.0f%%/%.0f -> 적용방어력=%.2f 채널=%s 치명=%.0f%% | 기대=%.2f 실제=%.2f | %s"),
				Raw, ReadDef, ReadPenPct * 100.f, ReadPenFlat, ExpectedEffDef,
				ChannelName, CritChance * 100.f,
				Expected, Actual, bMatch ? TEXT("일치") : TEXT("*** 불일치 ***"));
		}
		else
		{
			// ⚠ 확률이 0 과 1 사이면 어느 쪽이 나올지 알 수 없다. 없는 확신을 만들지 않는다.
			const bool bEither = FMath::IsNearlyEqual(Actual, ExpectedNoCrit, 0.01f)
			                  || FMath::IsNearlyEqual(Actual, ExpectedCrit, 0.01f);
			UE_LOG(LogEternalReturn, Log,
				TEXT("[피해테스트] 원시=%.1f 방어력=%.1f 채널=%s 치명=%.0f%% | 비치명=%.2f 치명=%.2f 실제=%.2f | %s"),
				Raw, ReadDef, ChannelName, CritChance * 100.f,
				ExpectedNoCrit, ExpectedCrit, Actual,
				bEither ? TEXT("둘 중 하나와 일치") : TEXT("*** 둘 다 아님 ***"));
		}

		UE_LOG(LogEternalReturn, Log,
			TEXT("[피해테스트]   체력 %.1f -> %.1f"), HPBefore, HPAfter);

		// ⭐ 관통은 **지역 변수로만** 계산해야 한다. Target 의 Defense 를 GE 수정자로 깎으면
		//   다른 공격자의 피해까지 깎인 방어력으로 계산된다. 그게 안 일어났는지 매번 검사한다.
		// 흡혈 확인 - 자기 자신을 때리므로 회복이 같은 HP 에 섞인다.
		// 그래서 "실제 피해" 는 이미 흡혈이 반영된 순감소량이다. 기대값을 따로 계산해 대조한다.
		const float LifestealRate = (bBasic ? ASC->GetNumericAttribute(UERAttributeSet::GetLifestealAttribute()) : 0.f)
		                          + ASC->GetNumericAttribute(UERAttributeSet::GetOmniLifestealAttribute());
		if (LifestealRate > 0.f)
		{
			float HealCut = 0.f;
			if (bAoE)  { HealCut = FMath::Max(HealCut, 0.5f); }
			if (bWild) { HealCut = FMath::Max(HealCut, 0.6f); }
			const float HealAmp  = ASC->GetNumericAttribute(UERAttributeSet::GetHealAmpAttribute());
			const float ExpHeal  = Expected * LifestealRate * (1.f - HealCut) * (1.f + HealAmp);

			UE_LOG(LogEternalReturn, Log,
				TEXT("[흡혈] 흡혈률=%.0f%% 치유감소=%.0f%% | 기대회복=%.2f (피해 %.2f 기준) | 순감소=%.2f"),
				LifestealRate * 100.f, HealCut * 100.f, ExpHeal, Expected, Actual);
			UE_LOG(LogEternalReturn, Log,
				TEXT("[흡혈]   기대 순감소 = %.2f - %.2f = %.2f"), Expected, ExpHeal, Expected - ExpHeal);
		}

		if (bWild)
		{
			ASC->RemoveLooseGameplayTag(ERTags::Actor_Type_Wildlife);
		}
		if (bBoss)
		{
			ASC->RemoveLooseGameplayTag(ERTags::Actor_Type_Boss);
		}

		const float DefAfter = ASC->GetNumericAttribute(UERAttributeSet::GetDefenseAttribute());
		if (!FMath::IsNearlyEqual(DefAfter, ReadDef, 0.01f))
		{
			UE_LOG(LogEternalReturn, Error,
				TEXT("[피해테스트] ⚠ 대상 방어력이 %.2f -> %.2f 로 **변했다**. ")
				TEXT("관통을 Target 어트리뷰트에 적용하고 있다 - 지역 변수로 계산해야 한다."),
				ReadDef, DefAfter);
		}
		else
		{
			UE_LOG(LogEternalReturn, Log,
				TEXT("[피해테스트]   대상 방어력 %.2f 그대로 (관통이 어트리뷰트를 안 건드렸다)"), DefAfter);
		}

		if (bCritCertain && !bMatch)
		{
			UE_LOG(LogEternalReturn, Error,
				TEXT("[피해테스트] 불일치. 확인 순서: ")
				TEXT("① SetByCaller 태그 이름 ② RelevantAttributesToCapture 등록 ")
				TEXT("③ F02-04 의 IncomingDamage 처리 ④ 클램프에 걸렸는지(체력/최대체력) ")
				TEXT("⑤ 관통 순서(퍼센트가 먼저여야 한다)"));
		}
	}

	/**
	 * 어트리뷰트를 이름으로 세팅한다.
	 *
	 * 왜 범용으로 만들었나: 치명타·증감까지 ER.Damage.Test 의 인자로 받으면 인자가 9개가 된다.
	 * 스탯은 미리 세팅해 두고, Test 는 원시피해·방어력·채널·관통만 받는다.
	 *
	 * ⚠ Base 값을 직접 세팅한다. 클램프(F02-03)는 그대로 탄다 -
	 *   CritChance 2 를 넣어도 1 로 잘린다.
	 */
	void SetStat(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogEternalReturn, Warning,
				TEXT("[스탯] 사용법: ER.Damage.SetStat <어트리뷰트이름> <값>   예) ER.Damage.SetStat CritChance 1"));
			return;
		}

		UAbilitySystemComponent* ASC = FindLocalASC(World);
		if (!ASC || !ASC->IsOwnerActorAuthoritative())
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[스탯] 서버의 ASC 를 찾지 못했다."));
			return;
		}

		const FString Name = Args[0];
		const float Value = FCString::Atof(*Args[1]);

		// 리플렉션으로 찾는다. 어트리뷰트가 33개라 손으로 나열하면 추가할 때마다 빠뜨린다.
		FProperty* Prop = FindFProperty<FProperty>(UERAttributeSet::StaticClass(), FName(*Name));
		if (!Prop)
		{
			UE_LOG(LogEternalReturn, Error,
				TEXT("[스탯] '%s' 라는 어트리뷰트가 없다. 대소문자까지 정확해야 한다 (예: CritChance, DamageUp)."),
				*Name);
			return;
		}

		const FGameplayAttribute Attribute(Prop);
		ASC->SetNumericAttributeBase(Attribute, Value);

		const float Read = ASC->GetNumericAttribute(Attribute);
		if (!FMath::IsNearlyEqual(Read, Value, 0.001f))
		{
			UE_LOG(LogEternalReturn, Log,
				TEXT("[스탯] %s = %.3f 로 요청했으나 **클램프되어 %.3f** 다 (F02-03 상한)."),
				*Name, Value, Read);
		}
		else
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[스탯] %s = %.3f"), *Name, Read);
		}
	}

	// ── 어트리뷰트 변화 구독 (F02-06 검증) ──────────────────
	//
	// ⭐ 이 파일에 두는 이유: F02-06 이 요구하는 "테스트용 임시 구독" 인데,
	//   이 파일은 F07 에서 통째로 지운다. 지울 코드를 새로 만들지 않는다.
	//
	// ⚠ 실제 UI 는 이렇게 하지 않는다 - 구독은 **듣는 쪽(위젯)** 이 자기 수명에 맞춰 건다.
	//   여기서는 전역 핸들로 붙였다 뗀다.

	/**
	 * 구독 중인 것. Unwatch 로 해제한다.
	 *
	 * ⚠ **핸들을 반드시 보관한다.** AddStatic 으로 건 델리게이트는 RemoveAll(객체) 로
	 *   지울 수 없다 - 바인딩된 객체가 없기 때문이다. Remove(핸들) 만 통한다.
	 */
	struct FWatchEntry
	{
		TWeakObjectPtr<UAbilitySystemComponent> ASC;
		FGameplayAttribute Attribute;
		FDelegateHandle Handle;
	};
	TArray<FWatchEntry> WatchedAttributes;

	/** 사망 델리게이트 핸들. */
	FDelegateHandle OutOfHealthHandle;
	TWeakObjectPtr<UERAttributeSet> WatchedSet;

	void OnWatchedAttributeChanged(const FOnAttributeChangeData& Data)
	{
		UE_LOG(LogEternalReturn, Log,
			TEXT("[구독] %s : %.2f -> %.2f"),
			*Data.Attribute.GetName(), Data.OldValue, Data.NewValue);
	}

	void OnWatchedOutOfHealth(AActor* Instigator)
	{
		// ⭐ E04 검증 지점. 체력 0 인 대상을 또 때려도 이 줄이 **한 번만** 나와야 한다.
		UE_LOG(LogEternalReturn, Warning,
			TEXT("[구독] ⭐ 사망 알림 - 가해자=%s"), *GetNameSafe(Instigator));
	}

	void Watch(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogEternalReturn, Warning,
				TEXT("[구독] 사용법: ER.Attr.Watch <어트리뷰트이름>   예) ER.Attr.Watch HP"));
			return;
		}

		UAbilitySystemComponent* ASC = FindLocalASC(World);
		if (!ASC)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[구독] ASC 를 찾지 못했다."));
			return;
		}

		FProperty* Prop = FindFProperty<FProperty>(UERAttributeSet::StaticClass(), FName(*Args[0]));
		if (!Prop)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[구독] '%s' 어트리뷰트가 없다."), *Args[0]);
			return;
		}

		const FGameplayAttribute Attribute(Prop);

		// ⭐ 자체 델리게이트가 아니라 **ASC 가 주는 것**을 쓴다.
		const FDelegateHandle Handle =
			ASC->GetGameplayAttributeValueChangeDelegate(Attribute)
				.AddStatic(&ERDamageDebug::OnWatchedAttributeChanged);

		WatchedAttributes.Add(FWatchEntry{ ASC, Attribute, Handle });

		// 사망 알림도 한 번만 붙인다.
		if (!OutOfHealthHandle.IsValid())
		{
			const UERAttributeSet* Set = Cast<UERAttributeSet>(
				ASC->GetAttributeSet(UERAttributeSet::StaticClass()));
			if (Set)
			{
				OutOfHealthHandle = Set->OnOutOfHealth.AddStatic(&ERDamageDebug::OnWatchedOutOfHealth);
				WatchedSet = const_cast<UERAttributeSet*>(Set);
			}
		}

		UE_LOG(LogEternalReturn, Log,
			TEXT("[구독] %s 구독 시작 (%s). 값이 변할 때만 로그가 난다."),
			*Args[0], ASC->IsOwnerActorAuthoritative() ? TEXT("서버") : TEXT("클라"));
	}

	void Unwatch(const TArray<FString>& Args, UWorld* World)
	{
		int32 Count = 0;
		for (const FWatchEntry& Entry : WatchedAttributes)
		{
			if (UAbilitySystemComponent* ASC = Entry.ASC.Get())
			{
				// ⚠ 해제를 빠뜨리면 dangling 델리게이트가 남는다. 이 경로도 검증 대상이다.
				//   ⭐ RemoveAll(객체) 가 아니라 **Remove(핸들)** 이다 - AddStatic 은 객체가 없다.
				ASC->GetGameplayAttributeValueChangeDelegate(Entry.Attribute).Remove(Entry.Handle);
				++Count;
			}
		}
		WatchedAttributes.Reset();

		if (OutOfHealthHandle.IsValid())
		{
			if (UERAttributeSet* Set = WatchedSet.Get())
			{
				Set->OnOutOfHealth.Remove(OutOfHealthHandle);
			}
			OutOfHealthHandle.Reset();
			WatchedSet.Reset();
		}

		UE_LOG(LogEternalReturn, Log, TEXT("[구독] %d 건 해제. 이제 변화 로그가 안 나와야 한다."), Count);
	}

	/** 체력 비례 계수를 세팅한다. ER.Damage.SetProp <최대> <현재> <잃은체력> */
	void SetProp(const TArray<FString>& Args, UWorld* World)
	{
		MaxHPRatio  = Args.Num() >= 1 ? FCString::Atof(*Args[0]) : 0.f;
		CurHPRatio  = Args.Num() >= 2 ? FCString::Atof(*Args[1]) : 0.f;
		LostHPRatio = Args.Num() >= 3 ? FCString::Atof(*Args[2]) : 0.f;

		UE_LOG(LogEternalReturn, Log,
			TEXT("[비례계수] 대상최대=%.3f 대상현재=%.3f 자신잃은=%.3f"),
			MaxHPRatio, CurHPRatio, LostHPRatio);
	}

	/**
	 * 치명타 확률이 실제로 그 비율로 나오는지 표본으로 본다.
	 *
	 * ⚠ 개별 실행은 난수라 단정할 수 없다. 확률 검증은 표본으로만 가능하다.
	 *   치명타는 피해가 (1 + 0.75 + CritDamageUp) 배로 튀므로 그걸로 세면 된다.
	 */
	void CritSample(const TArray<FString>& Args, UWorld* World)
	{
		UAbilitySystemComponent* ASC = FindLocalASC(World);
		if (!ASC || !ASC->IsOwnerActorAuthoritative())
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[치명타표본] 서버의 ASC 를 찾지 못했다."));
			return;
		}

		const int32 Trials = Args.Num() >= 1 ? FMath::Clamp(FCString::Atoi(*Args[0]), 1, 100000) : 1000;

		// 방어력·관통·증감을 0 으로 두고 순수하게 치명타만 본다.
		ASC->SetNumericAttributeBase(UERAttributeSet::GetDefenseAttribute(), 0.f);
		ASC->SetNumericAttributeBase(UERAttributeSet::GetDefPenPercentAttribute(), 0.f);
		ASC->SetNumericAttributeBase(UERAttributeSet::GetDefPenFlatAttribute(), 0.f);
		ASC->SetNumericAttributeBase(UERAttributeSet::GetMaxHPAttribute(), 1.e9f);
		ASC->SetNumericAttributeBase(UERAttributeSet::GetHPAttribute(), 1.e9f);

		const float Chance   = ASC->GetNumericAttribute(UERAttributeSet::GetCritChanceAttribute());
		const float CritUp   = ASC->GetNumericAttribute(UERAttributeSet::GetCritDamageUpAttribute());
		constexpr float Raw  = 100.f;
		const float CritDmg  = Raw * (1.f + (0.75f + CritUp));

		int32 CritCount = 0;
		for (int32 i = 0; i < Trials; ++i)
		{
			const float Before = ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute());

			UGameplayEffect* GE = MakeDamageEffect();
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			FGameplayEffectSpec Spec(GE, Context, 1.f);
			Spec.SetSetByCallerMagnitude(ERTags::Data_Damage_Base, Raw);
			Spec.AddDynamicAssetTag(ERTags::Damage_Type_BasicAttack);
			ASC->ApplyGameplayEffectSpecToSelf(Spec);

			const float Dealt = Before - ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute());
			if (FMath::IsNearlyEqual(Dealt, CritDmg, 0.01f))
			{
				++CritCount;
			}
		}

		const float Observed = static_cast<float>(CritCount) / static_cast<float>(Trials);
		UE_LOG(LogEternalReturn, Log,
			TEXT("[치명타표본] 설정=%.1f%% 관측=%.1f%% (%d/%d 회) | 치명타 피해=%.1f 기본=%.1f"),
			Chance * 100.f, Observed * 100.f, CritCount, Trials, CritDmg, Raw);

		if (Chance <= 0.f && CritCount > 0)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[치명타표본] ⚠ 확률 0%% 인데 치명타가 %d 회 났다."), CritCount);
		}
		else if (Chance >= 1.f && CritCount < Trials)
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[치명타표본] ⚠ 확률 100%% 인데 %d/%d 만 치명타다."), CritCount, Trials);
		}
	}

	/**
	 * ⭐ 추가 공격력(BonusAP) 경로 확인.
	 *
	 * Docs/4_Argument/5_추가공격력_산출방식.md 의 전제를 실제로 시험한다:
	 *   Instant GE 는 Base 를, Duration/Infinite GE 는 Current 만 바꾼다.
	 *   따라서 EvaluateBonus() = 장비·버프분 이다.
	 */
	void TestBonus(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 3)
		{
			UE_LOG(LogEternalReturn, Warning,
				TEXT("[추가공격력] 사용법: ER.Damage.BonusTest <기본공격력> <추가공격력> <BonusAPRatio>"));
			return;
		}

		UAbilitySystemComponent* ASC = FindLocalASC(World);
		if (!ASC || !ASC->IsOwnerActorAuthoritative())
		{
			UE_LOG(LogEternalReturn, Error, TEXT("[추가공격력] 서버의 ASC 를 찾지 못했다."));
			return;
		}

		const float BaseAP  = FCString::Atof(*Args[0]);
		const float BonusAP = FCString::Atof(*Args[1]);
		const float Ratio   = FCString::Atof(*Args[2]);

		// 기본 공격력 = Base 값. 초기화 GE(Instant)가 하는 일과 같다.
		ASC->SetNumericAttributeBase(UERAttributeSet::GetAttackPowerAttribute(), BaseAP);

		// 추가 공격력 = Infinite GE 로 준다. 장비가 하는 일과 같다.
		UGameplayEffect* Buff = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("GE_ERDebugAPBuff"));
		Buff->DurationPolicy = EGameplayEffectDurationType::Infinite;
		{
			FGameplayModifierInfo Mod;
			Mod.Attribute = UERAttributeSet::GetAttackPowerAttribute();
			Mod.ModifierOp = EGameplayModOp::Additive;
			Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(BonusAP));
			Buff->Modifiers.Add(Mod);
		}
		FGameplayEffectContextHandle BuffContext = ASC->MakeEffectContext();
		ASC->ApplyGameplayEffectToSelf(Buff, 1.f, BuffContext);

		// 방어력 0 으로 두면 감산이 없어 피해 = 원시값이다.
		// ⚠ 관통도 0 으로 되돌린다. ER.Damage.Test 가 남긴 값이 섞이면 결과가 흐려진다.
		ASC->SetNumericAttributeBase(UERAttributeSet::GetDefenseAttribute(), 0.f);
		ASC->SetNumericAttributeBase(UERAttributeSet::GetDefPenPercentAttribute(), 0.f);
		ASC->SetNumericAttributeBase(UERAttributeSet::GetDefPenFlatAttribute(), 0.f);
		ASC->SetNumericAttributeBase(UERAttributeSet::GetMaxHPAttribute(), 100000.f);
		ASC->SetNumericAttributeBase(UERAttributeSet::GetHPAttribute(), 100000.f);

		const float TotalAP  = ASC->GetNumericAttribute(UERAttributeSet::GetAttackPowerAttribute());
		const float HPBefore = ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute());
		const float Expected = BonusAP * Ratio;

		UGameplayEffect* GE = MakeDamageEffect();
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		FGameplayEffectSpec Spec(GE, Context, 1.f);
		Spec.SetSetByCallerMagnitude(ERTags::Data_Damage_Base, 0.f);
		Spec.SetSetByCallerMagnitude(ERTags::Data_Damage_APRatio, 0.f);
		Spec.SetSetByCallerMagnitude(ERTags::Data_Damage_BonusAPRatio, Ratio);
		Spec.SetSetByCallerMagnitude(ERTags::Data_Damage_SkillAmpRatio, 0.f);
		Spec.AddDynamicAssetTag(ERTags::Damage_Type_Skill);
		ASC->ApplyGameplayEffectSpecToSelf(Spec);

		const float Actual = HPBefore - ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute());
		const bool  bMatch = FMath::IsNearlyEqual(Actual, Expected, 0.01f);

		UE_LOG(LogEternalReturn, Log,
			TEXT("[추가공격력] 기본=%.1f 추가=%.1f 총합=%.1f | 계수=%.2f | 기대=%.2f 실제=%.2f | %s"),
			BaseAP, BonusAP, TotalAP, Ratio, Expected, Actual,
			bMatch ? TEXT("일치") : TEXT("*** 불일치 ***"));

		if (!bMatch)
		{
			UE_LOG(LogEternalReturn, Error,
				TEXT("[추가공격력] 불일치. 총합이 기본+추가가 아니면 버프가 안 걸린 것이고, ")
				TEXT("총합은 맞는데 피해가 0 이면 EvaluateBonus 가 0 을 돌려준 것이다 ")
				TEXT("- 그 경우 Infinite 가 아니라 Instant 로 걸렸는지 본다."));
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GERAttrWatchCmd(
	TEXT("ER.Attr.Watch"),
	TEXT("[임시] 어트리뷰트 변화를 구독한다. ER.Attr.Watch <어트리뷰트이름>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ERDamageDebug::Watch));

static FAutoConsoleCommandWithWorldAndArgs GERAttrUnwatchCmd(
	TEXT("ER.Attr.Unwatch"),
	TEXT("[임시] 구독을 전부 해제한다."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ERDamageDebug::Unwatch));

static FAutoConsoleCommandWithWorldAndArgs GERDamageSetPropCmd(
	TEXT("ER.Damage.SetProp"),
	TEXT("[임시] 체력 비례 계수 세팅. ER.Damage.SetProp <대상최대> <대상현재> <자신잃은>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ERDamageDebug::SetProp));

static FAutoConsoleCommandWithWorldAndArgs GERDamageCritSampleCmd(
	TEXT("ER.Damage.CritSample"),
	TEXT("[임시] 치명타 확률을 표본으로 확인. ER.Damage.CritSample [횟수=1000]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ERDamageDebug::CritSample));

static FAutoConsoleCommandWithWorldAndArgs GERDamageSetStatCmd(
	TEXT("ER.Damage.SetStat"),
	TEXT("[임시] 어트리뷰트를 이름으로 세팅. ER.Damage.SetStat <이름> <값>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ERDamageDebug::SetStat));

static FAutoConsoleCommandWithWorldAndArgs GERDamageTestCmd(
	TEXT("ER.Damage.Test"),
	TEXT("[임시] 데미지 공식 검증. ER.Damage.Test <원시피해> <방어력> [basic|skill|true|none(+aoe)(+wild)(+boss)] [퍼센트관통] [고정관통]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ERDamageDebug::Test));

static FAutoConsoleCommandWithWorldAndArgs GERDamageBonusCmd(
	TEXT("ER.Damage.BonusTest"),
	TEXT("[임시] 추가 공격력 경로 검증. ER.Damage.BonusTest <기본공격력> <추가공격력> <계수>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ERDamageDebug::TestBonus));

#endif // !UE_BUILD_SHIPPING
