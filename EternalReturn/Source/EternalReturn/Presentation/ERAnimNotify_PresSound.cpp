// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/ERAnimNotify_PresSound.h"

#include "Components/SkeletalMeshComponent.h"
#include "EternalReturn.h"
#include "Kismet/GameplayStatics.h"
#include "Presentation/ERPresentationComponent.h"
#include "Sound/SoundBase.h"

FString UERAnimNotify_PresSound::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("ER 소리 %s"), *GetNameSafe(Sound.Get()));
}

void UERAnimNotify_PresSound::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	// Super 를 부르지 않는다 — 부르면 기본 소리가 한 번 더 난다. 재생 몸통은 엔진 Play Sound 와 같다 (AnimNotify_PlaySound.cpp:37-72) · 소리만 스킨 짝으로
	if (!Sound || !MeshComp)
	{
		return;
	}
	const AActor* Owner = MeshComp->GetOwner();
	const UERPresentationComponent* Pres = Owner ? Owner->FindComponentByClass<UERPresentationComponent>() : nullptr;
	USoundBase* ToPlay = Pres ? Pres->ResolveSound(Sound) : Sound.Get();   // 애니 에디터 미리보기엔 연출 컴포넌트가 없다 → 기본 소리
	if (!ToPlay || !ToPlay->IsOneShot())
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[연출] 노티파이 소리 %s — 한 번 재생 소리가 아니다 (%s) · 안 틂"), *GetNameSafe(ToPlay), *GetNameSafe(Animation));
		return;
	}
	UWorld* World = MeshComp->GetWorld();
#if WITH_EDITORONLY_DATA
	if (bPreviewIgnoreAttenuation && World && World->WorldType == EWorldType::EditorPreview)
	{
		if (MeshComp->IsPlaying())
		{
			UGameplayStatics::PlaySound2D(World, ToPlay, VolumeMultiplier, PitchMultiplier);
		}
		return;
	}
#endif
	if (bFollow)
	{
		UGameplayStatics::SpawnSoundAttached(ToPlay, MeshComp, AttachName, FVector(ForceInit), EAttachLocation::SnapToTarget, false, VolumeMultiplier, PitchMultiplier);
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(World, ToPlay, MeshComp->GetComponentLocation(), VolumeMultiplier, PitchMultiplier);
	}
	if (Pres)
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[연출] %s 노티파이 소리 %s [%s] (%s)"), *GetNameSafe(Owner), *ToPlay->GetName(),
			ToPlay != Sound ? TEXT("스킨 교체") : TEXT("기본"), *GetNameSafe(Animation));
	}
}
