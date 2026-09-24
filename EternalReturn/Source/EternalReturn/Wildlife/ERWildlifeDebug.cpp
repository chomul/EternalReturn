// Copyright Epic Games, Inc. All Rights Reserved.
//
// [임시] F12 야생동물 디버그 명령. 스폰 · 조회. F17 에서 지운다.

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EternalReturn.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GAS/ERAttributeSet.h"
#include "HAL/IConsoleManager.h"
#include "Core/ERTeamStatics.h"
#include "Wildlife/ERWildlifeAIController.h"
#include "Wildlife/ERWildlifeCharacter.h"
#include "Wildlife/ERWildlifeData.h"
#include "Wildlife/ERWildlifeSettings.h"
#include "Wildlife/ERWildlifeSpawnSubsystem.h"

namespace
{

// ER.Wild.Spawn <이름> [레벨] — 서버 창의 첫 플레이어 폰 앞 3m 에 스폰. 이름 = DA_Wild_<이름>. 예: ER.Wild.Spawn Bear · ER.Wild.Spawn MutantBear 3
void WildSpawnCmd(const TArray<FString>& Args, UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[야생동물디버그] 서버 창에서만 된다."));
		return;
	}
	if (Args.Num() < 1)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[야생동물디버그] 사용법: ER.Wild.Spawn <이름> [레벨]  (DA_Wild_<이름>)"));
		return;
	}
	const UERWildlifeData* Data = UERWildlifeSettings::FindData(FName(*Args[0]));
	if (!Data)
	{
		return;   // FindData 가 Error 로그
	}
	// 레벨을 안 주면 종의 기초 레벨 (닭 1 · 곰 6 — F12-03)
	const int32 Level = Args.Num() >= 2 ? FMath::Max(1, FCString::Atoi(*Args[1])) : Data->BaseLevel;

	const UERWildlifeSettings& S = UERWildlifeSettings::Get();
	UClass* Class = S.WildlifeClass.IsNull() ? nullptr : S.WildlifeClass.LoadSynchronous();
	if (!Class)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[야생동물디버그] Project Settings > Game > ER Wildlife 에 WildlifeClass 가 없다."));
		return;
	}

	const APlayerController* PC = World->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[야생동물디버그] 서버 플레이어 폰이 없다."));
		return;
	}
	const FVector Location = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 300.f;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AERWildlifeCharacter* Wild = World->SpawnActor<AERWildlifeCharacter>(Class, Location, FRotator::ZeroRotator, Params);
	if (!Wild)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[야생동물디버그] 스폰 실패 (%s)."), *GetNameSafe(Class));
		return;
	}
	if (!Wild->Initialize(Data, Level))
	{
		Wild->Destroy();
	}
}

// ER.Wild.Show — 이 창에서 보이는 모든 야생동물의 종 · 레벨 · 스탯 · 태그. 클라 창에서는 복제된 값이다.
void WildShowCmd(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}
	const TCHAR* Side = World->GetNetMode() == NM_Client ? TEXT("클라") : TEXT("서버");
	int32 Count = 0;
	for (TActorIterator<AERWildlifeCharacter> It(World); It; ++It)
	{
		const AERWildlifeCharacter* W = *It;
		const UAbilitySystemComponent* ASC = W->GetAbilitySystemComponent();
		FGameplayTagContainer Tags;
		if (ASC) { ASC->GetOwnedGameplayTags(Tags); }
		const AERWildlifeAIController* AI = W->GetController<AERWildlifeAIController>();   // 서버에만 있다
		UE_LOG(LogEternalReturn, Warning, TEXT("[야생동물디버그][%s] %s %s Lv.%d%s%s%s  HP %.1f / %.1f  공격력 %.1f  방어력 %.1f  이동 %.2f  사거리 %.2f  태그 [%s]  도먼시 %d"),
			Side, *W->GetName(), *GetNameSafe(W->GetData()), W->GetLevel(), W->IsMutant() ? TEXT(" 변이") : TEXT(""), W->IsBoss() ? TEXT(" 보스") : TEXT(""),
			AI ? *FString::Printf(TEXT(" [AI %s · 팀 %d]"), AERWildlifeAIController::StateName(AI->GetAIState()), AI->GetAggroTeam()) : (W->IsDead() ? TEXT(" [시체]") : TEXT("")),
			ASC ? ASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute()) : 0.f, ASC ? ASC->GetNumericAttribute(UERAttributeSet::GetMaxHPAttribute()) : 0.f,
			ASC ? ASC->GetNumericAttribute(UERAttributeSet::GetAttackPowerAttribute()) : 0.f, ASC ? ASC->GetNumericAttribute(UERAttributeSet::GetDefenseAttribute()) : 0.f,
			ASC ? ASC->GetNumericAttribute(UERAttributeSet::GetMoveSpeedAttribute()) : 0.f, ASC ? ASC->GetNumericAttribute(UERAttributeSet::GetAttackRangeAttribute()) : 0.f,
			*Tags.ToStringSimple(), static_cast<int32>(W->NetDormancy));
		++Count;
	}
	if (Count == 0)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[야생동물디버그][%s] 야생동물 없음"), Side);
	}
}

// ER.Wild.HP <n> — 모든 야생동물의 체력을 n 으로 (서버 창). 사망 검증용 — 30 으로 두고 한 대 때린다 (피해 경로를 그대로 타게).
void WildHPCmd(const TArray<FString>& Args, UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client || Args.Num() < 1)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[야생동물디버그] 사용법 (서버 창): ER.Wild.HP <n>"));
		return;
	}
	const float HP = FMath::Max(1.f, FCString::Atof(*Args[0]));
	for (TActorIterator<AERWildlifeCharacter> It(World); It; ++It)
	{
		if (UAbilitySystemComponent* ASC = It->GetAbilitySystemComponent())
		{
			ASC->SetNumericAttributeBase(UERAttributeSet::GetHPAttribute(), HP);
			It->FlushNetDormancy();
			UE_LOG(LogEternalReturn, Warning, TEXT("[야생동물디버그] %s HP -> %.0f"), *It->GetName(), HP);
		}
	}
}

// ER.Wild.Stress <n> [이름] — 서버 플레이어 주변 격자에 n 마리 스폰 (기본 Bear). 클래스 비용 측정용 — `stat unit` 의 Game ms 를 0 / n / n+틱 끔 으로 비교.
void WildStressCmd(const TArray<FString>& Args, UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client || Args.Num() < 1)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[야생동물디버그] 사용법 (서버 창): ER.Wild.Stress <n> [이름]"));
		return;
	}
	const int32 Count = FMath::Clamp(FCString::Atoi(*Args[0]), 1, 1000);
	const UERWildlifeData* Data = UERWildlifeSettings::FindData(Args.Num() >= 2 ? FName(*Args[1]) : FName(TEXT("Bear")));
	const UERWildlifeSettings& S = UERWildlifeSettings::Get();
	UClass* Class = S.WildlifeClass.IsNull() ? nullptr : S.WildlifeClass.LoadSynchronous();
	const APlayerController* PC = World->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Data || !Class || !Pawn)
	{
		return;
	}
	const int32 Side = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Count)));
	const float Gap = 250.f;
	const FVector Origin = Pawn->GetActorLocation() + FVector(400.f, -Side * Gap * 0.5f, 0.f);
	int32 Made = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Loc = Origin + FVector((i / Side) * Gap, (i % Side) * Gap, 0.f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AERWildlifeCharacter* W = World->SpawnActor<AERWildlifeCharacter>(Class, Loc, FRotator::ZeroRotator, Params))
		{
			if (W->Initialize(Data, 1)) { ++Made; } else { W->Destroy(); }
		}
	}
	UE_LOG(LogEternalReturn, Warning, TEXT("[야생동물디버그] 스트레스 — %s %d마리 스폰 (격자 %d×%d · %.0fcm). `stat unit` 으로 Game ms 비교"), *Data->GetName(), Made, Side, Side, Gap);
}

// ER.Wild.Tick <0|1> — 모든 야생동물의 CMC · 메시 틱 on/off (서버 창). 유휴 비용 측정용 — 04 도먼시가 같은 걸 자동으로 한다.
void WildTickCmd(const TArray<FString>& Args, UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client || Args.Num() < 1)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[야생동물디버그] 사용법 (서버 창): ER.Wild.Tick <0|1>"));
		return;
	}
	const bool bOn = FCString::Atoi(*Args[0]) != 0;
	int32 N = 0;
	for (TActorIterator<AERWildlifeCharacter> It(World); It; ++It)
	{
		if (UCharacterMovementComponent* Move = It->GetCharacterMovement()) { Move->SetComponentTickEnabled(bOn); }
		if (USkeletalMeshComponent* Mesh = It->GetMesh()) { Mesh->SetComponentTickEnabled(bOn); }
		++N;
	}
	UE_LOG(LogEternalReturn, Warning, TEXT("[야생동물디버그] %d마리 CMC · 메시 틱 %s"), N, bOn ? TEXT("켬") : TEXT("끔"));
}

// ER.Wild.Clear — 모든 야생동물 제거 (서버 창)
void WildClearCmd(const TArray<FString>& Args, UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client) { return; }
	int32 N = 0;
	for (TActorIterator<AERWildlifeCharacter> It(World); It; ++It) { It->Destroy(); ++N; }
	UE_LOG(LogEternalReturn, Warning, TEXT("[야생동물디버그] %d마리 제거"), N);
}

// ER.Wild.Aggro <n> — 서버 첫 플레이어에게 가장 가까운 대기 중 야생동물 n 마리를 전투로 (서버 창 · F12-04 스트레스 ② — 이동 중 비용 측정)
void WildAggroCmd(const TArray<FString>& Args, UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client) { return; }
	const APlayerController* PC = World->GetFirstPlayerController();
	const APawn* Player = PC ? PC->GetPawn() : nullptr;
	const int32 Team = ERTeamStatics::GetTeamId(Player);
	if (!Player || Team == INDEX_NONE)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[야생동물디버그] 서버 플레이어의 팀을 못 찾았다."));
		return;
	}
	TArray<AERWildlifeAIController*> Idle;
	for (TActorIterator<AERWildlifeCharacter> It(World); It; ++It)
	{
		AERWildlifeAIController* AI = It->GetController<AERWildlifeAIController>();
		if (AI && !It->IsDead() && AI->GetAIState() == EERWildlifeAIState::Idle) { Idle.Add(AI); }
	}
	Idle.Sort([Player](const AERWildlifeAIController& A, const AERWildlifeAIController& B)
	{
		return FVector::DistSquared2D(A.GetPawn()->GetActorLocation(), Player->GetActorLocation()) < FVector::DistSquared2D(B.GetPawn()->GetActorLocation(), Player->GetActorLocation());
	});
	const int32 N = FMath::Min(Args.Num() >= 1 ? FCString::Atoi(*Args[0]) : 1, Idle.Num());
	for (int32 i = 0; i < N; ++i) { Idle[i]->EnterCombat(Team, false, TEXT("디버그")); }
	UE_LOG(LogEternalReturn, Warning, TEXT("[야생동물디버그] %d마리 전투 진입 (팀 %d) — `stat unit` 으로 Game ms"), N, Team);
}

// ER.Wild.Spawners — 스폰 자리 슬롯 상태 (서버 창 · F12-03)
void WildSpawnersCmd(const TArray<FString>&, UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client) { return; }
	if (const UERWildlifeSpawnSubsystem* Sub = World->GetSubsystem<UERWildlifeSpawnSubsystem>())
	{
		Sub->DumpSlots();
	}
}

} // namespace

static FAutoConsoleCommandWithWorldAndArgs GERWildAggroCmd(
	TEXT("ER.Wild.Aggro"), TEXT("[임시] 가까운 대기 야생동물 n 마리 전투 진입 (서버 창). ER.Wild.Aggro <n>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WildAggroCmd));
static FAutoConsoleCommandWithWorldAndArgs GERWildSpawnersCmd(
	TEXT("ER.Wild.Spawners"), TEXT("[임시] 스폰 자리 슬롯 상태 (서버 창)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WildSpawnersCmd));
static FAutoConsoleCommandWithWorldAndArgs GERWildStressCmd(
	TEXT("ER.Wild.Stress"), TEXT("[임시] n 마리 격자 스폰 — 클래스 비용 측정 (서버 창). ER.Wild.Stress <n> [이름]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WildStressCmd));
static FAutoConsoleCommandWithWorldAndArgs GERWildTickCmd(
	TEXT("ER.Wild.Tick"), TEXT("[임시] 야생동물 CMC · 메시 틱 on/off (서버 창). ER.Wild.Tick <0|1>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WildTickCmd));
static FAutoConsoleCommandWithWorldAndArgs GERWildClearCmd(
	TEXT("ER.Wild.Clear"), TEXT("[임시] 야생동물 전부 제거 (서버 창)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WildClearCmd));
static FAutoConsoleCommandWithWorldAndArgs GERWildHPCmd(
	TEXT("ER.Wild.HP"), TEXT("[임시] 모든 야생동물 체력 세팅 (서버 창). ER.Wild.HP <n>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WildHPCmd));
static FAutoConsoleCommandWithWorldAndArgs GERWildSpawnCmd(
	TEXT("ER.Wild.Spawn"), TEXT("[임시] 야생동물 스폰 (서버 창). ER.Wild.Spawn <이름> [레벨] (DA_Wild_<이름>)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WildSpawnCmd));
static FAutoConsoleCommandWithWorldAndArgs GERWildShowCmd(
	TEXT("ER.Wild.Show"), TEXT("[임시] 야생동물 전부의 스탯 · 태그"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WildShowCmd));
