// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERDamageExecution.h"
#include "GAS/ERDamageStatics.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERLifestealEffect.h"
#include "GAS/ERStatCapSettings.h"
#include "EternalReturn.h"   // LogEternalReturn

// ─────────────────────────────────────────────────────────────
// 계산식 단계별 구현 상황
//   근거: Docs/0_GameDesign/Systems/스탯_데미지공식_역기획서.md §2.2
//
//   1   모드 보정                        ✅
//   2   고정 피해 채널 즉시 종료          ✅
//   3   계수 적용                        ✅
//   3-b 체력 비례 성분 + 보스 감쇠        ✅
//   4   적용 방어력 (관통)                ✅
//   5   방어력 감산 100/(100+DEF)         ✅
//   6   치명타                           ✅
//   7   공통 피해 증감                    ✅
//   8   채널별 증감                       ✅
//   9   최종 피해 추가                    ⬜ 보류 — 해석 미확인 (§8)
//   10  모드 보정                        ✅
//   11  무효화 계층                       ✅ F06-06 (State.DamageImmune) · 무적은 ⏸ 자리만
//   --  흡혈 (후처리)                     ✅ F03-05
// ─────────────────────────────────────────────────────────────

UERDamageExecution::UERDamageExecution()
{
	// ⭐ 22개를 통째로 붙인다. 하나씩 나열하면 언젠가 빠뜨리고,
	//   빠뜨린 어트리뷰트는 **경고 없이 0 으로 읽힌다** (GameplayEffect.cpp:4238).
	RelevantAttributesToCapture = ERDamageStatics().AllDefs;
}

void UERDamageExecution::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
#if WITH_SERVER_CODE
	const FERDamageStatics& S = ERDamageStatics();
	const FGameplayEffectSpec& Spec = ExecParams.GetOwningSpec();

	FAggregatorEvaluateParameters EvalParams;
	EvalParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvalParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	// ── 어빌리티가 넘긴 계수 ────────────────────────────────
	//
	// ⚠ 두 번째 인자가 false 다. 안 넘긴 계수는 0 이 정상이므로 경고를 내면 안 된다.
	//   (기본 공격은 SkillAmpRatio 를 안 쓴다)
	const float Base          = Spec.GetSetByCallerMagnitude(ERTags::Data_Damage_Base,          false, 0.f);
	const float APRatio       = Spec.GetSetByCallerMagnitude(ERTags::Data_Damage_APRatio,       false, 0.f);
	const float BonusAPRatio  = Spec.GetSetByCallerMagnitude(ERTags::Data_Damage_BonusAPRatio,  false, 0.f);
	const float SkillAmpRatio = Spec.GetSetByCallerMagnitude(ERTags::Data_Damage_SkillAmpRatio, false, 0.f);

	// 체력 비례 계수. ⚠ MaxHP 와 CurHP 를 바꿔 넣으면 스킬 성격이 정반대가 된다 -
	//   재키 Q 는 **현재** 체력 5% 라 대상이 죽어갈수록 약해지는데,
	//   최대 체력으로 계산하면 **처형기**가 된다.
	const float MaxHPRatio  = Spec.GetSetByCallerMagnitude(ERTags::Data_Damage_MaxHPRatio,  false, 0.f);
	const float CurHPRatio  = Spec.GetSetByCallerMagnitude(ERTags::Data_Damage_CurHPRatio,  false, 0.f);
	const float LostHPRatio = Spec.GetSetByCallerMagnitude(ERTags::Data_Damage_LostHPRatio, false, 0.f);

	// ── 캡처값 ──────────────────────────────────────────────
	float AttackPower = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.AttackPowerDef, EvalParams, AttackPower);

	// ⭐ 추가 공격력. 새 어트리뷰트를 만들지 않고 GAS 내장 계산을 쓴다.
	//   EvaluateBonus() = Evaluate() - GetBaseValue() (GameplayEffectAggregator.cpp:381)
	//   Base 는 Instant GE(초기값·레벨 성장), Bonus 는 Duration/Infinite GE(장비·버프)다.
	//   ⚠ 장비 GE 를 Instant 로 만들면 이 값이 조용히 작아진다.
	//   근거: Docs/4_Argument/5_추가공격력_산출방식.md
	float BonusAttackPower = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeBonusMagnitude(S.AttackPowerDef, EvalParams, BonusAttackPower);

	float SkillAmp = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.SkillAmpDef, EvalParams, SkillAmp);

	float Defense = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.DefenseDef, EvalParams, Defense);

	float DefPenPercent = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.DefPenPercentDef, EvalParams, DefPenPercent);

	float DefPenFlat = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.DefPenFlatDef, EvalParams, DefPenFlat);

	float CritChance = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.CritChanceDef, EvalParams, CritChance);

	float CritDamageUp = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.CritDamageUpDef, EvalParams, CritDamageUp);

	float DamageUp = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.DamageUpDef, EvalParams, DamageUp);

	float DamageDown = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.DamageDownDef, EvalParams, DamageDown);

	float BasicAtkAmp = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.BasicAtkAmpDef, EvalParams, BasicAtkAmp);

	float BasicAtkDamageDown = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.BasicAtkDamageDownDef, EvalParams, BasicAtkDamageDown);

	float SkillDamageDown = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.SkillDamageDownDef, EvalParams, SkillDamageDown);

	float ModeDamageUp = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.ModeDamageUpDef, EvalParams, ModeDamageUp);

	float ModeDamageDown = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.ModeDamageDownDef, EvalParams, ModeDamageDown);

	float Lifesteal = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.LifestealDef, EvalParams, Lifesteal);

	float OmniLifesteal = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.OmniLifestealDef, EvalParams, OmniLifesteal);

	// 체력 비례 피해용. ⚠ Target 과 Source 를 헷갈리면 완전히 다른 스킬이 된다.
	float TargetMaxHP = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.TargetMaxHPDef, EvalParams, TargetMaxHP);

	float TargetHP = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.TargetHPDef, EvalParams, TargetHP);

	float SourceMaxHP = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.SourceMaxHPDef, EvalParams, SourceMaxHP);

	float SourceHP = 0.f;
	ExecParams.AttemptCalculateCapturedAttributeMagnitude(S.SourceHPDef, EvalParams, SourceHP);

	// ── 3. 계수 적용 → 방어력 적용 전 원시 피해 ─────────────
	//
	// 기본 공격은 계수 없이 공격력 그대로다. 그건 어빌리티가 APRatio = 1 로 넘긴다 -
	// 여기서 채널을 보고 분기하지 않는다.
	float Raw = Base
		+ AttackPower      * APRatio
		+ BonusAttackPower * BonusAPRatio
		+ SkillAmp         * SkillAmpRatio;

	// ── 3-b. 체력 비례 성분 ─────────────────────────────────
	//
	// ⚠ 셋을 정확히 구분한다. 바꿔 넣으면 스킬 성격이 정반대가 된다.
	//   MaxHPRatio  : 대상의 **최대** 체력 (매그너스 W·E, 레니 R, 채찍 D)
	//   CurHPRatio  : 대상의 **현재** 체력 (재키 Q, 다니엘 단검 D) - 죽어갈수록 약해진다
	//   LostHPRatio : **공격자가 잃은** 체력 (시셀라 R, 재키 R)
	const float SourceLostHP = FMath::Max(SourceMaxHP - SourceHP, 0.f);

	float Proportional =
		  TargetMaxHP  * MaxHPRatio
		+ TargetHP     * CurHPRatio
		+ SourceLostHP * LostHPRatio;

	if (Proportional > 0.f)
	{
		// ── 대상별 감쇠 ─────────────────────────────────────
		//
		// ⭐⭐ **비례 피해에만 곱한다.** 일반 피해(Raw)에 곱하면
		//   보스가 **모든** 피해를 절반만 받는 완전히 다른 게임이 된다.
		//   Task 문서가 "가장 위험한 실수" 로 지목한 항목이다.
		//
		// ⭐ 태그로 판별한다. Cast<ABossActor> 를 하면 F03 이 F12 에 의존하게 되고,
		//   야생동물이 없는 지금 F03 을 끝낼 수 없다.
		//   계수는 설정에 둔다 - 0.5 는 커뮤니티 자료 기반이라 밸런싱하며 바뀔 값이다.
		//   근거: Docs/4_Argument/7_비례피해_감쇠판별.md
		const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
		if (TargetTags && TargetTags->HasTag(ERTags::Actor_Type_Boss))
		{
			Proportional *= UERStatCapSettings::Get().BossProportionalDamageScale;
		}

		Raw += Proportional;
	}

	// ── 채널 판별 ───────────────────────────────────────────
	//
	// 채널은 GE 애셋의 태그로 판별한다. 애셋 태그는 스펙 생성 시
	// CapturedSourceTags.GetSpecTags() 로 들어간다 (GameplayEffect.cpp:1581).
	const FGameplayTagContainer& SpecTags = Spec.CapturedSourceTags.GetSpecTags();
	const bool bTrueDamage   = SpecTags.HasTag(ERTags::Damage_Type_True);
	const bool bBasicAttack  = SpecTags.HasTag(ERTags::Damage_Type_BasicAttack);
	const bool bSkill        = SpecTags.HasTag(ERTags::Damage_Type_Skill);

	// ⭐ 태그가 하나도 없으면 **조용히 넘기지 않는다.**
	//   자체 구현이었다면 EDamageChannel 열거형이라 값을 안 넣으면 컴파일이 걸렸다.
	//   GAS 는 태그를 안 붙여도 아무 일도 일어나지 않아, 어느 분기에도 안 들어간 채 통과한다.
	if (!bTrueDamage && !bBasicAttack && !bSkill)
	{
		UE_LOG(LogEternalReturn, Warning,
			TEXT("[데미지] %s 에 피해 채널 태그가 없다. Damage.Type.BasicAttack / Skill / True 중 하나가 필요하다. ")
			TEXT("스킬로 취급한다 - 치명타가 붙지 않는다."),
			*GetNameSafe(Spec.Def));
	}

	// ── 1. 모드 보정 계수 ───────────────────────────────────
	//
	// 모든 채널이 마지막에 공유한다. 고정 피해도 이것만은 받는다.
	const float ModeMul = 1.f + ModeDamageUp - ModeDamageDown;

	float Damage;

	if (bTrueDamage)
	{
		// ── 2. 고정 피해 채널 → 즉시 종료 ───────────────────
		//
		// ⭐ 방어력·피해감소·증폭·치명타 **어느 것도 받지 않는다.**
		//
		// ⚠ 계산식 §2.2 는 2 단계가 3 단계(계수 적용)보다 **앞**이라,
		//   고정 피해는 Ctx.BaseDamage 만 쓰고 공격력 계수를 타지 않는다.
		//   문서대로 구현했지만 이게 의도인지는 **(미확인)** 이다 -
		//   "공격력에 비례하는 고정 피해 스킬" 을 만들 수 없는 형태다.
		//   ⚠ 3-b(체력 비례)도 마찬가지로 안 탄다. 2 단계가 3·3-b 보다 앞이다.
		//   실험체를 붙일 때 다시 본다.
		Damage = Base * ModeMul;
	}
	else
	{
		// ── 4. 적용 방어력 (관통) ───────────────────────────
		//
		// ⭐ **순서가 고정이다. 퍼센트 관통이 먼저, 고정 관통이 나중.**
		//   계산식 §2.2 4단계가 "순서 고정" 이라고 못 박았다. 뒤집으면 결과가 달라진다 -
		//   방어력 100 에 관통 50% / 20 이면 정순 30, 역순 40 이다.
		//
		// ⚠ 음수 방어력을 추가 피해로 바꾸지 않는다 (자체 결정).
		//   관통이 방어력보다 크면 음수가 실제로 나온다.
		//
		// DefPenPercent 는 F02-03 이 상한 0.8 로 자른다 - 100% 관통은 나오지 않는다.
		float EffectiveDefense = Defense * (1.f - DefPenPercent);
		EffectiveDefense -= DefPenFlat;
		EffectiveDefense = FMath::Max(EffectiveDefense, 0.f);

		// 5. 방어력 감산. 방어력이 올라갈수록 효율이 떨어지지만 0 이 되지 않는다 -
		//    무한 방어가 나오지 않는 이유다. 근거: 스탯 문서 §6.2 · §3
		Damage = Raw * (100.f / (100.f + EffectiveDefense));

		// ── 6. 치명타 ───────────────────────────────────────
		//
		// ⭐⭐ **채널 검사가 맨 앞이다.** 확률을 먼저 굴리고 나중에 채널을 보면,
		//   리팩터링 중 순서가 뒤집혀 **스킬에 치명타가 붙는다.**
		//   §7 4순위의 검증 기준이 이것 하나다 -
		//   "스킬 채널에서는 어떤 경우에도 치명타가 발생하지 않음".
		//
		// ⚠ 적용 위치는 **방어력 감산 뒤**다. 계산식 §2.2 가 6 단계를 5 단계 뒤에 뒀다.
		//
		// 난수는 서버에서만 돈다 (Execution 자체가 서버 전용). 클라가 다시 굴릴 경로가 없다.
		// ⚠ GAS 예측을 켜면 이야기가 달라진다. 지금은 켜지 않는다 (CLAUDE.md §8).
		if (bBasicAttack && FMath::FRand() < CritChance)
		{
			// 기본 175%. 0.75 는 이 프로젝트의 밸런스 상수다.
			// 기획자가 조정할 일이 생기면 그때 별도 설정으로 뺀다 (지금은 요구가 없다).
			constexpr float CritBaseBonus = 0.75f;
			Damage *= 1.f + (CritBaseBonus + CritDamageUp);
		}

		// ── 7. 공통 피해 증감 ───────────────────────────────
		//
		// 가감산 상쇄다. DamageDown 은 F02-03 이 상한 0.8 로 자르므로 음수가 되지 않는다.
		Damage *= 1.f + DamageUp - DamageDown;

		// ── 8. 채널별 증감 ──────────────────────────────────
		//
		// ⚠ **기본공격과 스킬의 규칙이 다르다.** 기본공격은 가감산 상쇄,
		//   스킬은 증폭 항이 없고 감소만 있다. 계산식 §2.2 8 단계 그대로다.
		//   (스킬 증폭은 3 단계에서 이미 계수로 들어갔다)
		Damage *= bBasicAttack
			? (1.f + BasicAtkAmp - BasicAtkDamageDown)
			: (1.f - SkillDamageDown);

		// 9. 최종 피해 추가 - **보류**. 원본 표기가 `x 최종 피해 추가(%)` 라
		//    (1+x) 인지 x 배인지 해석이 안 됐다 (§8). 추측해서 넣으면 피해가 배 단위로 틀린다.

		// ── 10. 모드 보정 ───────────────────────────────────
		Damage *= ModeMul;
	}

	// ── 11. 무효화 계층 (F06-06) ────────────────────────────
	//
	// ⭐ **피해 면역** — 시셀라 W. 여기까지 계산한 뒤 통째로 0 으로 만든다.
	//
	// ⚠⚠ **CC 는 막지 않는다.** 피해 면역은 *"CC는 그대로 적용됨"* 이라고
	//   역기획서 §3.4 가 명시한다. 그래서 여기(피해 계산)에서만 처리하고
	//   CC 부여 경로(ERCC::ApplyCC)는 건드리지 않는다.
	//
	// ⭐ **무적(State.Invulnerable)도 여기서 본다.**
	//   사용자 확인 (2026-09-10): *"무적이지만 CC 기는 걸리긴 해"*
	//   -> 역기획서 §3.4 의 "무적의 CC 차단 여부 (미확인)" 이 **해소됐다.**
	//      무적도 CC 는 걸리므로 **피해만 0** 이고, 피해 면역과 로직이 같다.
	//
	// ⚠⚠ **그래도 태그는 합치지 않는다.** 역기획서 §3.4 가 "반드시 다른 플래그" 라고
	//   명시하고, 원작 서술에 **피격 판정** 차이가 시사돼 있다 —
	//   피해 면역은 *"피격 판정 자체는 남고 표식을 남기는 효과는 막을 수 없다"*.
	//   그 차이가 확인되면 나눠야 하는데, 지금 합쳐 놓으면 **못 나눈다.**
	//   ⚠ 온힛·표식 처리는 아직 없다 (F07/F08). 그때 여기서 갈린다. **(미확인)**
	//
	// ⚠ 위쪽(비례 피해)의 TargetTags 는 그 블록 스코프 안이다. EvalParams 것을 쓴다.
	if (EvalParams.TargetTags
		&& (EvalParams.TargetTags->HasTag(ERTags::State_DamageImmune)
		 || EvalParams.TargetTags->HasTag(ERTags::State_Invulnerable)))
	{
		// ⚠ 계산을 건너뛰지 않고 **끝에서 0** 으로 만든다.
		//   중간에 빠져나가면 흡혈·비례 피해 같은 후처리가 어긋난다.
		Damage = 0.f;
	}

	// 음수 피해는 회복이 되어 버린다. 여기서 자른다.
	// ⚠ 상한(최대 체력 등) 클램프는 여기서 하지 않는다. F02-03 의 PreAttributeChange 가 한다.
	Damage = FMath::Max(Damage, 0.f);

	if (Damage > 0.f)
	{
		// ⭐ 체력이 아니라 IncomingDamage 에 넣는다. 받는 쪽은 F02-04 다.
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UERAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, Damage));
	}

	// ── 흡혈 (F03-05) ───────────────────────────────────────
	//
	// ⭐ **출력 수정자로는 할 수 없다.** 출력은 언제나 Target 에 적용된다
	//   (GameplayEffect.cpp:3809-3821 - Owner 는 피격자의 ASC 다).
	//   그래서 공격자의 ASC 에 회복 GE 를 직접 적용한다.
	//   근거: Docs/4_Argument/6_흡혈_적용경로.md
	if (Damage > 0.f)
	{
		// ⭐ Lifesteal 은 기본 공격 피해만, OmniLifesteal 은 모든 유형이다 (역기획서 §1.4).
		//   기본 공격에서는 둘 다 적용되므로 합한다.
		const float LifestealRate = (bBasicAttack ? Lifesteal : 0.f) + OmniLifesteal;

		if (LifestealRate > 0.f)
		{
			// ── 치유 감소 ───────────────────────────────────
			//
			// ⭐ **겹쳐도 가장 높은 것 하나만** 적용한다. 곱하지 않는다.
			//   곱하면 감소 원인이 늘어날수록 회복이 기하급수로 0 에 수렴한다.
			//   (2026-09-08 결정, 자체 결정값)
			//
			// ⚠⚠ **아래 숫자는 "깎는 비율" 이다.** 역기획서 §1.4 의
			//   "광역 50%, 야생동물 60%" 를 그대로 곱하면 안 된다 -
			//   곱하는 값은 (1 - HealCut) 이다. 이 문서 초판이 실제로 이걸 틀렸다.
			float HealCut = 0.f;
			if (Spec.CapturedSourceTags.GetSpecTags().HasTag(ERTags::Damage_Shape_AoE))
			{
				HealCut = FMath::Max(HealCut, 0.5f);   // 광역: 50% 감소
			}
			if (Spec.CapturedTargetTags.GetAggregatedTags()
				&& Spec.CapturedTargetTags.GetAggregatedTags()->HasTag(ERTags::Actor_Type_Wildlife))
			{
				HealCut = FMath::Max(HealCut, 0.6f);   // 야생동물: 60% 감소
			}

			const float HealAmount = Damage * LifestealRate * (1.f - HealCut);

			if (HealAmount > 0.f)
			{
				if (UAbilitySystemComponent* SourceASC = ExecParams.GetSourceAbilitySystemComponent())
				{
					// ⚠ 회복은 **공격자**에게 간다. Target 이 아니다.
					FGameplayEffectContextHandle HealContext = SourceASC->MakeEffectContext();
					HealContext.AddSourceObject(SourceASC->GetOwnerActor());

					FGameplayEffectSpec HealSpec(
						GetDefault<UERLifestealEffect>(), HealContext, 1.f);
					HealSpec.SetSetByCallerMagnitude(ERTags::SetByCaller_HealAmount, HealAmount);

					SourceASC->ApplyGameplayEffectSpecToSelf(HealSpec);
				}
			}
		}
	}
#endif // WITH_SERVER_CODE
}
