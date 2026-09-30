// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "ERAnimNotify_HitMarker.generated.h"

/**
 * "여기가 맞는 순간" **표시만** 하는 노티파이 (Argument 52 T2). 게임 로직 권한이 없다 — Notify() 는 아무것도 안 한다.
 *
 * ⭐ 판정은 서버의 CastTime 타이머다 (CLAUDE.md §8 · ERSkillData.h CastTime). 연출 컴포넌트가 이 노티파이의 **위치만 읽어**
 *   선딜 구간 재생 속도 = 마커 시각 ÷ CastTime 으로 틀고, 판정 순간에 1배속으로 되돌린다 → 손이 맞는 순간이 항상 CastTime 에 온다.
 * 애니를 바꾸거나 속도를 바꿔도 마커가 애니와 같이 움직이니 CastTime 을 다시 맞출 필요가 없다. 스킨 · 캐릭터마다 다른 애니여도 각자 맞는다.
 */
UCLASS(meta = (DisplayName = "ER 타격 지점 (표시용)"))
class ETERNALRETURN_API UERAnimNotify_HitMarker : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override { return TEXT("ER 타격 지점"); }
};
