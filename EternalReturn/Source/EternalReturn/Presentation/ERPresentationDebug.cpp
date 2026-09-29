// Copyright Epic Games, Inc. All Rights Reserved.
//
// [임시] F12.5 연출 디버그 명령. F17 에서 지운다.
//   ER.Pres.Show [Wild]      — 이 창(서버 · 클라 어디서든)의 실험체 해석 결과 전부. Wild 면 야생동물도. 클라에서 치면 클라의 캐시 · 로드 상태를 본다 (P2 확인).
//   ER.Skin.Set <n> [플레이어] — 서버 창. n 번 스킨으로 (UERCharacterData.Skins 인덱스). 플레이어 = GameState.PlayerArray 순번 (기본 0).

#include "Engine/World.h"
#include "EngineUtils.h"
#include "EternalReturn.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"
#include "Character/ERCharacterBase.h"
#include "Core/ERPlayerState.h"
#include "Presentation/ERPresentationComponent.h"
#include "Wildlife/ERWildlifeCharacter.h"

namespace
{

void PresShowCmd(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}
	const bool bWild = Args.Num() >= 1 && Args[0].Equals(TEXT("Wild"), ESearchCase::IgnoreCase);
	int32 Count = 0;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (!It->IsA<AERCharacterBase>() && !(bWild && It->IsA<AERWildlifeCharacter>()))
		{
			continue;
		}
		if (const UERPresentationComponent* Pres = It->FindComponentByClass<UERPresentationComponent>())
		{
			Pres->DumpToLog();
			++Count;
		}
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[연출] Show 끝 — %d개 (%s)"), Count, World->GetNetMode() == NM_Client ? TEXT("클라 창") : TEXT("서버 창"));
}

void SkinSetCmd(const TArray<FString>& Args, UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[연출] ER.Skin.Set 은 서버 창에서만 된다."));
		return;
	}
	if (Args.Num() < 1)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[연출] 사용법: ER.Skin.Set <n> [플레이어 순번]"));
		return;
	}
	const AGameStateBase* GS = World->GetGameState();
	const int32 PlayerIdx = Args.Num() >= 2 ? FCString::Atoi(*Args[1]) : 0;
	AERPlayerState* PS = GS && GS->PlayerArray.IsValidIndex(PlayerIdx) ? Cast<AERPlayerState>(GS->PlayerArray[PlayerIdx]) : nullptr;
	if (!PS)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[연출] 플레이어 %d 가 없다."), PlayerIdx);
		return;
	}
	PS->SetSkinIndex(FMath::Max(0, FCString::Atoi(*Args[0])));
}

}   // namespace

static FAutoConsoleCommandWithWorldAndArgs GERPresShowCmd(
	TEXT("ER.Pres.Show"), TEXT("[임시] 실험체(· Wild 면 야생동물) 연출 해석 결과 — 키 · 애셋 · 출처(스킨/무기세트/기본). 서버 · 클라 창 각각"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PresShowCmd));
static FAutoConsoleCommandWithWorldAndArgs GERSkinSetCmd(
	TEXT("ER.Skin.Set"), TEXT("[임시] 스킨 바꾸기 (서버 창). ER.Skin.Set <n> [플레이어 순번]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SkinSetCmd));
