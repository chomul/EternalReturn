// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ERPlayerController.h"
#include "Character/ERCharacterBase.h"
#include "Character/ERInputConfig.h"
#include "Combat/ERRideComponent.h"
#include "ERCollisionChannels.h"
#include "EternalReturn.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "DrawDebugHelpers.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERAttributeSet.h"
#include "Combat/ERTargeting.h"
#include "Core/ERGameState.h"
#include "GAS/ERReviveAbility.h"
#include "Core/ERTeamStatics.h"
#include "GAS/ERSkillData.h"
#include "GAS/Fragment/ERSkillFragment_Mode.h"
#include "NavigationSystem.h"
#include "Presentation/ERPresentationComponent.h"

AERPlayerController::AERPlayerController()
{
	// 클릭 이동이라 커서가 보여야 한다.
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AERPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// ⚠ 매핑 컨텍스트는 **로컬 컨트롤러에만** 붙인다. 서버에서 붙이면 아무 의미가 없고,
	//   데디케이티드 서버에서는 서브시스템 자체가 없다.
	if (!IsLocalController())
	{
		return;
	}

	if (!InputConfig || !InputConfig->DefaultContext)
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[입력] %s 의 Input Config 또는 Default Context 가 비어 있다. 이동이 동작하지 않는다."),
			*GetNameSafe(this));
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(InputConfig->DefaultContext, InputConfig->DefaultContextPriority);

		UE_LOG(LogEternalReturn, Log, TEXT("[입력] Default 컨텍스트 적용 (우선순위 %d)"),
			InputConfig->DefaultContextPriority);
	}
}

void AERPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		// 프로젝트 설정의 Default Input Component Class 가 EnhancedInputComponent 여야 한다.
		UE_LOG(LogEternalReturn, Error,
			TEXT("[입력] InputComponent 가 UEnhancedInputComponent 가 아니다. ")
			TEXT("Project Settings > Input > Default Input Component Class 를 확인한다."));
		return;
	}

	if (!InputConfig || !InputConfig->MoveToCursor)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[입력] MoveToCursor 액션이 비어 있다."));
		return;
	}

	EnhancedInput->BindAction(InputConfig->MoveToCursor, ETriggerEvent::Started,
		this, &AERPlayerController::OnMoveToCursor);

	if (InputConfig->ToggleCameraLock)
	{
		EnhancedInput->BindAction(InputConfig->ToggleCameraLock, ETriggerEvent::Started,
			this, &AERPlayerController::OnToggleCameraLock);
	}

	// ── 스킬 슬롯 (F07-01) ──────────────────────────────────
	//
	// ⭐ 표를 순회해 바인딩한다. 슬롯마다 핸들러를 따로 만들지 않는다 —
	//   BindAction 의 페이로드 오버로드로 **슬롯 태그를 같이 넘긴다.**
	for (const TPair<FGameplayTag, TObjectPtr<UInputAction>>& Pair : InputConfig->SkillSlotActions)
	{
		if (!Pair.Key.IsValid() || !Pair.Value)
		{
			// ⚠ 표에 빈 줄이 있다. 애셋 실수다. 나머지는 계속 바인딩한다.
			UE_LOG(LogEternalReturn, Warning,
				TEXT("[입력] Skill Slot Actions 에 빈 항목이 있다 (태그=%s, 액션=%s)."),
				*Pair.Key.ToString(), *GetNameSafe(Pair.Value));
			continue;
		}

		// 누르면 미리보기 · 떼면 발동 (Argument 56 C2). 평타는 Started 에서 바로 (OnSkillSlotStarted 가 가른다)
		EnhancedInput->BindAction(Pair.Value, ETriggerEvent::Started,
			this, &AERPlayerController::OnSkillSlotStarted, Pair.Key);
		EnhancedInput->BindAction(Pair.Value, ETriggerEvent::Completed,
			this, &AERPlayerController::OnSkillSlotCompleted, Pair.Key);

		UE_LOG(LogEternalReturn, Log, TEXT("[입력] 슬롯 바인딩: %s -> %s"),
			*Pair.Key.ToString(), *GetNameSafe(Pair.Value));
	}

	// ⚠ 표가 비어 있으면 스킬 키가 하나도 안 먹는다. 조용히 넘어가면 원인을 못 찾는다.
	if (InputConfig->SkillSlotActions.IsEmpty())
	{
		UE_LOG(LogEternalReturn, Warning,
			TEXT("[입력] Skill Slot Actions 가 비어 있다. 스킬 키가 동작하지 않는다."));
	}
}

namespace
{
	/**
	 * F11-05 D: 모드(저격) 중 같은 슬롯 재입력은 **사격** — 원작 확인 (사용자 2026-09-21): 모드에서 D 또는 좌클릭 = 발사, 우클릭(이동) = 해제.
	 * 모드 스킬 인스턴스가 활성이면 입력을 Attack 슬롯으로 돌린다 (모드가 평타 슬롯을 저지사격으로 바꿔 뒀다). 클라 · 서버 양쪽에서 같은 규칙.
	 */
	FGameplayTag ResolveModeSlot(const UAbilitySystemComponent& ASC, FGameplayTag SlotTag)
	{
		for (const FGameplayAbilitySpec& Spec : ASC.GetActivatableAbilities())
		{
			const UERGameplayAbility* ER = Cast<UERGameplayAbility>(Spec.GetPrimaryInstance());
			const UERSkillData* Skill = Cast<UERSkillData>(Spec.SourceObject.Get());
			// 모드 조각만 — 활성 유지하는 다른 조각(빈사 스킬 도화선 · 자가 소생)은 재입력이 사격이 아니다
			if (ER && Spec.DynamicAbilityTags.HasTagExact(SlotTag) && ER->IsModeActive() && Skill && Skill->FindFragment<UERSkillFragment_Mode>())
			{
				UE_LOG(LogEternalReturn, Verbose, TEXT("[입력] %s -> 모드 중 재입력 = %s"), *SlotTag.ToString(), *ERTags::Ability_Slot_Attack.GetTag().ToString());
				return ERTags::Ability_Slot_Attack;
			}
		}
		return SlotTag;
	}

	/**
	 * F14 S4 빈사 스킬 (Argument 79 I1): 빈사 중 Q · W · R = 빈사 스킬 슬롯. 일반 스킬과 슬롯 태그를 나눠 선판정 루프가 섞이지 않게 (첫 스펙에서 끝난다).
	 * 클라 · 서버 같은 규칙 (모드 재입력과 같은 자리). 그 밖의 키는 그대로 — 빈사면 일반 스킬 차단이 막는다.
	 */
	FGameplayTag ResolveDownedSlot(const UAbilitySystemComponent& ASC, FGameplayTag SlotTag)
	{
		if (!ASC.HasMatchingGameplayTag(ERTags::State_Downed))
		{
			return SlotTag;
		}
		if (SlotTag == ERTags::Ability_Slot_Q) { return ERTags::Ability_Slot_Downed_Q; }
		if (SlotTag == ERTags::Ability_Slot_W) { return ERTags::Ability_Slot_Downed_W; }
		if (SlotTag == ERTags::Ability_Slot_R) { return ERTags::Ability_Slot_Downed_R; }
		return SlotTag;
	}

	FGameplayTag ResolveSlot(const UAbilitySystemComponent& ASC, FGameplayTag SlotTag)
	{
		return ResolveModeSlot(ASC, ResolveDownedSlot(ASC, SlotTag));
	}

	/** 활성 중 재입력 = 취소인 스킬인가 (자가 제세동 — 원작 "재시전해서 취소") */
	bool CancelsOnReinput(const FGameplayAbilitySpec& Spec)
	{
		const UERSkillData* Skill = Cast<UERSkillData>(Spec.SourceObject.Get());
		if (!Spec.IsActive() || !Skill)
		{
			return false;
		}
		for (const TObjectPtr<UERSkillFragment>& F : Skill->Fragments)
		{
			if (F && F->CancelOnReinput()) { return true; }
		}
		return false;
	}
}

void AERPlayerController::OnSkillSlotStarted(FGameplayTag SlotTag)
{
	if (SlotTag.MatchesTagExact(ERTags::Ability_Slot_Attack))
	{
		OnSkillSlotPressed(SlotTag);   // 평타는 지금처럼 바로
		return;
	}
	PreviewSlot = SlotTag;
	// 누른 자리 고정 스킬 (레니 R) — 지금 커서가 설치 자리 · 떼는 커서는 방향
	HoldAnchorSlot = FGameplayTag();
	if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPawn()))
	{
		const FGameplayTag Slot = ResolveSlot(*ASC, SlotTag);
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			const UERSkillData* Skill = Spec.DynamicAbilityTags.HasTagExact(Slot) ? Cast<UERSkillData>(Spec.SourceObject.Get()) : nullptr;
			FHitResult Hit;
			if (Skill && Skill->bAimDirectionOnHold && GetHitResultUnderCursor(ECC_Visibility, false, Hit))
			{
				HoldAnchor = Hit.ImpactPoint;
				HoldAnchorSlot = SlotTag;
				UE_LOG(LogEternalReturn, Log, TEXT("[입력] %s 자리 고정 %s — 커서로 방향 · 떼면 발동"), *SlotTag.ToString(), *HoldAnchor.ToCompactString());
				break;
			}
		}
	}
}

void AERPlayerController::OnSkillSlotCompleted(FGameplayTag SlotTag)
{
	if (PreviewSlot != SlotTag)
	{
		return;   // 우클릭으로 취소했거나 평타
	}
	PreviewSlot = FGameplayTag();
	OnSkillSlotPressed(SlotTag);   // 발동 — 떼는 순간의 커서로 (경로는 그대로: 선판정 → 조준 실어 서버)
}

void AERPlayerController::DrawSkillPreview()
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPawn());
	if (!ASC)
	{
		return;
	}
	const FGameplayTag Slot = ResolveSlot(*ASC, PreviewSlot);
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.DynamicAbilityTags.HasTagExact(Slot))
		{
			continue;
		}
		const UERGameplayAbility* Ability = Cast<UERGameplayAbility>(Spec.GetPrimaryInstance());
		const UERSkillData* Skill = Cast<UERSkillData>(Spec.SourceObject.Get());
		FHitResult Hit;
		if (Ability && Skill && GetHitResultUnderCursor(ECC_Visibility, /*bTraceComplex=*/false, Hit))
		{
			if (HoldAnchorSlot.IsValid() && HoldAnchorSlot == PreviewSlot)
			{
				// 고정 자리 원 + 날릴 방향 화살표
				Ability->DrawPreview(*Skill, HoldAnchor);
				const FVector Up(0.f, 0.f, 30.f);
				DrawDebugDirectionalArrow(GetWorld(), HoldAnchor + Up, FVector(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, HoldAnchor.Z) + Up, 80.f, FColor::Yellow, false, 0.f, 0, 5.f);
			}
			else
			{
				Ability->DrawPreview(*Skill, Hit.ImpactPoint);
			}
		}
		return;
	}
}

void AERPlayerController::OnSkillSlotPressed(FGameplayTag SlotTag)
{
	UAbilitySystemComponent* ASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPawn());
	if (!ASC)
	{
		return;
	}

	// ⭐ **클라에서 부르면 GAS 가 서버로 보낸다.** 별도 RPC 가 필요 없다.
	//   NetExecutionPolicy 가 ServerInitiated 라 클라에서는 예측 실행 없이
	//   ServerTryActivateAbility 로 요청만 가고, 서버가 실행한 뒤 클라가 따라 실행한다.
	//
	// ⚠⚠ **TryActivateAbilitiesByTag 를 쓰면 안 된다.** 그 함수는 어빌리티 **클래스**의
	//   AbilityTags 만 본다 (AbilitySystemComponent_Abilities.cpp:1475) —
	//   GrantSkills 가 스펙의 **DynamicAbilityTags** 에 심은 슬롯 태그는 안 본다.
	//   E10 에서 이걸로 하루를 썼다. Docs/5_ErrorReport/E10 참조.
	//
	// ⭐ Lyra 와 같은 방식으로 **직접 순회**한다 (LyraAbilitySystemComponent.cpp:198-200).
	//   DynamicAbilityTags 에 있어야 "같은 로직 클래스를 여러 슬롯이 쓴다" 가 성립한다 —
	//   클래스 태그에 박으면 GA_Projectile 이 Q 인지 W 인지 애셋마다 갈라야 한다.
	//
	// ⚠ CC 차단은 여기서 검사하지 않는다. 어빌리티의 ActivationBlockedTags 가 막는다 (F06-01 · UERGameplayAbility 생성자).
	//   여기서 또 막으면 규칙이 두 곳에 생긴다.
	//
	// ⭐ F07-05: TryActivateAbility 대신 **조준을 실어** 서버로 보낸다 (Docs/4_Argument/18).
	//   TryActivateAbility 가 하던 클라 선판정(CanActivateAbility)은 여기서 **인스턴스**로 직접 한다 —
	//   엔진의 클라 선판정은 CDO 에서 돌아 쿨다운 태그를 못 본다 (E12). 인스턴스로 하면 쿨다운 중 헛 RPC 도 안 보낸다.
	SlotTag = ResolveSlot(*ASC, SlotTag);   // F11-05 D 모드 중 재입력 = 사격 · F14 빈사면 빈사 스킬 슬롯

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.Ability || !Spec.DynamicAbilityTags.HasTagExact(SlotTag))
		{
			continue;
		}

		// 활성 중 재입력 = 취소 (자가 제세동) — 선판정 없이 서버로 (서버가 다시 보고 끊는다)
		if (CancelsOnReinput(Spec))
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[입력] %s -> 활성 중 재입력 = 취소 요청 (%s)"), *SlotTag.ToString(), *GetNameSafe(Spec.SourceObject.Get()));
			ServerActivateSkill(SlotTag, FGameplayAbilityTargetDataHandle());
			return;
		}

		const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
		const UGameplayAbility* Checker = Instance ? Instance : Spec.Ability.Get();
		FGameplayTagContainer Relevant;   // 엔진이 실패 원인 태그(쿨다운 · 차단)를 채운다
		if (!Checker->CanActivateAbility(Spec.Handle, ASC->AbilityActorInfo.Get(), nullptr, nullptr, &Relevant))
		{
			// Log 레벨 — 안 나가는 이유를 Verbose 없이도 보게 (2026-09-21 저격 모드에서 사격이 안 나간 원인을 못 찾았다).
			//   엔진은 OptionalRelevantTags 를 거의 안 채운다 (5.4 CheckCooldown 은 전역 실패 태그가 없으면 빈 채로) — 세 검사를 따로 돌려 어느 것인지 찍는다.
			const FGameplayAbilityActorInfo* Info = ASC->AbilityActorInfo.Get();
			const bool bCooldown = Checker->CheckCooldown(Spec.Handle, Info);
			const bool bCost     = Checker->CheckCost(Spec.Handle, Info);
			const bool bTags     = Checker->DoesAbilitySatisfyTagRequirements(*ASC);
			const FGameplayTagContainer* CD = Checker->GetCooldownTags();
			FGameplayTagContainer Owned; ASC->GetOwnedGameplayTags(Owned);
			UE_LOG(LogEternalReturn, Log, TEXT("[입력] %s -> 클라 선판정 실패 — %s Lv.%d%s%s · 쿨다운 %s (%s) · 코스트 %s · 태그 %s · 보유 태그 [%s]"), *SlotTag.ToString(),
				*GetNameSafe(Spec.SourceObject.Get()), Spec.Level, Spec.IsActive() ? TEXT(" 활성 중") : TEXT(""), Instance ? TEXT("") : TEXT(" CDO"),
				bCooldown ? TEXT("OK") : TEXT("✘"), CD ? *CD->ToStringSimple() : TEXT("-"), bCost ? TEXT("OK") : TEXT("✘"), bTags ? TEXT("OK") : TEXT("✘"),
				*Owned.ToStringSimple());
			return;
		}

		// 조준 = 커서 아래 지점 + 액터. 못 찾으면 빈 핸들 — 서버가 시전자 정면으로 대신한다.
		//
		// ⚠ **Select 채널로 먼저 잰다.** 캐릭터 캡슐(Pawn 프로파일)은 Visibility 를 **무시**해서
		//   Visibility 트레이스는 캐릭터 위에서도 바닥을 잡는다 — SingleTarget 평타가 항상 적중 0 이었다 (2026-09-14, E13).
		//   메시가 있으면 메시가 Visibility 를 막아 주지만 그건 연출 메시에 판정을 맡기는 것이라 캡슐로 간다.
		//   Select 는 클릭 전용 채널이다 (2026-09-23) — 실험체는 캡슐이, 야생동물은 **히트박스**가 막는다 (몸 충돌은 캡슐만).
		//   지형은 Select 를 무시하므로 캐릭터 위가 아니면 아래 Visibility 트레이스가 바닥 지점을 준다.
		// ⭐ **발동은 이동 명령을 취소한다** — 평타 · 스킬 전부 (사용자 2026-09-23).
		//   안 멈추면 클릭 이동(SimpleMoveToLocation)의 경로가 살아 있어서, 시전이 끝나면 원래 가던 방향으로 다시 걸어간다.
		//   ⚠ 이동은 **클라가 몬다** (E07) — 그래서 여기(소유 클라)서 멈춘다. 저격 모드 진입은 _Mode 도 멈춘다(중복 무해).
		//   ⚠ 선판정을 통과했을 때만이다 — 쿨다운 중 연타로 걸음이 끊기면 안 된다.
		//   돌진 · 도약(_SelfMove)은 ERForcedMove 가 따로 미므로 여기서 멈춰도 영향이 없다.
		//   예외: 걸으며 쓰는 스킬 (bCastWhileMoving — 시셀라 Q · E · 사용자 2026-10-06)
		const UERSkillData* PressedSkill = Cast<UERSkillData>(Spec.SourceObject.Get());
		if (!PressedSkill || !PressedSkill->bCastWhileMoving)
		{
			StopMovement();
		}

		FGameplayAbilityTargetDataHandle Aim;
		FHitResult Hit;
		if (GetHitResultUnderCursor(ERCollisionChannel::Select, /*bTraceComplex=*/false, Hit)
			|| GetHitResultUnderCursor(ECC_Visibility, /*bTraceComplex=*/false, Hit))
		{
			Aim = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(Hit);
		}
		// 누른 자리 고정 스킬 (레니 R) — [0] = 누른 자리 · [1] = 지금 커서 (방향)
		if (HoldAnchorSlot.IsValid() && HoldAnchorSlot == SlotTag && Aim.Num() > 0)
		{
			FHitResult AnchorHit;
			AnchorHit.Location = AnchorHit.ImpactPoint = HoldAnchor;
			AnchorHit.bBlockingHit = true;
			FGameplayAbilityTargetDataHandle Two = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(AnchorHit);
			Two.Append(Aim);
			Aim = Two;
			UE_LOG(LogEternalReturn, Log, TEXT("[입력] %s 발동 — 자리 %s · 방향 점 %s"), *SlotTag.ToString(), *HoldAnchor.ToCompactString(), *Hit.ImpactPoint.ToCompactString());
		}
		HoldAnchorSlot = FGameplayTag();

		ServerActivateSkill(SlotTag, Aim);
		UE_LOG(LogEternalReturn, Verbose, TEXT("[입력] %s -> 발동 요청 (조준 %s)"),
			*SlotTag.ToString(), Aim.Num() > 0 ? *Hit.ImpactPoint.ToCompactString() : TEXT("없음"));
		return;
	}

	UE_LOG(LogEternalReturn, Verbose, TEXT("[입력] %s -> 슬롯에 스킬이 없다"), *SlotTag.ToString());
}

bool AERPlayerController::ServerActivateSkill_Validate(FGameplayTag SlotTag, FGameplayAbilityTargetDataHandle Aim)
{
	// 조작된 태그만 끊는다. 좌표는 어빌리티가 클램프한다 — 여기서 끊으면 연결이 닫힌다.
	return SlotTag.IsValid() && SlotTag.MatchesTag(ERTags::Ability_Slot);
}

void AERPlayerController::ServerActivateSkill_Implementation(FGameplayTag SlotTag, FGameplayAbilityTargetDataHandle Aim)
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPawn());
	if (!ASC)
	{
		return;
	}

	SlotTag = ResolveSlot(*ASC, SlotTag);   // 클라가 모드를 아직 모를 수도 있다 — 서버가 다시 판단

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.Ability || !Spec.DynamicAbilityTags.HasTagExact(SlotTag))
		{
			continue;
		}

		// 활성 중 재입력 = 취소 (자가 제세동 — 원작 "재시전해서 취소") · 취소 → 조각 OnEnd 가 쿨다운 반환
		if (CancelsOnReinput(Spec))
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[입력] %s -> %s 재입력 취소 (서버)"), *SlotTag.ToString(), *GetNameSafe(Spec.SourceObject.Get()));
			ASC->CancelAbilityHandle(Spec.Handle);
			return;
		}

		// ⭐ 서버(권위)에서는 TriggerAbilityFromGameplayEvent 가 허용된다 (AbilitySystemComponent_Abilities.cpp:2567-2579).
		//   내부에서 CanActivateAbility(인스턴스) 를 다시 검사한다 — 클라 선판정을 믿지 않는다.
		//   Payload.TargetData 가 어빌리티의 ActivateAbility(TriggerEventData) 로 간다.
		FGameplayEventData Payload;
		Payload.EventTag = ERTags::Event_Skill_Aim;
		Payload.Instigator = GetPawn();
		Payload.TargetData = Aim;

		const bool bActivated = ASC->TriggerAbilityFromGameplayEvent(
			Spec.Handle, ASC->AbilityActorInfo.Get(), ERTags::Event_Skill_Aim, &Payload, *ASC);

		if (bActivated)
		{
			UE_LOG(LogEternalReturn, Verbose, TEXT("[입력] 서버 %s -> 발동"), *SlotTag.ToString());
		}
		else
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[입력] 서버 %s -> 거부 — %s Lv.%d%s (쿨다운 · CC · 레벨 0 · 코스트 · 이미 활성)"),
				*SlotTag.ToString(), *GetNameSafe(Spec.SourceObject.Get()), Spec.Level, Spec.IsActive() ? TEXT(" 활성 중") : TEXT(""));
		}
		return;
	}
}

// ═══════════════════════════════════════════════════════════════
//  카메라 (F05-03)
// ═══════════════════════════════════════════════════════════════

void AERPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// 스킬 키를 누르고 있는 동안 범위 미리보기 (Argument 56 C2 · D1) — 로컬만
	if (PreviewSlot.IsValid() && IsLocalController())
	{
		DrawSkillPreview();
	}

	// 자동 공격 (Argument 67 A1) — 대상이 있을 때만 (평소엔 비용 0) · 소유 클라만
	if (bAutoAttacking && IsLocalController())
	{
		TickAutoAttack();
	}

	// ⭐ Tick 을 쓰는 이유: 가장자리 스크롤은 마우스 **위치**에 반응하므로
	//   키 입력 이벤트로는 표현되지 않는다. 매 프레임 평가가 실제로 필요한 경우다
	//   (CLAUDE.md §2 - "정말 매 프레임 필요할 때만").
	//
	// ⚠ 잠금 상태에서는 아무것도 하지 않는다. 기본값이 잠금이라 평소엔 비용이 없다.
	if (!bCameraLocked && bEdgeScrollEnabled)
	{
		UpdateEdgeScroll(DeltaTime);
	}

	// 스킬 줌 보간 — 목표와 다를 때만 (평소엔 비용 0). 거리는 캐릭터 리그에, 오프셋은 아래 ApplyCameraOffset 이 합친다.
	if (CurrentZoomScale != TargetZoomScale || CurrentZoomOffset != TargetZoomOffset)
	{
		CurrentZoomScale  = FMath::FInterpTo(CurrentZoomScale, TargetZoomScale, DeltaTime, CameraZoomInterpSpeed);
		CurrentZoomOffset = FMath::VInterpTo(CurrentZoomOffset, TargetZoomOffset, DeltaTime, CameraZoomInterpSpeed);
		if (FMath::IsNearlyEqual(CurrentZoomScale, TargetZoomScale, 0.001f)) { CurrentZoomScale = TargetZoomScale; }
		if (CurrentZoomOffset.Equals(TargetZoomOffset, 0.5f)) { CurrentZoomOffset = TargetZoomOffset; }
		if (AERCharacterBase* ERCharacter = Cast<AERCharacterBase>(GetPawn()))
		{
			ERCharacter->SetCameraZoomScale(CurrentZoomScale);
		}
	}

	ApplyCameraOffset();
}

bool AERPlayerController::ServerRequestRevive_Validate(AActor* Target)
{
	return true;   // 대상 없음 · 거리 · 팀은 정상 실패 (어빌리티가 로그) — 끊지 않는다
}

void AERPlayerController::ServerRequestRevive_Implementation(AActor* Target)
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPawn());
	if (!ASC || !Target)
	{
		return;
	}
	// 검사는 어빌리티가 (UERReviveAbility::CanRevive · 차단 태그) — PC 는 이벤트 하나만 (이동 이벤트와 같은 경계 · Argument 17)
	FGameplayEventData Event;
	Event.Instigator = GetPawn();
	Event.Target = Target;
	ASC->HandleGameplayEvent(ERTags::Event_Revive_Request, &Event);
}

void AERPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);
	if (!IsLocalController())
	{
		return;
	}
	if (InPawn && InPawn->GetRootComponent())
	{
		SetAudioListenerAttenuationOverride(InPawn->GetRootComponent(), FVector::ZeroVector);
	}
	else
	{
		ClearAudioListenerAttenuationOverride();
	}
}

void AERPlayerController::SetCameraZoom(float Scale, const FVector& WorldOffset)
{
	TargetZoomScale = FMath::Max(Scale, 0.1f);
	TargetZoomOffset = FVector(WorldOffset.X, WorldOffset.Y, 0.f);
	UE_LOG(LogEternalReturn, Verbose, TEXT("[카메라] 줌 목표 ×%.2f · 오프셋 %s"), TargetZoomScale, *TargetZoomOffset.ToCompactString());
}

void AERPlayerController::OnToggleCameraLock()
{
	bCameraLocked = !bCameraLocked;

	// 잠그면 캐릭터로 돌아온다. 밀려나 있던 오프셋을 버린다.
	if (bCameraLocked)
	{
		FreeCameraOffset = FVector::ZeroVector;
	}

	UE_LOG(LogEternalReturn, Log, TEXT("[카메라] %s"),
		bCameraLocked ? TEXT("잠금 - 캐릭터를 따라간다") : TEXT("해제 - 가장자리 스크롤"));
}

void AERPlayerController::UpdateEdgeScroll(float DeltaSeconds)
{
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		// 커서가 창 밖이다. 밀지 않는다.
		return;
	}

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0)
	{
		return;
	}

	// ⭐ 화면 방향을 먼저 구한다 (오른쪽 +X, 위쪽 +Y 인 2D).
	float ScreenRight = 0.f;
	float ScreenUp    = 0.f;

	if (MouseX <= EdgeScrollMargin)                   { ScreenRight = -1.f; }
	else if (MouseX >= ViewportX - EdgeScrollMargin)  { ScreenRight =  1.f; }

	if (MouseY <= EdgeScrollMargin)                   { ScreenUp =  1.f; }
	else if (MouseY >= ViewportY - EdgeScrollMargin)  { ScreenUp = -1.f; }

	if (FMath::IsNearlyZero(ScreenRight) && FMath::IsNearlyZero(ScreenUp))
	{
		return;
	}

	// ⭐⭐ 화면 축을 **카메라 회전에서 계산**한다. 월드 축으로 하드코딩하지 않는다.
	//   카메라 Yaw 가 0 이 아니라서(원작이 비스듬히 본다) +X/+Y 로 박으면
	//   가장자리 스크롤이 엉뚱한 방향으로 밀린다.
	//   ⭐ 이렇게 해 두면 카메라 Yaw 를 바꿔도 여기는 안 고쳐도 된다.
	const AERCharacterBase* ERCharacter = Cast<AERCharacterBase>(GetPawn());
	const float Yaw = ERCharacter ? ERCharacter->GetCameraYaw() : 0.f;
	const FRotator YawOnly(0.f, Yaw, 0.f);

	// 카메라가 내려다보므로 Forward 를 그대로 쓰면 아래로 파고든다. 바닥 평면에 눕힌다.
	const FVector GroundForward = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X);
	const FVector GroundRight   = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);

	const FVector Delta = (GroundForward * ScreenUp) + (GroundRight * ScreenRight);

	FreeCameraOffset += Delta.GetSafeNormal() * EdgeScrollSpeed * DeltaSeconds;

	// ⚠ 상한이 없으면 무한히 밀려나 캐릭터를 화면에서 잃어버린다.
	if (FreeCameraOffset.SizeSquared() > FMath::Square(MaxCameraOffset))
	{
		FreeCameraOffset = FreeCameraOffset.GetSafeNormal() * MaxCameraOffset;
	}
}

void AERPlayerController::ApplyCameraOffset()
{
	if (AERCharacterBase* ERCharacter = Cast<AERCharacterBase>(GetPawn()))
	{
		ERCharacter->SetCameraTargetOffset(FreeCameraOffset + CurrentZoomOffset);   // 잠금 해제 스크롤 + 스킬 줌 오프셋
	}
	// ⚠ 폰이 없는 동안(사망 ~ 부활)에는 반영할 곳이 없다.
	//   관전 카메라는 F14 에서 별도로 다룬다 (Argument 8 파트 2).
}

bool AERPlayerController::ResolveNavigableDestination(const FVector& ClickPoint, FVector& OutDestination) const
{
	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[이동] 네비게이션 시스템이 없다. 레벨에 NavMeshBoundsVolume 이 있는지 확인한다."));
		return false;
	}

	// ── ① 클릭 지점 근처에서 갈 수 있는 곳을 찾는다 ────────
	//
	// ⭐ 상자 위를 클릭해도 **옆 바닥**으로 스냅된다. 이게 MOBA 의 표준 동작이다 -
	//   못 가는 곳을 클릭하면 가장 가까운 갈 수 있는 곳으로 간다.
	FNavLocation Projected;
	if (NavSys->ProjectPointToNavigation(ClickPoint, Projected, NavProjectionExtent))
	{
		OutDestination = Projected.Location;
		return true;
	}

	// ── ② 실패하면 아래로 훑어 지면을 찾는다 ───────────────
	//
	// ⚠ 높은 벽·건물을 클릭하면 ①의 Z 반경(기본 500)으로는 바닥에 닿지 않는다.
	//   그 경우 클릭 지점에서 수직으로 내려 지면을 찾고, 거기서 다시 투영한다.
	//   벽 밑은 못 가는 자리라 투영이 **벽 바깥쪽**으로 밀어 준다.
	FHitResult GroundHit;
	const FVector TraceEnd = ClickPoint - FVector(0.f, 0.f, GroundTraceDownDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ERResolveClick), /*bTraceComplex=*/false);
	if (const APawn* ControlledPawn = GetPawn())
	{
		Params.AddIgnoredActor(ControlledPawn);
	}

	if (GetWorld()->LineTraceSingleByChannel(GroundHit, ClickPoint, TraceEnd, ECC_Visibility, Params))
	{
		if (NavSys->ProjectPointToNavigation(GroundHit.ImpactPoint, Projected, NavProjectionExtent))
		{
			OutDestination = Projected.Location;
			return true;
		}
	}

	// ── ③ 갈 수 있는 곳이 없다 ─────────────────────────────
	// 맵 밖이나 완전히 격리된 곳이다. 크래시가 아니라 무시가 맞다.
	return false;
}

void AERPlayerController::OnMoveToCursor()
{
	// 미리보기 중 우클릭 = 취소 (이동도 안 한다) `[자체]` — 원작 미확인 (Argument 56)
	if (PreviewSlot.IsValid())
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[입력] %s 미리보기 취소 (우클릭)"), *PreviewSlot.ToString());
		PreviewSlot = FGameplayTag();
		return;
	}

	// ── 이중 게이트 ① 입력 레이어 ──────────────────────────
	//
	// ⚠ **이건 방어선이 아니다.** 못 갈 명령을 서버에 안 보내서 반응을 자연스럽게
	//   하는 것이 목적이다. 이걸 우회해도 CMC 가 속도를 0 으로 만든다
	//   (UERCharacterMovementComponent). 그쪽이 실제 방어선이다.
	//
	// 근거: 역기획서 §9.2 · Docs/4_Argument/10_CC_차단축_태그설계.md
	if (IsMovementBlockedByCC())
	{
		return;
	}

	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, /*bTraceComplex=*/false, Hit))
	{
		// 하늘을 클릭한 경우 등. 조용히 무시한다 - 에러가 아니다.
		return;
	}

	// 탑승 중 (매그너스 R 바이크) — 경로를 시작하지 않고 조종 목표만 바꾼다 (누누 · 사용자 2026-10-03 · Argument 62 V3)
	if (UERRideComponent* Ride = GetPawn() ? GetPawn()->FindComponentByClass<UERRideComponent>() : nullptr; Ride && Ride->IsSteering())
	{
		SetAttackTarget(nullptr, TEXT("탑승"));
		Ride->SetSteerTarget(Hit.ImpactPoint);
		return;
	}

	// 자동 공격 (Argument 67 A1) — 커서 아래 살아 있는 적(실험체 · 야생동물)이면 대상으로. 이동 · 평타는 PlayerTick 이 한다
	//   Select 채널 — 평타 조준과 같은 이유 (캡슐 · 히트박스가 막는다 · E13)
	{
		FHitResult ActorHit;
		AActor* Clicked = GetHitResultUnderCursor(ERCollisionChannel::Select, /*bTraceComplex=*/false, ActorHit) ? ActorHit.GetActor() : nullptr;
		const UAbilitySystemComponent* ClickedASC = Clicked ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Clicked) : nullptr;
		const UAbilitySystemComponent* MyASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPawn());
		const bool bIncapacitated = MyASC && (MyASC->HasMatchingGameplayTag(ERTags::State_Downed) || MyASC->HasMatchingGameplayTag(ERTags::State_Dead));
		// 빈사 아군 우클릭 → 소생 (F14 · Argument 75 R1). 멀면 아래 이동으로 — 다가가서 다시 우클릭 `[자체]` (원작 자동 접근 (미확인))
		if (ClickedASC && GetPawn() && !bIncapacitated && Clicked != GetPawn() && ERTeamStatics::IsSameTeam(GetPawn(), Clicked)
			&& ClickedASC->HasMatchingGameplayTag(ERTags::State_Downed))
		{
			FString Why;
			if (UERReviveAbility::CanRevive(GetPawn(), Clicked, 0.f, &Why))
			{
				SetAttackTarget(nullptr, TEXT("소생"));
				StopMovement();
				// 빈사 아군 쪽을 바라본다 (사용자 2026-10-08) — 소유 클라 회전이 서버로 간다 (CMC) · 서버 어빌리티의 회전은 다른 창 몫. 순차 사격 몸 돌리기와 같은 이유
				const FVector ToTarget = (Clicked->GetActorLocation() - GetPawn()->GetActorLocation()).GetSafeNormal2D();
				if (!ToTarget.IsNearlyZero())
				{
					GetPawn()->SetActorRotation(FRotator(0.f, ToTarget.Rotation().Yaw, 0.f));
				}
				ServerRequestRevive(Clicked);
				UE_LOG(LogEternalReturn, Log, TEXT("[소생] %s 소생 요청"), *GetNameSafe(Clicked));
				return;
			}
			UE_LOG(LogEternalReturn, Log, TEXT("[소생] %s — %s · 다가간다"), *GetNameSafe(Clicked), *Why);
		}
		else if (ClickedASC && GetPawn() && !bIncapacitated && ERTeamStatics::IsHostile(GetPawn(), Clicked) && ClickedASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute()) > 0.f)
		{
			// 적 실험체를 새로 찍으면 혼잣말 (Argument 73 · 본인 · 클라 로컬 — 네트워크 없음). 확률 · 간격은 _Voice.json
			UERPresentationComponent* Pres = Clicked != AttackTarget.Get() && Clicked->IsA<AERCharacterBase>()
				? GetPawn()->FindComponentByClass<UERPresentationComponent>() : nullptr;
			SetAttackTarget(Clicked, TEXT("적 우클릭"));
			if (Pres)
			{
				Pres->PlayVoice(ERTags::Pres_Voice_TargetOn, /*bSelf=*/true);
			}
			return;
		}
	}
	SetAttackTarget(nullptr, TEXT("바닥 클릭"));

	// ⭐ **클라도 투영한다.** 예전에는 원본 클릭 지점으로 바로 움직여서,
	//   상자를 클릭하면 클라와 서버가 서로 다른 곳으로 갔다.
	FVector Destination;
	if (!ResolveNavigableDestination(Hit.ImpactPoint, Destination))
	{
		return;
	}

	// ⭐ 클라가 **먼저 로컬에서** 움직인다. 서버 응답을 기다리면 클릭할 때마다 지연이 보인다.
	//   서버가 목적지를 거부하면 CMC 의 보정이 위치를 되돌린다.
	StartMoveTo(Destination);

	// 판 시작 뒤 첫 이동 (Argument 73 · 본인 · 클라 로컬). 판 시작 = 첫 낮 단계가 복제된 뒤
	const AERGameState* GS = GetWorld() ? GetWorld()->GetGameState<AERGameState>() : nullptr;
	if (!bSaidFirstMove && GS && GS->GetPhaseIndex() >= 0)
	{
		bSaidFirstMove = true;
		if (UERPresentationComponent* Pres = GetPawn() ? GetPawn()->FindComponentByClass<UERPresentationComponent>() : nullptr)
		{
			Pres->PlayVoice(ERTags::Pres_Voice_FirstMove, /*bSelf=*/true);
		}
	}

	// 움직이면 액션 모션을 끊는다 (§5.1 "후딜 중 이동 = 애니메이션 캔슬"). 소유 클라가 로컬로 재생한 몫이라 여기서 — 서버 몫은 ServerSetDestination.
	//   ⚠ 없으면 평타 모션이 끝까지 돌며 미끄러진다 (2026-09-27 사용자 "움직이면서 때린다").
	if (UERPresentationComponent* Pres = GetPawn() ? GetPawn()->FindComponentByClass<UERPresentationComponent>() : nullptr)
	{
		Pres->StopActionAnim();
	}

	// ⚠ **원본 클릭 지점**을 보낸다. 서버는 클라의 계산을 믿지 않고 **직접 다시** 푼다.
	//   같은 함수를 쓰므로 결과는 같다.
	ServerSetDestination(Hit.ImpactPoint);
}

bool AERPlayerController::IsMovementBlockedByCC() const
{
	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return false;
	}

	const UAbilitySystemComponent* ASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ControlledPawn);

	// ⚠ ASC 가 없는 순간이 정상적으로 존재한다 (PlayerState 복제 전).
	//   그때는 막지 않는다 — 스폰 직후 조작이 먹지 않게 된다.
	return ASC && ASC->HasMatchingGameplayTag(ERTags::State_Block_Movement);
}

bool AERPlayerController::ServerSetDestination_Validate(const FVector& Destination)
{
	// ⭐ 여기서 거부하면 **연결이 끊긴다.** 명백히 조작된 값만 잡는다.
	//   정상 범위를 벗어난 정도는 _Implementation 에서 조용히 무시한다.
	return !Destination.ContainsNaN();
}

void AERPlayerController::ServerSetDestination_Implementation(const FVector& Destination)
{
	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	// ⭐ 검증 ① 이동할 수 있는 상태인가 (CC · 이동 차단).
	//
	//   ⚠ **이게 방어의 전부는 아니다.** 클라가 이 RPC 를 아예 안 보내고 움직여도
	//     서버 CMC 의 GetMaxSpeed() 가 0 을 반환해서 막힌다(F06). 그쪽이 실제 방어선이다.
	//     여기서 거르는 이유는 **못 갈 목적지를 서버 상태에 남기지 않기 위해서**다.
	//
	//   ⚠ 사망 검증은 아직 못 한다 — 사망 태그가 F11 에서 생긴다. 그때 여기에 추가한다.
	if (IsMovementBlockedByCC())
	{
		UE_LOG(LogEternalReturn, Verbose,
			TEXT("[이동] CC 로 막혀 있어 목적지를 받지 않는다. %s"), *GetNameSafe(ControlledPawn));
		return;
	}

	// 아군 소생 채널은 움직이면 끊긴다 (Argument 75 R1 · 사용자 2026-10-08)
	if (UAbilitySystemComponent* MoveASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ControlledPawn))
	{
		const FGameplayTagContainer ReviveTag(ERTags::Ability_Revive);
		MoveASC->CancelAbilities(&ReviveTag);
	}

	// ⭐ 검증 ② 거리. 화면 밖을 찍는 텔레포트 시도를 막는다.
	const float DistSq = FVector::DistSquared(ControlledPawn->GetActorLocation(), Destination);
	if (DistSq > FMath::Square(MaxClickDistance))
	{
		UE_LOG(LogEternalReturn, Warning,
			TEXT("[이동] 목적지가 너무 멀다 (%.0fcm > %.0fcm). 무시한다. %s"),
			FMath::Sqrt(DistSq), MaxClickDistance, *GetNameSafe(ControlledPawn));
		return;
	}

	// ⭐ 검증 ③ 갈 수 있는 곳인가. **클라와 같은 함수**로 다시 푼다 -
	//   클라가 보낸 좌표를 믿지 않는다.
	FVector Resolved;
	if (!ResolveNavigableDestination(Destination, Resolved))
	{
		// 갈 수 없는 곳이다. 크래시가 아니라 무시가 맞다.
		return;
	}

	// ⭐ 이동이 **수락됐다** 는 것을 어빌리티에 알린다 (F07-04).
	//   이동 취소형 스킬의 선딜을 끊고, 후딜을 일찍 끝낸다. 듣는 어빌리티가 없으면 아무 일도 없다.
	//   PC 는 어빌리티 내부를 모른다 — 이벤트 태그 하나가 경계다 (Docs/4_Argument/17).
	//   ⚠ 검증 ①(CC · Block.Movement)을 통과한 뒤라야 한다 — 이동 차단형 선딜은 여기까지 못 온다.
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetPawn(), ERTags::Event_Input_Move, FGameplayEventData());

	// 서버 몫의 모션도 끊는다 → RepAnimMontageInfo 로 다른 클라에서도 멈춘다 (소유 클라는 OnMoveToCursor 에서 이미).
	if (UERPresentationComponent* Pres = GetPawn()->FindComponentByClass<UERPresentationComponent>())
	{
		Pres->StopActionAnim();
	}

	// ── 서버가 무엇을 하고 무엇을 하지 않는가 ──────────────
	//
	// ⚠ **서버는 이동을 구동하지 않는다.** 서버가 독립적으로 PathFollowing 을 돌리면
	//   클라가 ServerMove 로 보낸 입력의 재생과 서로를 덮어쓴다 (E07).
	//
	// ⭐ **정확한 표현:**
	//     서버는 경로를 알지도, 신뢰하지도 않는다.
	//     서버는 CMC 가 전달한 movement input(Acceleration)을 authoritative 하게 처리한다.
	//     클라의 NavPath 는 그 입력을 만들어내는 로컬 로직일 뿐이다.
	//
	// ⚠⚠ 그래서 **텔레포트를 막는 것은 이 RPC 가 아니다.**
	//   CMC 의 ServerMove 위치 검증과 GetMaxSpeed() 가 막는다.
	//   이 RPC 는 "못 갈 목적지를 애초에 받지 않는" 필터이고, 그 이상을 주장하지 않는다.
	//
	// 근거: Docs/4_Argument/11_클릭이동_서버검증_수준.md (방안 B)
	ServerDestination = Resolved;

	UE_LOG(LogEternalReturn, Verbose, TEXT("[이동] 목적지 승인 %s"), *ServerDestination.ToString());
}

void AERPlayerController::StartMoveTo(const FVector& Destination)
{
	// ⭐ 엔진이 주는 것을 쓴다. 자체 경로 추적을 만들지 않는다 (CLAUDE.md §1).
	//   SimpleMoveToLocation 은 AIController 를 요구하지 않는다 (역기획서 §2.1).
	UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, Destination);
}

void AERPlayerController::SetAttackTarget(AActor* Target, const TCHAR* Why)
{
	if (!Target && !bAutoAttacking)
	{
		return;   // 바닥 클릭마다 로그하지 않는다
	}
	if (Target)
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[자동공격] 대상 %s 설정 (%s)"), *GetNameSafe(Target), Why);
	}
	else
	{
		UE_LOG(LogEternalReturn, Log, TEXT("[자동공격] 해제 — %s (대상 %s)"), Why, *GetNameSafe(AttackTarget.Get()));
		if (bChasing)
		{
			StopMovement();
		}
	}
	AttackTarget = Target;
	bAutoAttacking = Target != nullptr;
	bChasing = false;
	LastChaseTime = -1.f;
	LastAttackRequestTime = -1.f;
}

void AERPlayerController::TickAutoAttack()
{
	APawn* Me = GetPawn();
	AActor* Target = AttackTarget.Get();
	const UAbilitySystemComponent* MyASC = Me ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Me) : nullptr;
	const UAbilitySystemComponent* TargetASC = Target ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target) : nullptr;
	if (!Me || !MyASC || MyASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute()) <= 0.f)
	{
		SetAttackTarget(nullptr, TEXT("내 사망"));
		return;
	}
	if (!Target || !TargetASC || TargetASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute()) <= 0.f || !ERTeamStatics::IsHostile(Me, Target))
	{
		SetAttackTarget(nullptr, TEXT("대상 사망 · 사라짐"));
		return;
	}
	if (UERRideComponent* Ride = Me->FindComponentByClass<UERRideComponent>(); Ride && Ride->IsSteering())
	{
		SetAttackTarget(nullptr, TEXT("탑승"));
		return;
	}

	// 평타를 휘두르는 동안(선딜 · 후딜)은 따라가지 않는다 — 대상이 조금 벗어나자마자 추적하면 이동이 휘두르던 평타를 끊었다 (2026-10-06 로그 16:53:47 "모션 끊음")
	//   평타가 끝나면 다음 틱에 추적 · 다시 평타 (LoL 도 공격 동작은 끝까지 친다)
	{
		const FGameplayTag AttackSlot = ResolveModeSlot(*MyASC, ERTags::Ability_Slot_Attack);
		for (const FGameplayAbilitySpec& Spec : MyASC->GetActivatableAbilities())
		{
			if (Spec.DynamicAbilityTags.HasTagExact(AttackSlot) && Spec.IsActive())
			{
				return;
			}
		}
	}

	// 사거리 — 평타 판정과 같은 거리 함수 · 같은 어트리뷰트 (서버와 "사거리 안" 이 같게)
	const float RangeUU = MyASC->GetNumericAttribute(UERAttributeSet::GetAttackRangeAttribute()) * 100.f;
	const float Dist = ERTargeting::SingleTargetDistance(Me, Me->GetActorLocation(), Target, /*bIgnoreZ=*/true);
	const float Now = GetWorld()->GetTimeSeconds();
	// 경계에서 추적 ↔ 멈춤이 번갈아 나지 않게 — 추적 중엔 사거리보다 25cm 안쪽까지 들어가서 멈춘다 `[자체]` (2026-10-06 로그 "7.0m · 사거리 7.0m" 반복)
	const bool bOutOfRange = bChasing ? Dist > RangeUU - 25.f : Dist > RangeUU;
	if (bOutOfRange)
	{
		// 추적 — 클릭 이동과 같은 길 (클라 먼저 · 서버 검증). 0.25초마다 · 대상이 50cm 넘게 움직였을 때만 다시 길찾기 `[자체]`
		if (IsMovementBlockedByCC())
		{
			return;
		}
		const FVector Goal = Target->GetActorLocation();
		if (!bChasing || (Now - LastChaseTime >= 0.25f && FVector::Dist2D(Goal, LastChaseGoal) > 50.f))
		{
			FVector Destination;
			if (ResolveNavigableDestination(Goal, Destination))
			{
				StartMoveTo(Destination);
				ServerSetDestination(Goal);
				if (!bChasing)
				{
					UE_LOG(LogEternalReturn, Log, TEXT("[자동공격] 사거리 밖 — 추적 %s (%.1fm · 사거리 %.1fm)"), *GetNameSafe(Target), Dist / 100.f, RangeUU / 100.f);
				}
				bChasing = true;
				LastChaseGoal = Goal;
				LastChaseTime = Now;
			}
		}
		return;
	}

	// 사거리 안 — 멈추고 평타 (준비되면). 스킬 중 · CC 중엔 선판정이 막아 기다린다 → 끝나면 다시 친다 (사용자 "재개")
	// 추적이 아니어도 멈춘다 — 바닥 클릭으로 가던 중 사거리 안의 적을 우클릭하면 이전 길을 계속 가며 평타가 나갔다 (2026-10-06 로그 07:50:12 · 사용자 "이동하면서 공격이 나간 거 같다")
	if (bChasing || Me->GetVelocity().SizeSquared2D() > 1.f)
	{
		StopMovement();
		bChasing = false;
	}
	if (Now - LastAttackRequestTime >= 0.2f && RequestAttackOn(Target))
	{
		LastAttackRequestTime = Now;
	}
}

bool AERPlayerController::RequestAttackOn(AActor* Target)
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPawn());
	if (!ASC || !Target)
	{
		return false;
	}
	const FGameplayTag SlotTag = ResolveSlot(*ASC, ERTags::Ability_Slot_Attack);
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.Ability || !Spec.DynamicAbilityTags.HasTagExact(SlotTag))
		{
			continue;
		}
		// 평타 키와 같은 선판정 (인스턴스 · E12) — 실패는 조용히 (쿨 · 스킬 중 · CC — 매 틱 묻는다)
		const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
		const UGameplayAbility* Checker = Instance ? Instance : Spec.Ability.Get();
		if (!Checker->CanActivateAbility(Spec.Handle, ASC->AbilityActorInfo.Get()))
		{
			return false;
		}
		const FHitResult Hit(Target, nullptr, Target->GetActorLocation(), FVector::UpVector);
		ServerActivateSkill(SlotTag, UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(Hit));
		UE_LOG(LogEternalReturn, Verbose, TEXT("[자동공격] 평타 요청 -> %s"), *GetNameSafe(Target));
		return true;
	}
	return false;
}
