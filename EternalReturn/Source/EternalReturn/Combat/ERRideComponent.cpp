// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ERRideComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "Core/ERTeamStatics.h"
#include "EternalReturn.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GAS/ERAttributeSet.h"
#include "GAS/ERGameplayAbility.h"
#include "GAS/ERGameplayTags.h"
#include "GAS/Fragment/ERSkillFragment_Ride.h"
#include "Presentation/ERPresentationComponent.h"
#include "Presentation/ERPresentationData.h"

namespace
{
	/** 탑승 태그가 늦게 복제될 수 있다 — 시작 뒤 이만큼은 태그 없어도 조종 (소유 클라) */
	constexpr float TagGraceSeconds = 0.5f;
	/** 시작 직후 제자리 벽 · 붙어 있던 적을 충돌로 치지 않는다 `[자체]` */
	constexpr float CollideGraceSeconds = 0.15f;
}

UERRideComponent::UERRideComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UERRideComponent::BeginServer(const UERSkillFragment_Ride* InConfig, UERGameplayAbility* InAbility)
{
	Config = InConfig;
	Ability = InAbility;
	bServer = true;
	StartTime = GetWorld()->GetTimeSeconds();
	SetComponentTickEnabled(true);
}

void UERRideComponent::BeginLocal(const UERSkillFragment_Ride* InConfig)
{
	Config = InConfig;
	bLocal = true;
	bHasSteerTarget = false;
	StartTime = GetWorld()->GetTimeSeconds();
	HeadingYaw = GetOwner()->GetActorRotation().Yaw;
	// 클릭 이동 경로를 끈다 — 탄 동안은 조종 입력만 (우클릭은 목표만 바꾼다)
	if (const APawn* Pawn = Cast<APawn>(GetOwner()); Pawn && Pawn->GetController())
	{
		Pawn->GetController()->StopMovement();
	}
	SetComponentTickEnabled(true);
}

bool UERRideComponent::HasRidingTag() const
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	return ASC && ASC->HasMatchingGameplayTag(ERTags::State_Riding);
}

void UERRideComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Config)
	{
		SetComponentTickEnabled(false);
		return;
	}
	const float Elapsed = GetWorld()->GetTimeSeconds() - StartTime;
	// 끝 — 서버가 탑승 GE 를 지웠다 (충돌 · 재사용) 또는 7초 만료 → 태그가 빠짐
	if (!HasRidingTag() && (bServer || Elapsed > TagGraceSeconds))
	{
		if (bServer && GetOwner()->HasAuthority())
		{
			EndServer(TEXT("시간 끝"));   // GE 만료 — 리캐스트 창 · 몽타주 정리
		}
		StopLocal();
		return;
	}
	if (bLocal)
	{
		TickSteer(DeltaTime);
	}
	if (bServer && Elapsed > CollideGraceSeconds)
	{
		TickCollide();
	}
}

void UERRideComponent::TickSteer(float DeltaTime)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}
	// 조종 목표 쪽으로 초당 TurnRateDeg 까지만 (누누 눈덩이) · 목표가 없으면 직진
	if (bHasSteerTarget)
	{
		const FVector To = (SteerTarget - Pawn->GetActorLocation()).GetSafeNormal2D();
		if (!To.IsNearlyZero())
		{
			const float Delta = FMath::FindDeltaAngleDegrees(HeadingYaw, To.Rotation().Yaw);
			const float Step = Config->TurnRateDeg * DeltaTime;
			HeadingYaw += FMath::Clamp(Delta, -Step, Step);
		}
	}
	// 입력 세기 = 지금 속도 / 최고 속도 → 엔진이 최고 속도(탑승 GE Override) × 세기로 가속 · 예측 그대로
	const float Elapsed = GetWorld()->GetTimeSeconds() - StartTime;
	const float Speed = FMath::Lerp(Config->SpeedStart, Config->SpeedMax, FMath::Clamp(Elapsed / Config->RampTime, 0.f, 1.f));
	Pawn->AddMovementInput(FRotator(0.f, HeadingYaw, 0.f).Vector(), Speed / Config->SpeedMax);
}

void UERRideComponent::TickCollide()
{
	ACharacter* Me = Cast<ACharacter>(GetOwner());
	UERGameplayAbility* A = Ability.Get();
	if (!Me || !A)
	{
		return;
	}
	const FVector Loc = Me->GetActorLocation();
	FVector Dir = Me->GetVelocity().GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = Me->GetActorForwardVector().GetSafeNormal2D();
	}
	const float CapsuleR = Me->GetCapsuleComponent() ? Me->GetCapsuleComponent()->GetScaledCapsuleRadius() : 40.f;
	const float Reach = CapsuleR + Config->HitReach * 100.f;

	// ① 적 실험체 — 앞쪽 반원 안 (뒤에 붙은 적은 안 친다) · 살아 있는 · 적대
	if (const AGameStateBase* GS = GetWorld()->GetGameState())
	{
		for (const APlayerState* PS : GS->PlayerArray)
		{
			APawn* Other = PS ? PS->GetPawn() : nullptr;
			if (!Other || Other == Me || !ERTeamStatics::IsHostile(Me, Other))
			{
				continue;
			}
			const UAbilitySystemComponent* OASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Other);
			if (!OASC || OASC->GetNumericAttribute(UERAttributeSet::GetHPAttribute()) <= 0.f)
			{
				continue;
			}
			const FVector ToOther = Other->GetActorLocation() - Loc;
			if (ToOther.Size2D() <= Reach + 40.f && FVector::DotProduct(ToOther.GetSafeNormal2D(), Dir) > 0.f)
			{
				Config->ExplodeAt(A, Other->GetActorLocation(), *FString::Printf(TEXT("충돌 %s"), *GetNameSafe(Other)));
				EndServer(TEXT("적 충돌"));
				return;
			}
		}
	}
	// ② 벽 — 정적 지형을 앞으로 짧게 쓸기 · 정면으로 막힐 때만 (비스듬히 스치면 걷기처럼 미끄러진다)
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ERRideWall), false, Me);
	if (GetWorld()->SweepSingleByChannel(Hit, Loc, Loc + Dir * (Reach - CapsuleR * 0.5f), FQuat::Identity, ECC_WorldStatic,
		FCollisionShape::MakeSphere(CapsuleR * 0.5f), Params)
		&& FVector::DotProduct(Hit.ImpactNormal.GetSafeNormal2D(), Dir) < -0.5f)
	{
		Config->ExplodeAt(A, Hit.ImpactPoint, *FString::Printf(TEXT("벽 %s"), *GetNameSafe(Hit.GetActor())));
		EndServer(TEXT("벽 충돌"));
	}
}

void UERRideComponent::EndServer(const TCHAR* Why)
{
	if (!bServer)
	{
		return;
	}
	bServer = false;
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	UERGameplayAbility* A = Ability.Get();
	if (ASC)
	{
		// 탑승 GE (태그 State.Riding) · 리캐스트 창 — 내린 뒤 재사용(발사)이 남지 않게
		FGameplayTagContainer Remove;
		Remove.AddTag(ERTags::State_Riding);
		if (A && A->GetRecastTag().IsValid()) { Remove.AddTag(A->GetRecastTag()); }
		ASC->RemoveActiveEffectsWithGrantedTags(Remove);
	}
	// 몽타주 End (서버 = 복제 · 소유 클라는 태그가 빠지면 StopLocal 에서)
	if (UERPresentationComponent* Pres = GetOwner()->FindComponentByClass<UERPresentationComponent>())
	{
		Pres->JumpSkillSection(ERTags::Ability_Slot_R, ERPresSection::End);
	}
	UE_LOG(LogEternalReturn, Log, TEXT("[탑승] %s 내림 — %s"), *GetNameSafe(GetOwner()), Why);
	if (!bLocal)
	{
		Config = nullptr;
		SetComponentTickEnabled(false);
	}
}

void UERRideComponent::StopLocal()
{
	if (bLocal)
	{
		bLocal = false;
		if (!GetOwner()->HasAuthority())
		{
			if (UERPresentationComponent* Pres = GetOwner()->FindComponentByClass<UERPresentationComponent>())
			{
				Pres->JumpSkillSection(ERTags::Ability_Slot_R, ERPresSection::End);
			}
		}
	}
	if (!bServer)
	{
		Config = nullptr;
		SetComponentTickEnabled(false);
	}
}
