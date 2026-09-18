// Copyright Epic Games, Inc. All Rights Reserved.
//
// ⚠⚠ **임시 파일이다. F17(UI) 이 경험치 게이지를 그리면 삭제한다.**
//
//   야생동물(F12)이 없어서 경험치를 줄 방법이 없다. 그때까지 콘솔로 대신한다. ERSkillDebug.cpp 와 같은 성격.

#include "Engine/World.h"
#include "EternalReturn.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Core/ERPlayerState.h"
#include "GAS/ERAttributeSet.h"
#include "Growth/ERGrowthComponent.h"

namespace
{

/** 서버 월드의 모든 플레이어 (ERSkillDebug 와 같은 이유 — 리슨 서버 창의 로컬 폰은 서버 플레이어뿐). */
template <typename TFunc>
void ForEachServerPlayer(UWorld* World, TFunc Func)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[성장디버그] 서버 창에서만 된다."));
		return;
	}
	const AGameStateBase* GS = World->GetGameState();
	if (!GS)
	{
		return;
	}
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (AERPlayerState* ERPS = Cast<AERPlayerState>(PS))
		{
			Func(ERPS);
		}
	}
}

// ER.Growth.AddExp <n> — 전원 경험치 지급 (서버 창)
void GrowthAddExpCmd(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[성장디버그] 사용법: ER.Growth.AddExp <n>"));
		return;
	}
	const int32 Amount = FCString::Atoi(*Args[0]);
	ForEachServerPlayer(World, [Amount](AERPlayerState* PS)
	{
		if (PS->GetGrowth())
		{
			PS->GetGrowth()->AddExp(Amount, EERExpSource::Debug);
		}
	});
}

// ER.Growth.Show — 이 창에서 보이는 모든 플레이어의 레벨 · 경험치 · 성장 어트리뷰트(ER.Item.Stats 에 없는 HP · 재생 · 이동).
//   클라 창에서 남의 Exp 는 0 으로 보여야 한다 (OwnerOnly).
void GrowthShowCmd(const TArray<FString>& Args, UWorld* World)
{
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}
	const TCHAR* Side = World->GetNetMode() == NM_Client ? TEXT("클라") : TEXT("서버");
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const AERPlayerState* ERPS = Cast<AERPlayerState>(PS);
		const UERGrowthComponent* Growth = ERPS ? ERPS->GetGrowth() : nullptr;
		if (!Growth)
		{
			continue;
		}
		const FERLevelExpRow* Row = UERGrowthComponent::FindLevelRow(Growth->GetLevel());
		FString Stats;
		if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ERPS))
		{
			Stats = FString::Printf(TEXT("  HP %.1f / %.1f  재생 %.3f  이동 %.2f"),
				ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute()), ASC->GetNumericAttribute(UERAttributeSet::GetMaxHPAttribute()),
				ASC->GetNumericAttribute(UERAttributeSet::GetHPRegenAttribute()), ASC->GetNumericAttribute(UERAttributeSet::GetMoveSpeedAttribute()));
		}
		UE_LOG(LogEternalReturn, Warning, TEXT("[성장디버그][%s] %s Lv.%d  경험치 %d / %d  (최대 Lv.%d)%s"),
			Side, *ERPS->GetName(), Growth->GetLevel(), Growth->GetExp(), Row ? Row->RequiredExp : 0, UERGrowthComponent::GetMaxLevel(), *Stats);
	}
}

// ER.Growth.ProfExp <n> — 전원의 **장착 무기군**에 숙련도 경험치 (서버 창). F12 없이 레벨 5 를 만들기 위해.
void GrowthProfExpCmd(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[성장디버그] 사용법: ER.Growth.ProfExp <n>"));
		return;
	}
	const float Amount = FCString::Atof(*Args[0]);
	ForEachServerPlayer(World, [Amount](AERPlayerState* PS)
	{
		if (PS->GetGrowth())
		{
			PS->GetGrowth()->AddEquippedWeaponProficiencyExp(Amount, TEXT("Debug"));
		}
	});
}

// ER.Growth.Prof — 이 창에서 보이는 모든 플레이어의 무기군별 숙련도. 클라 창에서 남의 것은 비어 있어야 한다 (OwnerOnly).
void GrowthProfCmd(const TArray<FString>& Args, UWorld* World)
{
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}
	const TCHAR* Side = World->GetNetMode() == NM_Client ? TEXT("클라") : TEXT("서버");
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const AERPlayerState* ERPS = Cast<AERPlayerState>(PS);
		const UERGrowthComponent* Growth = ERPS ? ERPS->GetGrowth() : nullptr;
		if (!Growth)
		{
			continue;
		}
		FString Line;
		for (const FERWeaponProficiency& P : Growth->GetWeaponProficiencies())
		{
			Line += FString::Printf(TEXT("  %s Lv.%d (%.1f / %.0f)"), *UEnum::GetDisplayValueAsText(P.WeaponType).ToString(),
				P.Level, P.Exp, UERGrowthComponent::ProficiencyRequiredExp(P.Level));
		}
		if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ERPS))
		{
			Line += FString::Printf(TEXT("  | 스킬증폭 %.3f  기본공격증폭 %.3f"),
				ASC->GetNumericAttribute(UERAttributeSet::GetSkillAmpAttribute()), ASC->GetNumericAttribute(UERAttributeSet::GetBasicAtkAmpAttribute()));
		}
		UE_LOG(LogEternalReturn, Warning, TEXT("[성장디버그][%s] %s 숙련도:%s"), Side, *ERPS->GetName(), Line.IsEmpty() ? TEXT(" (없음)") : *Line);
	}
}

} // namespace

static FAutoConsoleCommandWithWorldAndArgs GERGrowthProfExpCmd(
	TEXT("ER.Growth.ProfExp"), TEXT("[임시] 전원 장착 무기군 숙련도 경험치 (서버 창). ER.Growth.ProfExp <n>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GrowthProfExpCmd));
static FAutoConsoleCommandWithWorldAndArgs GERGrowthProfCmd(
	TEXT("ER.Growth.Prof"), TEXT("[임시] 모든 플레이어의 무기군별 숙련도"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GrowthProfCmd));
static FAutoConsoleCommandWithWorldAndArgs GERGrowthAddExpCmd(
	TEXT("ER.Growth.AddExp"), TEXT("[임시] 전원 경험치 지급 (서버 창). ER.Growth.AddExp <n>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GrowthAddExpCmd));
static FAutoConsoleCommandWithWorldAndArgs GERGrowthShowCmd(
	TEXT("ER.Growth.Show"), TEXT("[임시] 모든 플레이어의 레벨 · 경험치"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GrowthShowCmd));
