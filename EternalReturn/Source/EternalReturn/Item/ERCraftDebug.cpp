// Copyright Epic Games, Inc. All Rights Reserved.
//
// ⚠⚠ **임시 파일이다. 제작 UI(F17)가 생기면 통째로 삭제한다.** ERItemDebug.cpp 와 같은 성격.

#include "EternalReturn.h"
#include "Engine/World.h"
#include "Item/ERCraftLibrary.h"
#include "Item/ERItemData.h"
#include "Item/ERInventoryComponent.h"
#include "Core/ERPlayerState.h"
#include "GameFramework/PlayerController.h"

namespace
{

FString Named(FName Id)
{
	const FERItemRow* Row = ERItem::Find(Id);
	return Row ? FString::Printf(TEXT("%s(%s)"), *Id.ToString(), *Row->DisplayName.ToString()) : Id.ToString();
}

// 트리를 들여쓰기로 찍는다 (재귀). 순환은 ExpandLeaves 가 잡으니 여기서는 깊이 32 에서 끊는다.
void PrintTree(FName Id, int32 Depth)
{
	FName A, B;
	const bool bCraftable = ERCraft::GetMaterials(Id, A, B);
	UE_LOG(LogEternalReturn, Warning, TEXT("[제작디버그] %s%s%s"), *FString::ChrN(Depth * 2, TEXT(' ')), *Named(Id), bCraftable ? TEXT("") : TEXT("  <- 잎"));
	if (bCraftable && Depth < 32)
	{
		PrintTree(A, Depth + 1);
		PrintTree(B, Depth + 1);
	}
}

// ER.Craft.Tree <id> — 트리 + 잎 재료 합산
void CraftTreeCmd(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[제작디버그] 사용법: ER.Craft.Tree <id>"));
		return;
	}
	const FName Id(*Args[0]);
	PrintTree(Id, 0);

	TMap<FName, int32> Leaves;
	if (!ERCraft::ExpandLeaves(Id, Leaves))
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[제작디버그] %s 잎 계산 실패 (위 Error 참고)"), *Id.ToString());
		return;
	}
	FString Line;
	for (const TPair<FName, int32>& P : Leaves)
	{
		Line += FString::Printf(TEXT("  %s x%d"), *Named(P.Key), P.Value);
	}
	UE_LOG(LogEternalReturn, Warning, TEXT("[제작디버그] %s 잎 재료:%s"), *Named(Id), *Line);
}

// ER.Craft.With <id> — 이 재료로 만들 수 있는 것
void CraftWithCmd(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[제작디버그] 사용법: ER.Craft.With <id>"));
		return;
	}
	TArray<FName> Results;
	ERCraft::FindUsing(FName(*Args[0]), Results);
	FString Line;
	for (const FName& R : Results)
	{
		Line += TEXT("  ") + Named(R);
	}
	UE_LOG(LogEternalReturn, Warning, TEXT("[제작디버그] %s 로 만들 수 있는 것 %d개:%s"), *Named(FName(*Args[0])), Results.Num(), *Line);
}

// ER.Craft.Result <a> <b> — 정방향
void CraftResultCmd(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 2)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[제작디버그] 사용법: ER.Craft.Result <a> <b>"));
		return;
	}
	const FName Result = ERCraft::FindResult(FName(*Args[0]), FName(*Args[1]));
	UE_LOG(LogEternalReturn, Warning, TEXT("[제작디버그] %s + %s -> %s"), *Args[0], *Args[1], Result.IsNone() ? TEXT("(없음)") : *Named(Result));
}

// ER.Craft.Make <id> — 이 창의 플레이어가 제작 요청 (클라 → ServerCraft RPC). 실제 게임 경로.
void CraftMakeCmd(const TArray<FString>& Args, UWorld* World)
{
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	AERPlayerState* PS = PC ? PC->GetPlayerState<AERPlayerState>() : nullptr;
	if (Args.Num() < 1 || !PS || !PS->GetInventory())
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[제작디버그] 사용법: ER.Craft.Make <id> (플레이어 필요)"));
		return;
	}
	PS->GetInventory()->ServerCraft(FName(*Args[0]));
}

} // namespace

static FAutoConsoleCommandWithWorldAndArgs GERCraftMakeCmd(
	TEXT("ER.Craft.Make"), TEXT("[임시] 제작 요청 (클라 -> 서버 RPC). ER.Craft.Make <id>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CraftMakeCmd));
static FAutoConsoleCommandWithWorldAndArgs GERCraftTreeCmd(
	TEXT("ER.Craft.Tree"), TEXT("[임시] 제작 트리 + 잎 재료. ER.Craft.Tree <id>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CraftTreeCmd));
static FAutoConsoleCommandWithWorldAndArgs GERCraftWithCmd(
	TEXT("ER.Craft.With"), TEXT("[임시] 이 재료로 만들 수 있는 것. ER.Craft.With <id>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CraftWithCmd));
static FAutoConsoleCommandWithWorldAndArgs GERCraftResultCmd(
	TEXT("ER.Craft.Result"), TEXT("[임시] A + B 의 결과. ER.Craft.Result <a> <b>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CraftResultCmd));
