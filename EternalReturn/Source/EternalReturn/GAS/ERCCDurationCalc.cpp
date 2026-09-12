// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERCCDurationCalc.h"

#include "EternalReturn.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayTags.h"

namespace
{
	/**
	 * 대상의 CCResist 캡처 정의.
	 *
	 * ⚠ **Snapshot 이 false 다.** 지속시간은 적용 시점에 한 번 계산되므로 스냅샷 여부가
	 *   결과를 바꾸지 않지만, "부여 시점의 저항으로 계산한다" 는 뜻을 명시적으로 남긴다
	 *   (역기획서 §4 — "저항 감산은 부여 시점 1회로 처리한다").
	 */
	struct FERCCResistCapture
	{
		FGameplayEffectAttributeCaptureDefinition CCResistDef;

		FERCCResistCapture()
		{
			CCResistDef = FGameplayEffectAttributeCaptureDefinition(
				UERAttributeSet::GetCCResistAttribute(),
				EGameplayEffectAttributeCaptureSource::Target,
				/*bSnapshot=*/false);
		}
	};

	const FERCCResistCapture& CCResistCapture()
	{
		static FERCCResistCapture Capture;
		return Capture;
	}
}

UERCCDurationCalc::UERCCDurationCalc()
{
	RelevantAttributesToCapture.Add(CCResistCapture().CCResistDef);
}

float UERCCDurationCalc::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	// ── ① 스킬이 준 길이 ──────────────────────────────────────
	//
	// ⚠ 값이 없으면 0 이다. 그러면 CC 가 안 걸린다 — ERCC::ApplyCC 가 미리 검사하지만
	//   이 클래스를 다른 경로에서 쓸 수도 있으니 여기서도 한 번 짚는다.
	const float BaseDuration = Spec.GetSetByCallerMagnitude(
		ERTags::SetByCaller_CCDuration, /*WarnIfNotFound=*/false, /*DefaultIfNotFound=*/0.f);

	if (BaseDuration <= 0.f)
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[CC] %s 의 SetByCaller.CCDuration 이 없다. 지속시간이 0 이 되어 CC 가 걸리지 않는다."),
			*GetNameSafe(Spec.Def));
		return 0.f;
	}

	// ── ② 대상의 방해 저항 ────────────────────────────────────
	FAggregatorEvaluateParameters EvalParams;
	EvalParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvalParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	float CCResist = 0.f;
	GetCapturedAttributeMagnitude(CCResistCapture().CCResistDef, Spec, EvalParams, CCResist);

	// ⚠ **여기서 상한을 자르지 않는다.** CCResist 는 F02-03 이 [0, 0.8] 로 이미 자른다.
	//   두 곳에서 자르면 한쪽을 고쳤을 때 조용히 어긋난다.
	//   ⭐ 다만 음수는 막는다 — 저항이 음수면 지속시간이 늘어나 버린다.
	CCResist = FMath::Max(CCResist, 0.f);

	const float FinalDuration = BaseDuration * (1.f - CCResist);

	UE_LOG(LogEternalReturn, Verbose,
		TEXT("[CC] %s 지속시간 %.2f초 -> %.2f초 (방해 저항 %.0f%%)"),
		*GetNameSafe(Spec.Def), BaseDuration, FinalDuration, CCResist * 100.f);

	return FinalDuration;
}
