// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_Mode.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "AbilitySystemComponent.h"
#include "Core/ERPlayerController.h"
#include "Core/ERPlayerState.h"
#include "EternalReturn.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "TimerManager.h"

// ─────────────────────────────────────────────────────────────
// 상태
// ─────────────────────────────────────────────────────────────

UERGameplayAbility* UERSkillModeState::GetAbility() const
{
	return Cast<UERGameplayAbility>(GetOuter());
}

void UERSkillModeState::OnExpired()                                  { Exit(TEXT("시간")); }
void UERSkillModeState::OnCancelledByCC()                            { Exit(TEXT("CC")); }
void UERSkillModeState::OnCancelledByMove(FGameplayEventData Payload) { Exit(TEXT("이동")); }

void UERSkillModeState::OnAnyAbilityEnded(const FAbilityEndedData& Data)
{
	UERGameplayAbility* A = GetAbility();
	const UERSkillFragment_Mode* F = Frag.Get();
	AERPlayerState* PS = A ? Cast<AERPlayerState>(A->GetOwningActorFromActorInfo()) : nullptr;
	if (!bActive || !A || !F || !PS || Data.bWasCancelled || Data.AbilitySpecHandle != PS->GetModeAttackHandle())
	{
		return;
	}
	const UERSkillData* Skill = A->GetSkillData();
	++Shots;
	UE_LOG(LogEternalReturn, Log, TEXT("[모드] %s <- %s %d/%d발"), *GetNameSafe(A->GetOwningActorFromActorInfo()), *GetNameSafe(Skill), Shots, F->Mode.Shots);
	UERGameplayAbility::DebugDrawText(A->GetAvatarActorFromActorInfo(), FString::Printf(TEXT("[모드] %d/%d발%s"), Shots, F->Mode.Shots,
		(Shots == F->Mode.Shots - 1 && F->Mode.FinalAttackData) ? *FString::Printf(TEXT(" -> %s"), *GetNameSafe(F->Mode.FinalAttackData)) : TEXT("")));

	// ⚠ 지금은 그 평타 어빌리티의 EndAbility 안 — 스펙 교체/해제는 다음 틱에 (ClearAbility 가 그 인스턴스를 치운다).
	TWeakObjectPtr<UERSkillModeState> WeakThis(this);
	const bool bLast = Shots >= F->Mode.Shots;
	const bool bSwapFinal = !bLast && F->Mode.FinalAttackData && Shots == F->Mode.Shots - 1;
	if (bLast || bSwapFinal)
	{
		A->GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis, bLast]()
		{
			UERSkillModeState* Self = WeakThis.Get();
			if (!Self || !Self->bActive) { return; }
			if (bLast)
			{
				Self->Exit(TEXT("소진"));
			}
			else if (UERGameplayAbility* Ab = Self->GetAbility())
			{
				if (AERPlayerState* P = Cast<AERPlayerState>(Ab->GetOwningActorFromActorInfo()))
				{
					if (const UERSkillFragment_Mode* Fr = Self->Frag.Get()) { P->SetModeAttack(Fr->Mode.FinalAttackData, Ab->GetAbilityLevel()); }
				}
			}
		}));
	}
}

void UERSkillModeState::Cleanup(const TCHAR* Reason)
{
	if (!bActive)
	{
		return;
	}
	bActive = false;
	UERGameplayAbility* A = GetAbility();
	const UERSkillFragment_Mode* F = Frag.Get();
	UAbilitySystemComponent* ASC = A ? A->GetAbilitySystemComponentFromActorInfo() : nullptr;
	if (!A || !F)
	{
		return;
	}
	if (ASC && EndedHandle.IsValid())
	{
		ASC->OnAbilityEnded.Remove(EndedHandle);
		EndedHandle.Reset();
	}
	if (AERPlayerState* PS = Cast<AERPlayerState>(A->GetOwningActorFromActorInfo()))
	{
		PS->SetModeAttack(nullptr, 0);
	}

	// 미사용 해제 → 남은 쿨다운의 일부를 돌려준다 (원문 "쿨 50% 반환"). 쿨다운 GE 의 시작 시각을 앞당긴다.
	float Refunded = 0.f;
	const FGameplayTagContainer* CooldownTags = &A->GetCooldownTagsForFragment();
	if (ASC && CooldownTags && Shots == 0 && F->Mode.UnusedCooldownRefund > 0.f)
	{
		const float Now = ASC->GetWorld()->GetTimeSeconds();
		for (const FActiveGameplayEffectHandle& H : ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(*CooldownTags)))
		{
			if (const FActiveGameplayEffect* Active = ASC->GetActiveGameplayEffect(H))
			{
				const float Remaining = Active->GetTimeRemaining(Now);
				if (Remaining > 0.f)
				{
					Refunded = Remaining * F->Mode.UnusedCooldownRefund;
					ASC->ModifyActiveEffectStartTime(H, -Refunded);
				}
			}
		}
	}

	UERGameplayAbility::DebugDrawText(A->GetAvatarActorFromActorInfo(), FString::Printf(TEXT("[모드] 해제 (%s) · %d발%s"), Reason, Shots, Refunded > 0.f ? *FString::Printf(TEXT(" · 쿨 %.1f초 반환"), Refunded) : TEXT("")), FColor::Orange);
	UE_LOG(LogEternalReturn, Log, TEXT("[모드] %s <- %s 해제 (%s) · %d발 · 평타 복구%s"),
		*GetNameSafe(A->GetOwningActorFromActorInfo()), *GetNameSafe(A->GetSkillData()), Reason, Shots,
		Refunded > 0.f ? *FString::Printf(TEXT(" · 미사용 쿨 %.1f초 반환"), Refunded) : TEXT(""));
}

void UERSkillModeState::Exit(const TCHAR* Reason)
{
	Cleanup(Reason);
	if (UERGameplayAbility* A = GetAbility())
	{
		A->EndFromFragment(/*bCancelled=*/false);
	}
}

// ─────────────────────────────────────────────────────────────
// 조각
// ─────────────────────────────────────────────────────────────

void UERSkillFragment_Mode::OnExecute(FERSkillContext& Ctx) const
{
	UERGameplayAbility* A = Ctx.Ability;
	if (!A || !Ctx.bAuthority || Mode.Duration <= 0.f)
	{
		return;
	}
	if (!Ctx.PlayerState || !Ctx.ASC || !Mode.AttackData)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[모드] %s <- %s 진입 실패 — PlayerState 가 아니거나 Mode.AttackData 가 없다."),
			*GetNameSafe(A->GetOwningActorFromActorInfo()), *GetNameSafe(Ctx.Skill));
		return;
	}
	UERSkillModeState* S = Ctx.GetState<UERSkillModeState>(this);
	if (!S)
	{
		return;
	}
	S->Frag = this;
	S->bActive = true;
	S->Shots = 0;
	Ctx.bKeepActive = true;   // 어빌리티는 EndAbility 하지 않는다 — S->Exit 가 끝낸다

	Ctx.PlayerState->SetModeAttack(Mode.AttackData, Ctx.Level);
	Ctx.PlayerState->SetModeAimLimit(Ctx.AimDirection, Mode.AimHalfAngleDeg);   // 진입 시 바라본 방향 ± 반각 (저격 30°)

	// 해제 조건 셋 — 선딜(BeginCast)과 같은 AbilityTask · 같은 태그/이벤트 (Argument 17). 어빌리티와 함께 죽는다. 콜백은 상태 객체에.
	UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(A, Mode.Duration);
	Wait->OnFinish.AddDynamic(S, &UERSkillModeState::OnExpired);
	Wait->ReadyForActivation();

	UAbilityTask_WaitGameplayTagAdded* WaitCC = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(A, ERTags::State_Block_Skill, nullptr, /*OnlyTriggerOnce=*/true);
	WaitCC->Added.AddDynamic(S, &UERSkillModeState::OnCancelledByCC);
	WaitCC->ReadyForActivation();

	if (Mode.bCancelOnMove)
	{
		UAbilityTask_WaitGameplayEvent* WaitMove = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(A, ERTags::Event_Input_Move, nullptr, /*OnlyTriggerOnce=*/true);
		WaitMove->EventReceived.AddDynamic(S, &UERSkillModeState::OnCancelledByMove);
		WaitMove->ReadyForActivation();
	}

	// 발수 — 모드 평타 스펙이 끝날 때마다 (GAS 델리게이트). 빗나가도 한 발 (원작 탄 소모).
	S->EndedHandle = Ctx.ASC->OnAbilityEnded.AddUObject(S, &UERSkillModeState::OnAnyAbilityEnded);

	UERGameplayAbility::DebugDrawText(Ctx.Avatar, FString::Printf(TEXT("[모드] 진입 %.0f초 · 0/%d발 · 평타 -> %s"), Mode.Duration, Mode.Shots, *GetNameSafe(Mode.AttackData)));
	UE_LOG(LogEternalReturn, Log, TEXT("[모드] %s <- %s 진입 %.1f초 · %d발 (평타 -> %s%s) · 이동 해제 %s"),
		*GetNameSafe(A->GetOwningActorFromActorInfo()), *GetNameSafe(Ctx.Skill), Mode.Duration, Mode.Shots,
		*GetNameSafe(Mode.AttackData), Mode.FinalAttackData ? *FString::Printf(TEXT(" · 마지막 %s"), *GetNameSafe(Mode.FinalAttackData)) : TEXT(""),
		Mode.bCancelOnMove ? TEXT("O") : TEXT("X"));
}

void UERSkillFragment_Mode::OnLocalExecute(FERSkillContext& Ctx) const
{
	if (Mode.Duration <= 0.f || !Ctx.Ability)
	{
		return;
	}
	const FGameplayAbilityActorInfo* Info = Ctx.Ability->GetCurrentActorInfo();
	AERPlayerController* PC = Cast<AERPlayerController>(Info ? Info->PlayerController.Get() : nullptr);
	if (!PC)
	{
		return;
	}
	// 진행 중인 이동 정지 — 이동은 **클라 PC** 가 구동하므로(E07 · SimpleMoveToLocation) 로컬에서 끊어야 실제로 선다.
	//   이후 새 이동 명령은 Event.Input.Move 로 모드를 해제한다 (원작: 이동하면 해제).
	if (Mode.bStopMovementOnEnter)
	{
		PC->StopMovement();
		UE_LOG(LogEternalReturn, Verbose, TEXT("[모드] 진입 — 진행 중 이동 정지"));
	}
	// 카메라 줌아웃 + 조준 방향 오프셋 — PC 가 보간. 복구는 OnEnd.
	if (Mode.CameraZoomScale != 1.f || Mode.CameraAimOffset > 0.f)
	{
		PC->SetCameraZoom(Mode.CameraZoomScale, Ctx.AimDirection.GetSafeNormal2D() * Mode.CameraAimOffset * 100.f);
	}
}

void UERSkillFragment_Mode::OnEnd(FERSkillContext& Ctx, bool bCancelled) const
{
	if (!Ctx.Ability || Mode.Duration <= 0.f)
	{
		return;
	}
	// 카메라 복구 — 줌을 건 쪽(로컬)에서. 부드럽게 돌아간다.
	if (Ctx.Ability->IsLocallyControlled())
	{
		const FGameplayAbilityActorInfo* Info = Ctx.Ability->GetCurrentActorInfo();
		if (AERPlayerController* PC = Cast<AERPlayerController>(Info ? Info->PlayerController.Get() : nullptr))
		{
			PC->SetCameraZoom(1.f, FVector::ZeroVector);
		}
	}
	// 서버 — 외부 취소(사망 · CancelAbility)도 평타를 복구한다. Exit 경로면 이미 정리돼 아무 일도 없다.
	if (Ctx.bAuthority)
	{
		if (UERSkillModeState* S = Ctx.GetState<UERSkillModeState>(this))
		{
			S->Cleanup(bCancelled ? TEXT("취소") : TEXT("종료"));
		}
	}
}
