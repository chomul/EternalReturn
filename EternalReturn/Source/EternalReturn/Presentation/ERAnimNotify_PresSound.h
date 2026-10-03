// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "ERAnimNotify_PresSound.generated.h"

/**
 * 모션 소리 노티파이 (F19-01 K8 · Argument 59 N2) — 엔진 Play Sound 와 같은 칸 · 같은 사용법 + **스킨이 바꾼다**.
 *
 * Sound 에 기본(S000) 소리를 직접 고른다. 재생 때 시전자 연출 컴포넌트가 지금 스킨의 `SoundSwaps` 에 짝이 있으면 그걸 튼다
 * (짝은 `ER.Pres.Fill` 이 같은 파일명으로 채운다). 애니는 스킨마다 복제하지 않는다.
 *
 * ⚠ **모션 소리만** (조준 · 스캔 · 장전 · 발소리 …). 판정 순간 소리(발사 · 타격)는 서버 큐가 낸다 (`GameplayCue.Pres.Attack` · `.Hit`)
 *   — 여기에도 넣으면 두 번 난다. 데디 서버는 애니를 평가하지 않아 울리지 않는다.
 */
UCLASS(const, hidecategories = Object, collapsecategories, meta = (DisplayName = "ER 소리 (스킨 교체)"))
class ETERNALRETURN_API UERAnimNotify_PresSound : public UAnimNotify_PlaySound
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
