// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"   // TMap 의 키라 전방 선언 불가

#include "ERInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * 입력 액션 묶음.
 *
 * ⭐ 코드가 액션을 **이름이 아니라 애셋 참조**로 찾는다.
 *   문자열로 찾으면 오타가 런타임까지 안 걸리고, 이름을 바꿀 때 조용히 깨진다.
 *
 * Lyra 의 ULyraInputConfig 와 같은 형태다 (UDataAsset, LyraInputConfig.h:39).
 * 근거: Docs/6_Lyra참조/03_데이터애셋_구성.md
 *
 * ⚠ **UPrimaryDataAsset 이 아니라 UDataAsset 이다.** 이건 단독으로 로드·언로드할
 *   대상이 아니라 컨트롤러에 딸려 가는 설정이다 (같은 문서 §2).
 */
UCLASS(BlueprintType, Const, Meta = (DisplayName = "ER 입력 설정"))
class ETERNALRETURN_API UERInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	// ── 매핑 컨텍스트 ───────────────────────────────────────
	//
	// ⚠ IMC_Practice 는 **일부러 넣지 않았다.**
	//   연습 모드 즉시 이동(Ctrl+Alt+우클릭)이 거기 들어가는데,
	//   연습 모드 밖에서 켜지면 **텔레포트 치트**가 된다
	//   (역기획서 §10). 연습 모드를 만들 때 그때 추가한다.

	/** 평소 쓰는 컨텍스트. */
	UPROPERTY(EditDefaultsOnly, Category = "컨텍스트")
	TObjectPtr<UInputMappingContext> DefaultContext;

	/** 사망 · 관전 중. 이동·스킬이 빠져 있어야 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "컨텍스트")
	TObjectPtr<UInputMappingContext> DeadContext;

	/**
	 * 우선순위. 여러 컨텍스트가 같은 키를 쓰면 **높은 쪽이 이긴다.**
	 * UI 컨텍스트를 나중에 추가할 때 이보다 높게 준다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "컨텍스트")
	int32 DefaultContextPriority = 0;

	// ── 액션 ────────────────────────────────────────────────

	/** 우클릭 이동. ⚠ 우클릭이라는 것은 역기획서 §1.2 의 〔제안〕이며 원작 확인은 (미확인). */
	UPROPERTY(EditDefaultsOnly, Category = "이동")
	TObjectPtr<UInputAction> MoveToCursor;

	/** 카메라 잠금 토글. F05-03 에서 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "카메라")
	TObjectPtr<UInputAction> ToggleCameraLock;

	// ── 스킬 슬롯 (F07-01) ──────────────────────────────────
	//
	// ⭐ **액션 하나 = 슬롯 태그 하나.** 컨트롤러가 이 표를 순회해 바인딩하고,
	//   눌리면 그 태그로 TryActivateAbilitiesByTag 를 부른다.
	//   슬롯이 늘어도(F 슬롯 등) 코드를 안 고친다 — 여기 한 줄 추가하면 된다.
	//
	// ⚠ **슬롯 인덱스와 키를 분리한다.** 역기획서 §1.1 — "F 는 D 키와 스왑 가능하다".
	//   태그(슬롯)와 액션(키)이 이 표로 묶이므로 키 재배치는 IMC 애셋에서만 바뀐다.
	//
	// ⚠ D 슬롯은 무기가 소유하지만(F11) **입력 액션은 여기 둔다.** 키는 캐릭터 것이다.
	//   어빌리티가 안 붙어 있으면 TryActivate 가 그냥 실패한다 — 에러가 아니다.

	/** 슬롯 태그 → 입력 액션. Ability.Slot.P/Q/W/E/R/D 를 넣는다. */
	UPROPERTY(EditDefaultsOnly, Category = "스킬", meta = (Categories = "Ability.Slot"))
	TMap<FGameplayTag, TObjectPtr<UInputAction>> SkillSlotActions;
};
