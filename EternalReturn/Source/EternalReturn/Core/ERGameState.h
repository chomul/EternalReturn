// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ERGameState.generated.h"

/** 팀 하나의 구성. TArray<TArray<>> 는 복제되지 않으므로 한 겹 감싼다. */
USTRUCT()
struct FERTeamRoster
{
	GENERATED_BODY()

	/** 이 팀에 속한 PlayerState 들. 서버가 채우고 전원에게 복제된다. */
	UPROPERTY()
	TArray<TObjectPtr<class APlayerState>> Members;
};

/**
 * 모든 클라이언트가 알아야 하는 매치 상태.
 *
 * GameMode 는 서버에만 있으므로, 클라가 봐야 하는 값은 여기 둔다.
 *
 * ⚠ bAlwaysRelevant = true 이고 NetPriority 가 높다(GameStateBase.cpp).
 *   즉 여기 넣는 것은 전원에게 간다 — 인벤토리 같은 큰 데이터를 넣지 않는다.
 *   팀 구성은 24명 규모라 문제없다.
 */
UCLASS()
class AERGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** [서버 전용] 팀 배열을 TeamCount 크기로 준비한다. 이미 그 크기면 아무것도 하지 않는다. */
	void InitTeams(int32 TeamCount);

	/**
	 * [서버 전용] 정원이 남은 첫 팀에 넣고 그 팀 번호를 돌려준다.
	 * 모든 팀이 찼으면 INDEX_NONE.
	 */
	int32 AssignToFirstOpenTeam(class APlayerState* PlayerState, int32 TeamSize);

	/** [서버 전용] 나간 플레이어를 팀에서 뺀다. */
	void RemoveFromTeams(class APlayerState* PlayerState);

	/** 해당 팀의 현재 인원. 범위를 벗어나면 0. */
	int32 GetTeamMemberCount(int32 TeamId) const;

	int32 GetTeamCount() const { return Teams.Num(); }

protected:
	UPROPERTY(Replicated)
	TArray<FERTeamRoster> Teams;
};
