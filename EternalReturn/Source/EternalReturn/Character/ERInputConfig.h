// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

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
};
