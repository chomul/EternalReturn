// Copyright Epic Games, Inc. All Rights Reserved.
//
// ⚠⚠ **임시 파일이다. F07(스킬)이 끝나면 통째로 삭제한다.**
//
//   UI 가 없어서 쿨다운을 눈으로 볼 수 없다. 그때까지 콘솔로 대신한다.
//   ERCCDebug.cpp / ERDamageDebug.cpp 와 같은 성격이다.

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "EternalReturn.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Core/ERPlayerState.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/ERSkillData.h"

namespace
{

UAbilitySystemComponent* GetLocalSkillASC(UWorld* World)
{
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->GetPawn())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[스킬디버그] 조종 중인 폰이 없다."));
		return nullptr;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PC->GetPawn());
	if (!ASC)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[스킬디버그] 폰에서 ASC 를 찾지 못했다."));
	}
	return ASC;
}

// ─────────────────────────────────────────────────────────────
// ER.Skill.Cooldown — 부여된 스킬 전부의 쿨다운 남은/전체 시간
//
// ⭐ "만료 서버시각" 을 같이 찍는다. 서버·클라 창에서 각각 쳐서 그 값을 비교하면
//   누른 시점이 달라도 ±0.1초 검증이 된다 (남은시간은 누른 순간마다 다르니까).
// ─────────────────────────────────────────────────────────────
void SkillCooldownCmd(const TArray<FString>& Args, UWorld* World)
{
	UAbilitySystemComponent* ASC = GetLocalSkillASC(World);
	if (!ASC)
	{
		return;
	}

	const AGameStateBase* GS = World->GetGameState();
	const float ServerNow = GS ? GS->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
	const TCHAR* Side = World->GetNetMode() == NM_Client ? TEXT("클라") : TEXT("서버");

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		const UERGameplayAbility* Ability = Cast<UERGameplayAbility>(Spec.GetPrimaryInstance());
		if (!Ability)
		{
			continue;
		}

		float Remaining = 0.f;
		float Duration = 0.f;
		Ability->GetCooldownTimeRemainingAndDuration(Spec.Handle, ASC->AbilityActorInfo.Get(), Remaining, Duration);

		UE_LOG(LogEternalReturn, Warning, TEXT("[스킬디버그][%s] %s %s : 남은 %.2f / 전체 %.2f초  만료서버시각=%.2f  (서버시각추정=%.2f 로컬시각=%.2f)"),
			Side, *GetNameSafe(ASC->GetOwnerActor()), *Ability->GetSlotTag().ToString(), Remaining, Duration,
			Remaining > 0.f ? ServerNow + Remaining : 0.f, ServerNow, World->GetTimeSeconds());
	}
}

/** "Q" / "W" / "E" / "R" / "P" / "D" -> 슬롯 태그 */
bool ParseSlot(const FString& Name, FGameplayTag& OutTag)
{
	if (Name.Equals(TEXT("P"), ESearchCase::IgnoreCase)) { OutTag = ERTags::Ability_Slot_P; return true; }
	if (Name.Equals(TEXT("Q"), ESearchCase::IgnoreCase)) { OutTag = ERTags::Ability_Slot_Q; return true; }
	if (Name.Equals(TEXT("W"), ESearchCase::IgnoreCase)) { OutTag = ERTags::Ability_Slot_W; return true; }
	if (Name.Equals(TEXT("E"), ESearchCase::IgnoreCase)) { OutTag = ERTags::Ability_Slot_E; return true; }
	if (Name.Equals(TEXT("R"), ESearchCase::IgnoreCase)) { OutTag = ERTags::Ability_Slot_R; return true; }
	if (Name.Equals(TEXT("D"), ESearchCase::IgnoreCase)) { OutTag = ERTags::Ability_Slot_D; return true; }
	return false;
}

AERPlayerState* GetLocalERPlayerState(UWorld* World)
{
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	AERPlayerState* PS = PC ? PC->GetPlayerState<AERPlayerState>() : nullptr;
	if (!PS)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[스킬디버그] ERPlayerState 가 없다."));
	}
	return PS;
}

/**
 * 서버 월드의 **모든** 플레이어에게 적용한다.
 * ⭐ 리슨 서버 창의 "로컬 폰" 은 서버 플레이어(PS_0)뿐이라, 클라 플레이어를 건드리려면 전원에 걸어야 한다.
 *   (처음엔 로컬 폰에만 걸어서 클라 VP 가 안 차는 것처럼 보였다 — 2026-09-13)
 * ⚠ 데디케이티드 PIE 는 서버 창이 없다. 리슨 서버로 테스트한다.
 */
template <typename TFunc>
void ForEachServerPlayer(UWorld* World, TFunc Func)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[스킬디버그] 서버 창에서만 된다."));
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

// ─────────────────────────────────────────────────────────────
// ER.Skill.Points <n> — 전원에게 포인트 지급 (서버 창). F10 이 생기기 전 대용.
// ─────────────────────────────────────────────────────────────
void SkillPointsCmd(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		return;
	}
	const int32 Amount = FCString::Atoi(*Args[0]);
	ForEachServerPlayer(World, [Amount](AERPlayerState* PS) { PS->AddSkillPoints(Amount); });
}

// ─────────────────────────────────────────────────────────────
// ER.Skill.LevelUp <Q|W|E|R> — 클라 창에서. 실제 게임 경로(Server RPC)를 그대로 탄다.
// ─────────────────────────────────────────────────────────────
void SkillLevelUpCmd(const TArray<FString>& Args, UWorld* World)
{
	AERPlayerState* PS = GetLocalERPlayerState(World);
	FGameplayTag SlotTag;
	if (!PS || Args.Num() < 1 || !ParseSlot(Args[0], SlotTag))
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[스킬디버그] 사용법: ER.Skill.LevelUp <P|Q|W|E|R|D>"));
		return;
	}
	PS->ServerLevelUpSkill(SlotTag);
}

// ─────────────────────────────────────────────────────────────
// ER.Skill.Levels — 슬롯별 레벨/상한 + 포인트 + HP/VP (어느 창에서든)
// ─────────────────────────────────────────────────────────────
void SkillLevelsCmd(const TArray<FString>& Args, UWorld* World)
{
	UAbilitySystemComponent* ASC = GetLocalSkillASC(World);
	const AERPlayerState* PS = GetLocalERPlayerState(World);
	if (!ASC || !PS)
	{
		return;
	}
	const TCHAR* Side = World->GetNetMode() == NM_Client ? TEXT("클라") : TEXT("서버");

	UE_LOG(LogEternalReturn, Warning, TEXT("[스킬디버그][%s] %s 포인트=%d  HP=%.0f  VP=%.0f"),
		Side, *PS->GetName(), PS->SkillPoints,
		ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute()),
		ASC->GetNumericAttribute(UERAttributeSet::GetVPAttribute()));

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		const UERSkillData* Skill = Cast<UERSkillData>(Spec.SourceObject.Get());
		if (!Skill)
		{
			continue;
		}
		UE_LOG(LogEternalReturn, Warning, TEXT("[스킬디버그][%s]   %s %s Lv.%d/%d  코스트=%.0f  포인트사용=%s"),
			Side, *Skill->SlotTag.ToString(), *GetNameSafe(Skill), Spec.Level, Skill->MaxLevel,
			Skill->GetCost(Spec.Level), Skill->bUsesSkillPoints ? TEXT("O") : TEXT("X"));
	}
}

// ─────────────────────────────────────────────────────────────
// ER.Skill.HP <값> / ER.Skill.VP <값> — 서버 창에서 **전원** 세팅 (코스트 검증용)
// ─────────────────────────────────────────────────────────────
void SetVital(const TArray<FString>& Args, UWorld* World, const FGameplayAttribute& Attribute, const TCHAR* Label)
{
	if (Args.Num() < 1)
	{
		return;
	}
	const float Value = FCString::Atof(*Args[0]);
	ForEachServerPlayer(World, [&](AERPlayerState* PS)
	{
		if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
		{
			ASC->SetNumericAttributeBase(Attribute, Value);
			UE_LOG(LogEternalReturn, Warning, TEXT("[스킬디버그] %s %s=%.0f"), *PS->GetName(), Label, ASC->GetNumericAttribute(Attribute));
		}
	});
}
void SkillHPCmd(const TArray<FString>& Args, UWorld* World) { SetVital(Args, World, UERAttributeSet::GetHPAttribute(), TEXT("HP")); }
void SkillVPCmd(const TArray<FString>& Args, UWorld* World) { SetVital(Args, World, UERAttributeSet::GetVPAttribute(), TEXT("VP")); }

// ─────────────────────────────────────────────────────────────
// ER.Skill.Haste <값> — 스킬 가속을 직접 세팅 (서버 창에서)
//   ⚠ 어트리뷰트 Base 를 바꾼다. 아이템 경로(F08)가 생기면 그쪽으로 검증한다.
// ─────────────────────────────────────────────────────────────
void SkillHasteCmd(const TArray<FString>& Args, UWorld* World)
{
	SetVital(Args, World, UERAttributeSet::GetSkillHasteAttribute(), TEXT("스킬 가속"));
}

// ─────────────────────────────────────────────────────────────
// ER.Skill.Tags — 이 월드가 아는 모든 플레이어의 ASC 소유 태그 (어느 창에서든)
//   다른 플레이어의 State.Casting 이 복제되는지 볼 때.
// ─────────────────────────────────────────────────────────────
void SkillTagsCmd(const TArray<FString>& Args, UWorld* World)
{
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}
	const TCHAR* Side = World->GetNetMode() == NM_Client ? TEXT("클라") : TEXT("서버");
	for (APlayerState* PS : GS->PlayerArray)
	{
		const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS);
		if (!ASC)
		{
			continue;
		}
		FGameplayTagContainer Tags;
		ASC->GetOwnedGameplayTags(Tags);
		UE_LOG(LogEternalReturn, Warning, TEXT("[스킬디버그][%s] %s 태그: %s"), Side, *PS->GetName(), *Tags.ToStringSimple());
	}
}

} // namespace

static FAutoConsoleCommandWithWorldAndArgs GERSkillTagsCmd(
	TEXT("ER.Skill.Tags"), TEXT("[임시] 모든 플레이어의 ASC 소유 태그"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SkillTagsCmd));

static FAutoConsoleCommandWithWorldAndArgs GERSkillPointsCmd(
	TEXT("ER.Skill.Points"), TEXT("[임시] 전원 스킬 포인트 지급 (서버 창). ER.Skill.Points <n>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SkillPointsCmd));
static FAutoConsoleCommandWithWorldAndArgs GERSkillLevelUpCmd(
	TEXT("ER.Skill.LevelUp"), TEXT("[임시] 슬롯 레벨업 요청 (클라 -> 서버 RPC). ER.Skill.LevelUp <Q|W|E|R>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SkillLevelUpCmd));
static FAutoConsoleCommandWithWorldAndArgs GERSkillLevelsCmd(
	TEXT("ER.Skill.Levels"), TEXT("[임시] 슬롯별 레벨 · 포인트 · HP/VP"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SkillLevelsCmd));
static FAutoConsoleCommandWithWorldAndArgs GERSkillHPCmd(
	TEXT("ER.Skill.HP"), TEXT("[임시] 전원 HP 세팅 (서버 창). ER.Skill.HP <값>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SkillHPCmd));
static FAutoConsoleCommandWithWorldAndArgs GERSkillVPCmd(
	TEXT("ER.Skill.VP"), TEXT("[임시] 전원 VP 세팅 (서버 창). ER.Skill.VP <값>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SkillVPCmd));

static FAutoConsoleCommandWithWorldAndArgs GERSkillCooldownCmd(
	TEXT("ER.Skill.Cooldown"),
	TEXT("[임시] 스킬별 쿨다운 남은/전체 시간"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SkillCooldownCmd));

static FAutoConsoleCommandWithWorldAndArgs GERSkillHasteCmd(
	TEXT("ER.Skill.Haste"),
	TEXT("[임시] 스킬 가속 설정. ER.Skill.Haste <값>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SkillHasteCmd));
