// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ERGameState.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

void AERGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AERGameState, Teams);
}

void AERGameState::InitTeams(int32 TeamCount)
{
	if (!HasAuthority() || TeamCount <= 0)
	{
		return;
	}

	if (Teams.Num() != TeamCount)
	{
		Teams.Empty();
		Teams.SetNum(TeamCount);
	}
}

int32 AERGameState::AssignToFirstOpenTeam(APlayerState* PlayerState, int32 TeamSize)
{
	if (!HasAuthority() || !PlayerState || TeamSize <= 0)
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < Teams.Num(); ++Index)
	{
		if (Teams[Index].Members.Num() < TeamSize)
		{
			Teams[Index].Members.Add(PlayerState);
			return Index;
		}
	}

	// 모든 팀이 찼다. 호출자가 판단한다.
	return INDEX_NONE;
}

void AERGameState::RemoveFromTeams(APlayerState* PlayerState)
{
	if (!HasAuthority() || !PlayerState)
	{
		return;
	}

	for (FERTeamRoster& Team : Teams)
	{
		Team.Members.Remove(PlayerState);
	}
}

int32 AERGameState::GetTeamMemberCount(int32 TeamId) const
{
	return Teams.IsValidIndex(TeamId) ? Teams[TeamId].Members.Num() : 0;
}
