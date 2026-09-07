// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/ERCharacterBase.h"
#include "Core/ERPlayerState.h"
#include "EternalReturn.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

AERCharacterBase::AERCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	// ── 탑다운 카메라 리그 ──────────────────────────────────
	// 값은 삭제된 이전 구현에서 쓰던 것을 그대로 가져왔다. 자체 결정값이며
	// 실제 플레이 감각을 보고 조정한다.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));

	// 캐릭터가 돌아도 시점이 같이 돌면 안 된다. 탑다운은 시점이 고정이다.
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw   = false;
	CameraBoom->bInheritRoll  = false;

	// ⭐ 벽에 닿았다고 카메라를 당기지 않는다.
	//   탑다운에서 시야가 갑자기 좁아지면 조작이 끊긴다.
	CameraBoom->bDoCollisionTest = false;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;

	// 컨트롤러 회전을 폰에 그대로 먹이지 않는다. 이동 방향으로 도는 것은 F05 에서 붙인다.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw   = false;
	bUseControllerRotationRoll  = false;

	// 스킬 판정(SkillTarget 채널) 응답은 여기서 건드리지 않는다.
	//
	// SetCollisionResponseToChannel 을 부르면 FBodyInstance 가
	// InvalidateCollisionProfileName() 을 호출해 프로파일이 Pawn -> Custom 으로 바뀌고
	// (BodyInstance.cpp:544-548), BP 에 저장된 Pawn 프로파일에 다시 덮인다.
	//
	// 대신 Config/DefaultEngine.ini 의 EditProfiles 로 Pawn 프로파일 자체를 고쳤다.
	// 상세: Docs/ErrorReport/E01_스킬판정_채널응답_무시됨.md
}

UAbilitySystemComponent* AERCharacterBase::GetAbilitySystemComponent() const
{
	const AERPlayerState* ERPlayerState = GetPlayerState<AERPlayerState>();
	return ERPlayerState ? ERPlayerState->GetAbilitySystemComponent() : nullptr;
}

void AERCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버 경로
	InitAbilityActorInfo();
}

void AERCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 클라 경로 — 이걸 빠뜨리면 클라에서 어빌리티가 조용히 안 나간다
	InitAbilityActorInfo();
}

void AERCharacterBase::InitAbilityActorInfo()
{
	AERPlayerState* ERPlayerState = GetPlayerState<AERPlayerState>();
	if (!ERPlayerState)
	{
		// PossessedBy 시점에 PlayerState 가 아직 없을 수 있다.
		// 그 경우 OnRep_PlayerState 또는 다음 Possess 에서 다시 불린다.
		return;
	}

	UAbilitySystemComponent* ASC = ERPlayerState->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// Owner = ASC 를 소유한 액터(PlayerState), Avatar = 월드에 서 있는 액터(이 폰)
	ASC->InitAbilityActorInfo(ERPlayerState, this);

	UE_LOG(LogEternalReturn, Log,
		TEXT("[GAS] InitAbilityActorInfo — %s / Owner=%s / Avatar=%s"),
		HasAuthority() ? TEXT("Server") : TEXT("Client"),
		*GetNameSafe(ERPlayerState), *GetNameSafe(this));
}
