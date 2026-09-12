// Copyright Epic Games, Inc. All Rights Reserved.
//
// ⚠⚠ **임시 파일이다. F07(스킬)이 끝나면 통째로 삭제한다.**
//
//   스킬이 없어서 CC 를 걸 수단이 없다. 그때까지 콘솔로 대신한다.
//   ERDamageDebug.cpp 와 같은 성격이다.

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EternalReturn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "GAS/ERCCLibrary.h"
#include "Character/ERCharacterBase.h"
#include "Combat/ERForcedMove.h"
#include "GameFramework/Character.h"
#include "GAS/ERAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/ERGameplayTags.h"
#include "GameplayTagContainer.h"

namespace
{

/** 조종 중인 폰의 ASC. 없으면 이유를 로그로 남긴다. */
UAbilitySystemComponent* GetLocalASC(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	const APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->GetPawn())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[CC디버그] 조종 중인 폰이 없다."));
		return nullptr;
	}

	UAbilitySystemComponent* ASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PC->GetPawn());

	if (!ASC)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[CC디버그] 폰에서 ASC 를 찾지 못했다."));
	}
	return ASC;
}

/** "Movement" / "BasicAttack" / "Skill" -> 축 태그 */
bool ParseBlockAxis(const FString& Name, FGameplayTag& OutTag)
{
	if (Name.Equals(TEXT("Movement"), ESearchCase::IgnoreCase))    { OutTag = ERTags::State_Block_Movement;    return true; }
	if (Name.Equals(TEXT("BasicAttack"), ESearchCase::IgnoreCase)) { OutTag = ERTags::State_Block_BasicAttack; return true; }
	if (Name.Equals(TEXT("Skill"), ESearchCase::IgnoreCase))       { OutTag = ERTags::State_Block_Skill;       return true; }
	return false;
}

// ─────────────────────────────────────────────────────────────
// ER.CC.Block <축> <초>
//
// ⚠⚠ **Loose 태그는 복제되지 않는다.** 부른 쪽에만 붙는다.
//   그래서 이 커맨드는 **Standalone / 1인 PIE 전용**이다.
//   (서버와 클라가 한 프로세스라 구분이 없다)
//
//   2인 PIE 에서 "서버가 걸면 클라가 멈춘다"를 확인하려면 진짜 GE 가 필요하다
//   -> CC GE 애셋을 만든 뒤 ER.CC.Apply 를 쓴다.
//
// ⚠ 지속시간을 FTimerManager 로 재는 것은 **디버그 편의일 뿐**이다.
//   진짜 CC 의 지속시간은 GE 의 Duration 이 관리한다 (CLAUDE.md §8).
// ─────────────────────────────────────────────────────────────
void CCBlock(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogEternalReturn, Warning,
			TEXT("[CC디버그] 사용법: ER.CC.Block <Movement|BasicAttack|Skill> [초=2.0]"));
		return;
	}

	FGameplayTag AxisTag;
	if (!ParseBlockAxis(Args[0], AxisTag))
	{
		UE_LOG(LogEternalReturn, Warning,
			TEXT("[CC디버그] 축 이름이 틀렸다: %s (Movement / BasicAttack / Skill)"), *Args[0]);
		return;
	}

	UAbilitySystemComponent* ASC = GetLocalASC(World);
	if (!ASC)
	{
		return;
	}

	const float Duration = (Args.Num() >= 2) ? FCString::Atof(*Args[1]) : 2.0f;

	ASC->SetLooseGameplayTagCount(AxisTag, 1);
	UE_LOG(LogEternalReturn, Warning, TEXT("[CC디버그] %s 를 %.2f초 붙였다. (⚠ 복제 안 됨)"),
		*AxisTag.ToString(), Duration);

	// 타이머가 World 수명을 넘지 않게 약한 참조로 잡는다.
	TWeakObjectPtr<UAbilitySystemComponent> WeakASC(ASC);
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle,
		[WeakASC, AxisTag]()
		{
			if (WeakASC.IsValid())
			{
				WeakASC->SetLooseGameplayTagCount(AxisTag, 0);
				UE_LOG(LogEternalReturn, Warning, TEXT("[CC디버그] %s 해제."), *AxisTag.ToString());
			}
		},
		Duration, /*bLoop=*/false);
}

// ─────────────────────────────────────────────────────────────
// ER.CC.Apply <GE 애셋 경로> <초>
//
// ⭐ 진짜 GE 경로를 탄다 — 복제되고, 만료되고, **ERCC::ApplyCC 의 검사를 전부 거친다.**
//   CC GE 애셋을 만든 뒤 이걸로 검증한다.
//
// 예) ER.CC.Apply /Game/ER/GAS/CC/GE_CC_Stun.GE_CC_Stun_C 1.0
// ─────────────────────────────────────────────────────────────
void CCApply(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogEternalReturn, Warning,
			TEXT("[CC디버그] 사용법: ER.CC.Apply <GE 애셋 경로> [초=1.0]"));
		return;
	}

	UAbilitySystemComponent* ASC = GetLocalASC(World);
	if (!ASC)
	{
		return;
	}

	// ⚠ 경로 끝의 _C 를 빠뜨리면 못 찾는다. 블루프린트 GE 의 클래스는 _C 로 끝난다.
	UClass* LoadedClass = StaticLoadClass(UGameplayEffect::StaticClass(), nullptr, *Args[0]);
	if (!LoadedClass)
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[CC디버그] GE 클래스를 못 찾았다: %s  (경로가 _C 로 끝나는지 확인한다)"), *Args[0]);
		return;
	}

	const float Duration = (Args.Num() >= 2) ? FCString::Atof(*Args[1]) : 1.0f;

	// ⭐ 자기 자신에게 건다. 실패해도 ApplyCC 가 이유를 로그로 남긴다.
	const bool bApplied = ERCC::ApplyCC(ASC, ASC, LoadedClass, Duration);

	UE_LOG(LogEternalReturn, Warning, TEXT("[CC디버그] %s -> %s"),
		*LoadedClass->GetName(), bApplied ? TEXT("적용됨") : TEXT("실패 (위 로그 참조)"));
}

// ─────────────────────────────────────────────────────────────
// ER.CC.Slow <감소율%> <초>
//
// ⭐ 둔화 전용. ER.CC.Apply 와 달리 SetByCaller.SlowPercent 까지 넣는다.
//   둔화 GE 는 지속시간(CCDuration)과 감소율(SlowPercent) **둘 다** 필요하다.
//
// 예) ER.CC.Slow 60 5   -> 60% 둔화 5초
//
// ⚠ GE 애셋 경로는 고정으로 둔다. 둔화 애셋은 하나뿐이라(GE_CC_Slow)
//   매번 경로를 타이핑하는 것이 검증을 방해한다.
// ─────────────────────────────────────────────────────────────
void CCSlow(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogEternalReturn, Warning,
			TEXT("[CC디버그] 사용법: ER.CC.Slow <감소율%%> [초=5]   예) ER.CC.Slow 60 5"));
		return;
	}

	UAbilitySystemComponent* ASC = GetLocalASC(World);
	if (!ASC)
	{
		return;
	}

	if (!ASC->IsOwnerActorAuthoritative())
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[CC디버그] 서버가 아니다. Play As Listen Server 의 서버 창에서 쓴다."));
		return;
	}

	static const TCHAR* SlowEffectPath = TEXT("/Game/GAS/CC/GE_CC_Slow.GE_CC_Slow_C");
	UClass* SlowClass = StaticLoadClass(UGameplayEffect::StaticClass(), nullptr, SlowEffectPath);
	if (!SlowClass)
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[CC디버그] 둔화 GE 를 못 찾았다: %s  (에디터 작업 ⑤절로 먼저 만든다)"),
			SlowEffectPath);
		return;
	}

	const float SlowPercent = FCString::Atof(*Args[0]) / 100.f;
	const float Duration    = (Args.Num() >= 2) ? FCString::Atof(*Args[1]) : 5.0f;

	// ⚠ ApplyCC 를 쓰지 않는다 — SlowPercent 를 추가로 넣어야 하기 때문이다.
	//   대신 ApplyCC 가 하는 검사 중 중요한 것(지속시간 정책)은 여기서도 본다.
	const UGameplayEffect* Def = SlowClass->GetDefaultObject<UGameplayEffect>();
	if (Def && Def->DurationPolicy != EGameplayEffectDurationType::HasDuration)
	{
		UE_LOG(LogEternalReturn, Error,
			TEXT("[CC디버그] GE_CC_Slow 의 Duration Policy 가 HasDuration 이 아니다."));
		return;
	}

	const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(SlowClass, 1.f, Context);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[CC디버그] 둔화 스펙을 만들지 못했다."));
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_CCDuration, Duration);
	SpecHandle.Data->SetSetByCallerMagnitude(ERTags::SetByCaller_SlowPercent, SlowPercent);

	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);

	UE_LOG(LogEternalReturn, Warning, TEXT("[CC디버그] 둔화 %.0f%% %.1f초 적용. MoveSpeed=%.2f"),
		SlowPercent * 100.f, Duration,
		ASC->GetNumericAttribute(UERAttributeSet::GetMoveSpeedAttribute()));
}

// ─────────────────────────────────────────────────────────────
// ER.CC.Speed — 현재 이동속도를 찍는다 (둔화 확인용)
// ─────────────────────────────────────────────────────────────
void CCSpeed(const TArray<FString>& Args, UWorld* World)
{
	UAbilitySystemComponent* ASC = GetLocalASC(World);
	if (!ASC)
	{
		return;
	}

	const float MoveSpeed = ASC->GetNumericAttribute(UERAttributeSet::GetMoveSpeedAttribute());

	// ⭐ 어트리뷰트(m/s)와 CMC(cm/s)를 함께 찍는다. 둘이 안 맞으면 반영 지점 문제다.
	float CMCSpeed = -1.f;
	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		if (const APawn* P = PC->GetPawn())
		{
			if (const UCharacterMovementComponent* CMC =
					Cast<UCharacterMovementComponent>(P->GetMovementComponent()))
			{
				CMCSpeed = CMC->GetMaxSpeed();
			}
		}
	}

	// ⚠ GetActiveEffectsWithAllTags 는 애셋 태그만 본다 (E09). 부여 태그를 보려면 이쪽이다.
	FGameplayTagContainer SlowQuery;
	SlowQuery.AddTag(ERTags::State_CC_Slow);
	const int32 SlowCount =
		ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(SlowQuery)).Num();

	UE_LOG(LogEternalReturn, Warning,
		TEXT("[CC디버그] MoveSpeed=%.2f m/s | CMC MaxWalkSpeed=%.1f cm/s | 걸린 둔화 %d개"),
		MoveSpeed, CMCSpeed, SlowCount);
}

// ─────────────────────────────────────────────────────────────
// ER.CC.Knockback <거리m> [초] [각도]
//
// ⭐ 조종 중인 캐릭터를 밀어낸다. 방향은 **카메라가 보는 방향** 기준이고,
//   각도(도)를 주면 그만큼 돌린다 — 그랩(끌어당김)은 180 을 준다.
//
// 예) ER.CC.Knockback 3 0.4        -> 앞으로 3m, 0.4초
//     ER.CC.Knockback 3 0.4 180    -> 뒤로 (그랩과 같은 방향)
//
// ⚠ 서버에서만 동작한다. Play As Listen Server 의 서버 창에서 쓴다.
// ─────────────────────────────────────────────────────────────
void CCKnockback(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogEternalReturn, Warning,
			TEXT("[CC디버그] 사용법: ER.CC.Knockback <거리m> [초=0.4] [각도=0]"));
		return;
	}

	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	ACharacter* Character = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!Character)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[CC디버그] 조종 중인 캐릭터가 없다."));
		return;
	}

	const float DistanceM = FCString::Atof(*Args[0]);
	const float Duration  = (Args.Num() >= 2) ? FCString::Atof(*Args[1]) : 0.4f;
	const float AngleDeg  = (Args.Num() >= 3) ? FCString::Atof(*Args[2]) : 0.f;

	// ⚠ 역기획서는 m 로 적혀 있고 언리얼은 cm 다. 여기서 환산한다.
	constexpr float MetersToUU = 100.f;

	// ⭐ 캐릭터가 보는 방향이 아니라 **카메라 기준**이다. 탑다운이라 캐릭터는
	//   아무 데나 보고 있을 수 있어서, 눈으로 확인하려면 화면 기준이 맞다.
	FVector Direction = FRotator(0.f, PC->GetControlRotation().Yaw, 0.f).Vector();
	if (!FMath::IsNearlyZero(AngleDeg))
	{
		Direction = Direction.RotateAngleAxis(AngleDeg, FVector::UpVector);
	}

	const FVector Before = Character->GetActorLocation();

	// ⭐ 벽 충돌이 실제로 걸리는지 보려고 구독한다. ⚠ [임시] — F07 에서는 스킬이 구독한다.
	//   람다가 캐릭터 수명을 넘지 않게 약한 참조로 잡는다.
	if (AERCharacterBase* ERChar = Cast<AERCharacterBase>(Character))
	{
		// ⚠ 중복 구독을 막는다. 넉백을 여러 번 치면 로그가 쌓인다.
		ERChar->OnForcedMoveWallImpact.RemoveAll(ERChar);
		ERChar->OnForcedMoveWallImpact.AddWeakLambda(ERChar,
			[](AERCharacterBase* Hit, const FHitResult& HitResult)
			{
				UE_LOG(LogEternalReturn, Warning,
					TEXT("[CC디버그] ⭐ 벽 충돌! 대상=%s  위치=%s"),
					*GetNameSafe(HitResult.GetActor()), *HitResult.ImpactPoint.ToCompactString());
			});
	}

	if (!ERForcedMove::ApplyForcedMove(Character, Direction, DistanceM * MetersToUU, Duration))
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[CC디버그] 넉백 실패 (위 로그 참조)"));
		return;
	}

	UE_LOG(LogEternalReturn, Warning, TEXT("[CC디버그] 넉백 %.1fm %.2f초 시작. 시작위치 %s"),
		DistanceM, Duration, *Before.ToCompactString());

	// ⭐ 끝난 뒤 **실제 이동 거리**를 찍는다. "정확히 3m" 가 지켜지는지 보는 것이 핵심이다.
	TWeakObjectPtr<ACharacter> WeakChar(Character);
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle,
		[WeakChar, Before, DistanceM]()
		{
			if (!WeakChar.IsValid())
			{
				return;
			}
			const float MovedM = FVector::Dist2D(WeakChar->GetActorLocation(), Before) / 100.f;
			UE_LOG(LogEternalReturn, Warning,
				TEXT("[CC디버그] 넉백 종료 — 실제 이동 %.2fm (목표 %.2fm)%s"),
				MovedM, DistanceM,
				(MovedM < DistanceM * 0.9f) ? TEXT("  ⚠ 벽에 막혔거나 덜 이동") : TEXT(""));
		},
		Duration + 0.1f, /*bLoop=*/false);
}

// ─────────────────────────────────────────────────────────────
// ER.CC.WallCheck — 지금 벽에 닿아 있는지 (판정 함수 확인용)
// ─────────────────────────────────────────────────────────────
void CCWallCheck(const TArray<FString>& Args, UWorld* World)
{
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	ACharacter* Character = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!Character)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[CC디버그] 조종 중인 캐릭터가 없다."));
		return;
	}

	FHitResult Hit;
	const bool bWall = ERForcedMove::CheckWallImpact(Character, Hit);

	UE_LOG(LogEternalReturn, Warning, TEXT("[CC디버그] 벽 판정=%s  강제이동중=%s%s"),
		bWall ? TEXT("O") : TEXT("X"),
		ERForcedMove::IsForcedMoving(Character) ? TEXT("O") : TEXT("X"),
		bWall ? *FString::Printf(TEXT("  대상=%s"), *GetNameSafe(Hit.GetActor())) : TEXT(""));
}

// ─────────────────────────────────────────────────────────────
// ER.CC.Resist <CCResist%> <SlowResist%>
//
// ⭐ 두 저항을 **직접 세팅**한다. F06-05 검증용.
//   ⚠ 어트리뷰트 Base 를 바꾼다 — 아이템·특성 경로를 흉내내는 것이지
//     실제 게임 경로가 아니다. F08(아이템) 이 생기면 그쪽으로 검증한다.
//
// 예) ER.CC.Resist 40 20   -> 방해 저항 40%, 둔화 저항 20%
// ─────────────────────────────────────────────────────────────
void CCResistCmd(const TArray<FString>& Args, UWorld* World)
{
	UAbilitySystemComponent* ASC = GetLocalASC(World);
	if (!ASC)
	{
		return;
	}

	if (Args.Num() >= 1)
	{
		ASC->SetNumericAttributeBase(
			UERAttributeSet::GetCCResistAttribute(), FCString::Atof(*Args[0]) / 100.f);
	}
	if (Args.Num() >= 2)
	{
		ASC->SetNumericAttributeBase(
			UERAttributeSet::GetSlowResistAttribute(), FCString::Atof(*Args[1]) / 100.f);
	}

	// ⚠ 상한(0.8)은 F02-03 의 PreAttributeChange 가 자른다. 넣은 값과 다를 수 있고
	//   그게 정상이다 — 그래서 **설정한 값이 아니라 읽은 값**을 찍는다.
	UE_LOG(LogEternalReturn, Warning,
		TEXT("[CC디버그] 방해 저항=%.0f%%  둔화 저항=%.0f%%  (⚠ 상한 0.8 이 적용된 뒤 값)"),
		ASC->GetNumericAttribute(UERAttributeSet::GetCCResistAttribute()) * 100.f,
		ASC->GetNumericAttribute(UERAttributeSet::GetSlowResistAttribute()) * 100.f);
}

// ─────────────────────────────────────────────────────────────
// ER.CC.Immune <초> — 이동 방해 면역 (매그너스 R 흉내)
//
// ⚠ Loose 태그라 복제되지 않는다. 1인 PIE / 리슨 서버 창에서만 의미가 있다.
// ─────────────────────────────────────────────────────────────
void CCImmune(const TArray<FString>& Args, UWorld* World)
{
	UAbilitySystemComponent* ASC = GetLocalASC(World);
	if (!ASC)
	{
		return;
	}

	const float Duration = (Args.Num() >= 1) ? FCString::Atof(*Args[0]) : 7.0f;

	// ⭐ 종류를 고를 수 있다. 기본은 이동 방해 면역(매그너스 R).
	//   damage -> 피해 면역(시셀라 W). ⚠ 피해 면역은 **CC 를 막지 않는다.**
	FGameplayTag ImmuneTag = ERTags::State_CCImmune;
	const TCHAR* Label = TEXT("이동 방해 면역");
	if (Args.Num() >= 2 && Args[1].Equals(TEXT("damage"), ESearchCase::IgnoreCase))
	{
		ImmuneTag = ERTags::State_DamageImmune;
		Label = TEXT("피해 면역 (⚠ CC 는 그대로 걸린다)");
	}

	ASC->SetLooseGameplayTagCount(ImmuneTag, 1);
	UE_LOG(LogEternalReturn, Warning, TEXT("[CC디버그] %s %.1f초."), Label, Duration);

	TWeakObjectPtr<UAbilitySystemComponent> WeakASC(ASC);
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle,
		[WeakASC, ImmuneTag]()
		{
			if (WeakASC.IsValid())
			{
				WeakASC->SetLooseGameplayTagCount(ImmuneTag, 0);
				UE_LOG(LogEternalReturn, Warning, TEXT("[CC디버그] 면역 해제."));
			}
		},
		Duration, /*bLoop=*/false);
}

// ─────────────────────────────────────────────────────────────
// ER.CC.List — 지금 붙어 있는 CC · 차단 축 태그
// ─────────────────────────────────────────────────────────────
void CCList(const TArray<FString>& Args, UWorld* World)
{
	UAbilitySystemComponent* ASC = GetLocalASC(World);
	if (!ASC)
	{
		return;
	}

	FGameplayTagContainer Owned;
	ASC->GetOwnedGameplayTags(Owned);

	UE_LOG(LogEternalReturn, Warning, TEXT("[CC디버그] ── 보유 태그 %d개 ──"), Owned.Num());
	for (const FGameplayTag& Tag : Owned)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("   %s"), *Tag.ToString());
	}

	UE_LOG(LogEternalReturn, Warning, TEXT("[CC디버그] 이동차단=%s  평타차단=%s  스킬차단=%s"),
		ASC->HasMatchingGameplayTag(ERTags::State_Block_Movement)    ? TEXT("O") : TEXT("X"),
		ASC->HasMatchingGameplayTag(ERTags::State_Block_BasicAttack) ? TEXT("O") : TEXT("X"),
		ASC->HasMatchingGameplayTag(ERTags::State_Block_Skill)       ? TEXT("O") : TEXT("X"));
}

// ─────────────────────────────────────────────────────────────
// ER.CC.Clear — 축 태그를 전부 뗀다 (Loose 태그만. GE 는 안 건드린다)
// ─────────────────────────────────────────────────────────────
void CCClear(const TArray<FString>& Args, UWorld* World)
{
	UAbilitySystemComponent* ASC = GetLocalASC(World);
	if (!ASC)
	{
		return;
	}

	ASC->SetLooseGameplayTagCount(ERTags::State_Block_Movement, 0);
	ASC->SetLooseGameplayTagCount(ERTags::State_Block_BasicAttack, 0);
	ASC->SetLooseGameplayTagCount(ERTags::State_Block_Skill, 0);

	// ⚠ GE 로 걸린 CC 는 안 풀린다. 그건 GE 를 제거해야 한다.
	UE_LOG(LogEternalReturn, Warning,
		TEXT("[CC디버그] Loose 축 태그를 뗐다. (⚠ GE 로 걸린 CC 는 그대로다)"));
}

} // namespace

static FAutoConsoleCommandWithWorldAndArgs GERCCBlockCmd(
	TEXT("ER.CC.Block"),
	TEXT("[임시] 차단 축 태그를 직접 붙인다. ER.CC.Block <Movement|BasicAttack|Skill> [초]  ⚠ 복제 안 됨"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CCBlock));

static FAutoConsoleCommandWithWorldAndArgs GERCCApplyCmd(
	TEXT("ER.CC.Apply"),
	TEXT("[임시] CC GE 애셋을 자신에게 적용한다. ER.CC.Apply <GE 경로(_C)> [초]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CCApply));

static FAutoConsoleCommandWithWorldAndArgs GERCCSlowCmd(
	TEXT("ER.CC.Slow"),
	TEXT("[임시] 둔화를 적용한다. ER.CC.Slow <감소율%> [초]   예) ER.CC.Slow 60 5"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CCSlow));

static FAutoConsoleCommandWithWorldAndArgs GERCCSpeedCmd(
	TEXT("ER.CC.Speed"),
	TEXT("[임시] 현재 이동속도와 걸린 둔화 개수를 찍는다."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CCSpeed));

static FAutoConsoleCommandWithWorldAndArgs GERCCKnockbackCmd(
	TEXT("ER.CC.Knockback"),
	TEXT("[임시] 넉백. ER.CC.Knockback <거리m> [초] [각도]   각도 180 = 그랩 방향"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CCKnockback));

static FAutoConsoleCommandWithWorldAndArgs GERCCWallCheckCmd(
	TEXT("ER.CC.WallCheck"),
	TEXT("[임시] 지금 벽에 닿아 있는지 판정해 본다."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CCWallCheck));

static FAutoConsoleCommandWithWorldAndArgs GERCCResistCmd(
	TEXT("ER.CC.Resist"),
	TEXT("[임시] 저항 설정. ER.CC.Resist <방해저항%> <둔화저항%>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CCResistCmd));

static FAutoConsoleCommandWithWorldAndArgs GERCCImmuneCmd(
	TEXT("ER.CC.Immune"),
	TEXT("[임시] 면역. ER.CC.Immune [초=7] [damage]   damage 를 주면 피해 면역"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CCImmune));

static FAutoConsoleCommandWithWorldAndArgs GERCCListCmd(
	TEXT("ER.CC.List"),
	TEXT("[임시] 보유 태그와 차단 축 상태를 찍는다."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CCList));

static FAutoConsoleCommandWithWorldAndArgs GERCCClearCmd(
	TEXT("ER.CC.Clear"),
	TEXT("[임시] Loose 축 태그를 전부 뗀다."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CCClear));
