// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ERTeamStatics.h"
#include "Core/ERPlayerState.h"
#include "GameFramework/Pawn.h"

int32 ERTeamStatics::GetTeamId(const AActor* Actor)
{
	if (!Actor)
	{
		return INDEX_NONE;
	}

	// 폰이면 자기 PlayerState 를 통해 찾는다.
	// ⚠ GetController() 를 타지 않는다 — 클라에는 남의 컨트롤러가 없어서
	//   (Controller.cpp:51 bOnlyRelevantToOwner) 적의 팀을 못 읽게 된다.
	//   APawn::GetPlayerState() 는 복제된 PlayerState 를 직접 준다.
	if (const APawn* Pawn = Cast<const APawn>(Actor))
	{
		if (const AERPlayerState* ERPlayerState = Pawn->GetPlayerState<AERPlayerState>())
		{
			return ERPlayerState->TeamId;
		}
		return INDEX_NONE;
	}

	// PlayerState 를 직접 넘긴 경우.
	if (const AERPlayerState* ERPlayerState = Cast<const AERPlayerState>(Actor))
	{
		return ERPlayerState->TeamId;
	}

	// ⚠ 투사체·설치물·소환물은 아직 처리하지 않는다.
	//   소유자(Owner)를 타고 올라가야 하는데, 소유권이 생기는 것은 스킬 작업 이후다.
	//   그때 이 함수에 Owner 경로를 추가한다.
	return INDEX_NONE;
}

bool ERTeamStatics::IsSameTeam(const AActor* A, const AActor* B)
{
	const int32 TeamA = GetTeamId(A);
	const int32 TeamB = GetTeamId(B);

	// ⭐ 무소속끼리는 같은 팀이 아니다. 곰과 늑대가 아군이 되면 안 된다.
	if (TeamA == INDEX_NONE || TeamB == INDEX_NONE)
	{
		return false;
	}

	return TeamA == TeamB;
}

bool ERTeamStatics::IsHostile(const AActor* A, const AActor* B)
{
	if (!A || !B || A == B)
	{
		return false;
	}

	const int32 TeamA = GetTeamId(A);
	const int32 TeamB = GetTeamId(B);

	// 둘 다 팀이 있고 같은 팀일 때만 적이 아니다.
	// 한쪽이라도 무소속이면 적이다 — 야생동물을 때릴 수 있어야 한다.
	if (TeamA != INDEX_NONE && TeamB != INDEX_NONE)
	{
		return TeamA != TeamB;
	}

	return true;
}
