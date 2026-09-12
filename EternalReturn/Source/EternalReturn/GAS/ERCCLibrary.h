// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

class UAbilitySystemComponent;
class UGameplayEffect;

/**
 * CC 부여 경로. **CC 는 전부 이 함수를 통해 걸린다.**
 *
 * ⭐ 한 곳으로 모으는 이유는 **검사할 자리를 만들기 위해서다.**
 *   CC GE 를 애셋으로 두기로 했고(Argument 9 방안 A), 애셋 설정이 잘못되면
 *   GAS 는 **아무 말 없이 CC 를 안 건다.** 그 침묵을 여기서 로그로 바꾼다.
 *
 * ⚠ BP 에 노출하지 않는다. CC 부여는 스킬(C++)이 하는 일이고,
 *   BP 에 열면 §7 의 "로직은 C++" 경계가 무너진다.
 *
 * 근거: Docs/4_Argument/9_CC효과_표현방식.md · 10_CC_차단축_태그설계.md
 */
namespace ERCC
{
	/**
	 * [서버] CC 를 부여한다.
	 *
	 * @param SourceASC   시전자. nullptr 이면 TargetASC 를 시전자로 둔다(환경 피해·디버그)
	 * @param TargetASC   대상
	 * @param CCEffect    CC GE 애셋 클래스 (HasDuration 이어야 한다)
	 * @param DurationSeconds  지속시간. GE 의 SetByCaller.CCDuration 으로 들어간다
	 * @return 실제로 적용됐으면 true
	 *
	 * ⚠ 실패하면 **반드시 로그를 남긴다.** 조용히 false 를 돌려주지 않는다.
	 */
	bool ApplyCC(
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC,
		TSubclassOf<UGameplayEffect> CCEffect,
		float DurationSeconds);

	/**
	 * [서버] 둔화를 다시 계산해 UERSlowEffect 를 갱신한다.
	 *
	 * ⭐ **가장 강한 둔화 하나만 적용한다.** 나머지는 제거하지 않고 계산에서만 뺀다 —
	 *   강한 것이 만료되면 남은 것 중 가장 강한 것이 자동으로 적용된다.
	 *   근거: Docs/4_Argument/12_둔화_중첩방식.md (방안 B)
	 *
	 * ⚠ **직접 부를 필요는 없다.** BindSlowRecalculation 이 GE 추가·제거에 연결한다.
	 */
	void RecalculateSlow(UAbilitySystemComponent* ASC);

	/**
	 * [서버] 둔화 재계산을 ASC 의 GE 추가·제거에 연결한다. **한 번만 부른다.**
	 *
	 * ⚠ 서버에서만 부른다. 재계산은 서버 권위다 (CLAUDE.md §6) —
	 *   MoveSpeed 어트리뷰트가 복제되므로 클라는 결과만 받는다.
	 */
	void BindSlowRecalculation(UAbilitySystemComponent* ASC);
}
