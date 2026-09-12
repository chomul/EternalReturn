// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERCCLibrary.h"

#include "AbilitySystemComponent.h"
#include "EternalReturn.h"
#include "GameplayEffect.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSlowEffect.h"

namespace ERCC
{

bool ApplyCC(
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC,
	TSubclassOf<UGameplayEffect> CCEffect,
	float DurationSeconds)
{
	// ── ① 대상 ────────────────────────────────────────────────
	if (!TargetASC)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[CC] 대상 ASC 가 없다. 부여할 수 없다."));
		return false;
	}

	// ── ② 서버 권위 ───────────────────────────────────────────
	//
	// ⚠ 클라가 자기에게 CC 를 거는 경로가 있으면 안 된다. 반대로 클라가 자기 CC 를
	//   **푸는** 경로가 되면 그게 더 나쁘다 — 기절을 무시하는 치트가 된다.
	if (!TargetASC->IsOwnerActorAuthoritative())
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[CC] 서버가 아니다. CC 부여는 서버 권위다. (%s)"),
			*GetNameSafe(TargetASC->GetOwnerActor()));
		return false;
	}

	// ── ③ GE 클래스 ───────────────────────────────────────────
	if (!CCEffect)
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[CC] CC GE 클래스가 비어 있다. 애셋을 지정하지 않았다. (%s)"),
			*GetNameSafe(TargetASC->GetOwnerActor()));
		return false;
	}

	const UGameplayEffect* EffectDef = CCEffect->GetDefaultObject<UGameplayEffect>();
	if (!EffectDef)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[CC] %s 의 CDO 를 얻지 못했다."), *CCEffect->GetName());
		return false;
	}

	// ── ④ ⭐⭐ 지속시간 정책 — 가장 흔한 애셋 실수 ─────────────
	//
	// ⚠⚠ **Instant GE 는 태그를 부여하지 못한다.**
	//   "This is a non-predicted instant effect (it never gets added to
	//    ActiveGameplayEffects)" — AbilitySystemComponent.cpp:951
	//   태그 부여는 ActiveGameplayEffects 에 등록된 GE 만 한다. 그래서 Instant 로
	//   두면 **CC 가 조용히 안 걸린다.** 에러도 경고도 없다.
	//
	// ⚠ 그리고 **UGameplayEffect 의 기본값이 Instant 다** (GameplayEffect.cpp:146).
	//   새 애셋을 만들면 이 상태로 시작한다 — 그래서 반드시 검사한다.
	//
	// ⚠ Infinite 도 막는다. 만료가 없으니 영구 CC 가 된다.
	if (EffectDef->DurationPolicy != EGameplayEffectDurationType::HasDuration)
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[CC] %s 의 Duration Policy 가 HasDuration 이 아니다. ")
			TEXT("Instant 는 태그를 부여하지 못하고 Infinite 는 안 풀린다. ")
			TEXT("애셋에서 Duration Policy 를 Has Duration 으로 바꾼다."),
			*CCEffect->GetName());
		return false;
	}

	// ── ⑤ 지속시간 ────────────────────────────────────────────
	//
	// ⚠ SetByCaller 를 빠뜨리면 0 이 되어 CC 가 걸렸다 말았다 한다.
	//   같은 기절이 스킬마다 0.5~1.3초라서 값을 애셋에 못 박는다 (역기획서 §3.1).
	if (DurationSeconds <= 0.f)
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[CC] %s 의 지속시간이 %.2f 다. 0 이하면 CC 가 걸리지 않는다."),
			*CCEffect->GetName(), DurationSeconds);
		return false;
	}

	// ── ⑥ 태그를 실제로 부여하는지 ────────────────────────────
	//
	// ⚠⚠ UE 5.3 에서 GE 의 태그 부여가 **UTargetTagsGameplayEffectComponent** 로
	//   옮겨졌다 (GameplayEffect.h:2285 — InheritableOwnedTagsContainer deprecated).
	//   애셋에 그 컴포넌트를 추가하지 않으면 태그가 하나도 안 붙는다.
	//   인터넷 예제 대부분이 5.2 이전이라 "Granted Tags" 칸을 찾다가 여기 걸린다.
	//
	// ⚠ 어떤 축 태그가 붙어야 하는지는 검사하지 않는다 — CC 마다 다르다.
	//   실명·시야 차단은 이동·평타·스킬을 막지 않는 것이 **정상**이다(역기획서 §3.3).
	//   그래서 "축 태그가 있는가"가 아니라 "태그가 하나라도 있는가"를 본다.
	if (EffectDef->GetGrantedTags().IsEmpty())
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[CC] %s 가 태그를 하나도 부여하지 않는다. ")
			TEXT("애셋의 Components 에 'Grant Tags to Target Actor' 를 추가하고 ")
			TEXT("Add Tags 에 State.CC.* 와 State.Block.* 를 넣는다."),
			*CCEffect->GetName());
		return false;
	}

	// ── ⑦ 적용 ────────────────────────────────────────────────
	//
	// SourceASC 가 없으면 대상 자신을 시전자로 둔다. 환경 피해나 디버그 커맨드가 그렇다.
	UAbilitySystemComponent* Instigator = SourceASC ? SourceASC : TargetASC;

	FGameplayEffectContextHandle Context = Instigator->MakeEffectContext();
	const FGameplayEffectSpecHandle SpecHandle =
		Instigator->MakeOutgoingSpec(CCEffect, 1.f, Context);

	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[CC] %s 의 스펙을 만들지 못했다."), *CCEffect->GetName());
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_CCDuration, DurationSeconds);

	const FActiveGameplayEffectHandle Applied =
		Instigator->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);

	if (!Applied.WasSuccessfullyApplied())
	{
		// ⚠ 여기까지 왔는데 실패하는 정상적인 경우가 있다 — 면역(State.CCImmune),
		//   ApplicationTagRequirements 불충족. 그래서 Error 가 아니라 Verbose 다.
		UE_LOG(LogEternalReturn, Verbose,
			TEXT("[CC] %s 가 %s 에게 적용되지 않았다 (면역 또는 태그 요구 불충족)."),
			*CCEffect->GetName(), *GetNameSafe(TargetASC->GetOwnerActor()));
		return false;
	}

	UE_LOG(LogEternalReturn, Verbose, TEXT("[CC] %s 를 %s 에게 %.2f초 부여했다."),
		*CCEffect->GetName(), *GetNameSafe(TargetASC->GetOwnerActor()), DurationSeconds);

	return true;
}

// ═══════════════════════════════════════════════════════════════
//  둔화 재계산 (F06-02)
// ═══════════════════════════════════════════════════════════════

void RecalculateSlow(UAbilitySystemComponent* ASC)
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	// ── ① 걸려 있는 둔화들 중 가장 큰 감소율을 찾는다 ────────
	//
	// ⭐ 나머지는 **제거하지 않는다.** 계산에서만 뺀다 —
	//   강한 것이 만료되면 이 함수가 다시 불려 남은 것 중 최대가 적용된다.
	FGameplayTagContainer SlowQuery;
	SlowQuery.AddTag(ERTags::State_CC_Slow);

	// ⚠⚠ **GetActiveEffectsWithAllTags 를 쓰면 안 된다.** 이름과 달리
	//   그 함수는 MakeQuery_MatchAllEffectTags 를 쓰고(AbilitySystemComponent.cpp:1522),
	//   EffectTagQuery 는 **애셋 태그만** 본다(GameplayEffect.cpp:5714).
	//   우리 State.CC.Slow 는 GE 가 **부여하는** 태그(Granted)라 하나도 안 잡힌다.
	//
	// ⭐ OwningTagQuery 는 애셋 태그와 부여 태그를 **둘 다** 본다
	//   (GameplayEffect.cpp:5697-5698). 그래서 이쪽을 쓴다.
	//   근거: Docs/5_ErrorReport/E09_둔화_태그조회_애셋태그.md
	const TArray<FActiveGameplayEffectHandle> SlowHandles =
		ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(SlowQuery));

	float MaxSlowPercent = 0.f;
	for (const FActiveGameplayEffectHandle& Handle : SlowHandles)
	{
		const FActiveGameplayEffect* ActiveEffect = ASC->GetActiveGameplayEffect(Handle);
		if (!ActiveEffect)
		{
			continue;
		}

		// ⚠ 값이 없으면 0 이다. 경고를 끄는 이유: 둔화가 아닌 GE 가 State.CC.Slow 를
		//   갖고 있을 수 있고(면역·표시용), 그때마다 로그가 쏟아지면 소용이 없다.
		//   대신 아래에서 "태그는 있는데 값이 0" 인 경우를 한 번 짚는다.
		const float SlowPercent = ActiveEffect->Spec.GetSetByCallerMagnitude(
			ERTags::SetByCaller_SlowPercent, /*WarnIfNotFound=*/false, /*DefaultIfNotFound=*/0.f);

		if (SlowPercent <= 0.f)
		{
			UE_LOG(LogEternalReturn, Warning,
				TEXT("[둔화] %s 가 State.CC.Slow 를 갖고 있는데 SetByCaller.SlowPercent 가 없다. ")
				TEXT("애셋의 SetByCaller 설정을 확인한다."),
				*GetNameSafe(ActiveEffect->Spec.Def));
			continue;
		}

		MaxSlowPercent = FMath::Max(MaxSlowPercent, SlowPercent);
	}

	// ── ①-b 둔화 저항으로 **강도**를 깎는다 (F06-05) ──────────
	//
	// ⭐⭐ SlowResist 는 **지속시간이 아니라 수치(강도)** 를 줄인다.
	//   원작: "둔화 저항 20% 일 때 둔화 40% 를 맞으면 40의 20%인 8%만 감소" -> 32% 둔화
	//   ⚠ 역기획서 §4 는 "지속시간을 감소" 라고 적었지만 **틀렸다.**
	//     근거: Docs/4_Argument/14_저항_적용방식.md 파트 1
	//
	// ⚠ 지속시간을 줄이는 것은 **CCResist** 다. 둔화는 두 저항을 **모두** 받는다 —
	//   지속시간은 UERCCDurationCalc 가, 강도는 여기가 처리한다.
	//
	// ⭐ 여기서 계산하는 이유: 둔화 강도는 GE 모디파이어를 안 거치고
	//   이 함수가 직접 정한다. 계산 클래스를 따로 만들 필요가 없다.
	if (MaxSlowPercent > 0.f)
	{
		// ⚠ 상한(0.8)은 F02-03 이 이미 자른다. 여기서 또 자르지 않는다.
		//   ⭐ 음수만 막는다 — 저항이 음수면 둔화가 강해져 버린다.
		const float SlowResist = FMath::Max(
			ASC->GetNumericAttribute(UERAttributeSet::GetSlowResistAttribute()), 0.f);

		MaxSlowPercent *= (1.f - SlowResist);
	}

	// ⚠ 감소율 100% 는 이동속도 0 이 된다. 하한 클램프가 막아 주지만
	//   중간값이 0 이나 음수가 되는 것 자체를 피한다.
	MaxSlowPercent = FMath::Clamp(MaxSlowPercent, 0.f, 0.99f);

	// ── ② 기존 UERSlowEffect 를 먼저 제거한다 ─────────────────
	//
	// ⚠⚠ **제거가 먼저다.** Multiply 모디파이어는 합산되므로
	//   (GameplayEffectAggregator.cpp:194-207) 두 개가 동시에 있으면 값이 틀린다.
	//   같은 함수 안에서 제거 -> 적용하므로 프레임 경계를 넘지 않아 속도가 튀지 않는다.
	ASC->RemoveActiveGameplayEffectBySourceEffect(UERSlowEffect::StaticClass(), ASC);

	// ── ③ 둔화가 없으면 여기서 끝 ─────────────────────────────
	if (MaxSlowPercent <= 0.f)
	{
		// ⭐ **조회 개수를 함께 찍는다.** E09 처럼 태그 조회가 조용히 실패하면
		//   "조회된 GE 0개" 가 바로 원인을 가리킨다. 이 부류는 에러가 안 나서
		//   로그가 없으면 어디서 막혔는지 알 수 없다.
		//   보려면: Log LogEternalReturn Verbose
		UE_LOG(LogEternalReturn, Verbose, TEXT("[둔화] %s - 걸린 둔화 없음 (조회된 GE %d개). 해제."),
			*GetNameSafe(ASC->GetOwnerActor()), SlowHandles.Num());
		return;
	}

	// ── ④ 최대 감소율로 다시 적용한다 ────────────────────────
	//
	// ⭐ 감소율(0.6) -> 배수(0.4) 변환은 **여기 한 곳에서만** 한다.
	//   태그를 둘로 나눈 이유가 이것이다 (ERGameplayTags.h 참조).
	const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	const FGameplayEffectSpecHandle SpecHandle =
		ASC->MakeOutgoingSpec(UERSlowEffect::StaticClass(), 1.f, Context);

	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[둔화] UERSlowEffect 스펙을 만들지 못했다."));
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(
		ERTags::SetByCaller_SlowMultiplier, 1.f - MaxSlowPercent);

	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);

	UE_LOG(LogEternalReturn, Verbose, TEXT("[둔화] %s — 최대 감소율 %.0f%% (배수 %.2f), 둔화 %d개 중"),
		*GetNameSafe(ASC->GetOwnerActor()), MaxSlowPercent * 100.f, 1.f - MaxSlowPercent,
		SlowHandles.Num());
}

void BindSlowRecalculation(UAbilitySystemComponent* ASC)
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	// ⚠⚠ **재진입 방지가 핵심이다.** UERSlowEffect 를 적용·제거하는 것도
	//   이 델리게이트들을 발동시킨다. 자기 자신을 걸러내지 않으면 무한 루프가 된다.
	//
	// ⭐ State.CC.Slow 태그로 거르지 않는 이유: UERSlowEffect 는 태그가 없어서
	//   ①의 목록에는 안 들어오지만, **델리게이트는 여전히 불린다.**
	//   그래서 클래스로 판별한다.

	ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddWeakLambda(ASC,
		[](UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle)
		{
			if (Spec.Def && Spec.Def->IsA(UERSlowEffect::StaticClass()))
			{
				return; // 자기 자신이다. 무시한다
			}
			RecalculateSlow(TargetASC);
		});

	ASC->OnAnyGameplayEffectRemovedDelegate().AddWeakLambda(ASC,
		[ASC](const FActiveGameplayEffect& Removed)
		{
			if (Removed.Spec.Def && Removed.Spec.Def->IsA(UERSlowEffect::StaticClass()))
			{
				return; // 자기 자신이다. 무시한다
			}
			RecalculateSlow(ASC);
		});
}

} // namespace ERCC
