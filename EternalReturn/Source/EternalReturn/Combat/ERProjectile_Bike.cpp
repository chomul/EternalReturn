// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ERProjectile_Bike.h"

#include "Components/SkeletalMeshComponent.h"
#include "EternalReturn.h"
#include "GameFramework/Character.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERSkillData.h"
#include "GAS/Fragment/ERSkillFragment_Ride.h"
#include "GAS/ERGameplayTags.h"
#include "Presentation/ERPresentationComponent.h"
#include "Presentation/ERPresentationData.h"

void AERProjectile_Bike::StartFlight()
{
	Super::StartFlight();
	if (!BikeComps.IsEmpty() || GetNetMode() == NM_DedicatedServer)
	{
		return;   // 한 번만 (BeginPlay · OnRep 둘 다 올 수 있다)
	}
	// 쏜 사람(Instigator — 복제된다)의 스킨 → 스킨마다 다른 바이크가 그대로 맞는다
	const APawn* Shooter = GetInstigator();
	const UERPresentationComponent* Pres = Shooter ? Shooter->FindComponentByClass<UERPresentationComponent>() : nullptr;
	const FERAttachProp* Prop = Pres ? Pres->FindProp(ERTags::Pres_Prop_Bike) : nullptr;
	if (!Prop)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[부착] %s — 쏜 사람 %s 의 스킨에 소품 Pres.Prop.Bike 가 없다 (Attach.json · ER.Pres.Fill)"), *GetName(), *GetNameSafe(Shooter));
		return;
	}
	// 바이크 몸에는 캐릭터 소켓이 없다 → 탔을 때와 같은 자리가 되게 "조각 오프셋 × 소켓(컴포넌트 공간) × 메시 상대 위치"를 루트 기준으로 미리 곱한다
	const ACharacter* ShooterChar = Cast<ACharacter>(Shooter);
	const USkeletalMeshComponent* ShooterMesh = ShooterChar ? ShooterChar->GetMesh() : nullptr;
	TArray<FERAttachPiece> Pieces = Prop->Pieces;
	for (FERAttachPiece& P : Pieces)
	{
		if (ShooterMesh)
		{
			const FTransform SocketCS = P.Socket.IsNone() ? FTransform::Identity : ShooterMesh->GetSocketTransform(P.Socket, RTS_Component);
			P.Offset = P.Offset * SocketCS * ShooterMesh->GetRelativeTransform();
		}
		P.Socket = NAME_None;
	}
	UERPresentationComponent::SpawnPieces(this, GetRootComponent(), Pieces, BikeComps);
	UE_LOG(LogEternalReturn, Log, TEXT("[부착] %s ← %s 의 바이크 %d조각"), *GetName(), *GetNameSafe(Shooter), BikeComps.Num());
}

void AERProjectile_Bike::Tick(float DeltaSeconds)
{
	const FVector Before = GetActorLocation();
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || bEnded || !bFlying)
	{
		return;
	}
	// 벽 — 이번 틱에 지나온 구간을 정적 지형으로 쓸기
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ERBikeWall), false, this);
	Params.AddIgnoredActor(GetOwner());
	if (GetWorld()->SweepSingleByChannel(Hit, Before, GetActorLocation(), FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(Launch.RadiusUU), Params))
	{
		Explode(Hit.ImpactPoint, *FString::Printf(TEXT("발사 바이크 벽 %s"), *GetNameSafe(Hit.GetActor())));
		EndFlight(TEXT("벽"));
	}
}

void AERProjectile_Bike::OnHitTarget(AActor* Target)
{
	// 적에 닿으면 그 자리 폭발 (부딪힌 대상 하나가 아니라 반경) → 끝
	Filter.IgnoredActors.Add(Target);
	Explode(Target->GetActorLocation(), *FString::Printf(TEXT("발사 바이크 충돌 %s"), *GetNameSafe(Target)));
	EndFlight(TEXT("적중"));
}

void AERProjectile_Bike::Explode(const FVector& Where, const TCHAR* Why)
{
	const UERSkillFragment_Ride* Cfg = Skill ? Skill->FindFragment<UERSkillFragment_Ride>() : nullptr;
	UERGameplayAbility* A = Ability.Get();
	if (!Cfg || !A)
	{
		UE_LOG(LogEternalReturn, Warning, TEXT("[탑승] %s — 폭발 설정이 없다 (재사용 DA 에 탑승 조각 · 어빌리티)"), *GetNameSafe(this));
		return;
	}
	Cfg->ExplodeAt(A, Where, Why);
}
