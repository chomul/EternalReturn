// Copyright Epic Games, Inc. All Rights Reserved.
//
// ⚠⚠ 임시 검증 코드다. F07(스킬 시스템)이 ERTargeting 의 실제 호출자가 되면 이 파일을 통째로 지운다. ⚠⚠
//
// 왜 만들었나:
//   판정은 기하 계산이라 로그 숫자만으로는 맞는지 확신이 안 선다.
//   부채꼴 각도·이중 반경 경계는 눈으로 봐야 틀린 걸 안다.
//   F07 이 붙기 전까지 ERTargeting 이 검증되지 않은 채 남는 것을 막으려는 목적이다.
//
// 지우는 법: 이 파일만 삭제하면 된다. 다른 파일에 참조가 없다.

#include "Combat/ERTargeting.h"
#include "Combat/ERTargetingTypes.h"
#include "Core/ERTeamStatics.h"
#include "EternalReturn.h"

#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "ERCollisionChannels.h"
#include "GameFramework/Character.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#if !UE_BUILD_SHIPPING

namespace ERTargetingDebug
{
	/** 기본 표시 시간. ER.Targeting.Duration 으로 바꿀 수 있다. */
	float DrawSeconds = 12.f;

	constexpr float MetersToUU = 100.f;

	/**
	 * 조준 방향 오버라이드(월드 Yaw, 도).
	 *
	 * 아직 이동·입력이 없어 캐릭터를 돌릴 수 없다. 방향이 필요한 판정(Projectile/Cone)을
	 * 시험하려면 방향을 손으로 줘야 한다. 음수면 폰의 정면을 그대로 쓴다.
	 */
	float YawOverride = -1.f;

	/**
	 * 판정이 수평(bIgnoreZ)이므로 형상도 바닥 평면에 그린다.
	 * 3D 구를 그리면 실제 판정과 어긋나 보인다.
	 */
	void DrawGroundCircle(UWorld* World, const FVector& Center, float RadiusUU,
	                      const FColor& Color, float Thickness)
	{
		if (RadiusUU <= 0.f)
		{
			return;
		}
		// 살짝 띄워야 지면에 파묻히지 않는다
		const FVector C = Center + FVector(0.f, 0.f, 4.f);
		DrawDebugCircle(World, C, RadiusUU, 48, Color, false, DrawSeconds, 0, Thickness,
			FVector(1, 0, 0), FVector(0, 1, 0), /*bDrawAxis=*/false);
	}

	/** 판정 기준점(발밑)을 표시한다. 여기서부터 거리를 잰다. */
	void DrawOriginMarker(UWorld* World, const FVector& Origin)
	{
		DrawDebugSphere(World, Origin, 12.f, 8, FColor::White, false, DrawSeconds, 0, 2.f);
		DrawDebugLine(World, Origin, Origin + FVector(0, 0, 120.f),
			FColor::White, false, DrawSeconds, 0, 1.5f);
		DrawDebugString(World, Origin + FVector(0, 0, 130.f), TEXT("Origin(발밑)"),
			nullptr, FColor::White, DrawSeconds, true);
	}

	/** 사거리 하한 — 이 안쪽은 제외된다. */
	void DrawRangeMin(UWorld* World, const FVector& Origin, float RangeMinM)
	{
		if (RangeMinM > 0.f)
		{
			DrawGroundCircle(World, Origin, RangeMinM * MetersToUU, FColor(120, 120, 120), 1.5f);
		}
	}

	/** 콘솔을 친 월드의 로컬 폰. 이게 시전자 역할을 한다. */
	APawn* GetLocalPawn(UWorld* World)
	{
		if (!World || !GEngine)
		{
			return nullptr;
		}
		APlayerController* PC = GEngine->GetFirstLocalPlayerController(World);
		return PC ? PC->GetPawn() : nullptr;
	}

	/** 서버 월드인지 클라 월드인지. 팀 필터 검증에서 이 구분이 중요하다. */
	const TCHAR* NetRoleName(const UWorld* World)
	{
		if (!World)
		{
			return TEXT("?");
		}
		switch (World->GetNetMode())
		{
		case NM_DedicatedServer: return TEXT("DedicatedServer");
		case NM_ListenServer:    return TEXT("ListenServer");
		case NM_Client:          return TEXT("Client");
		case NM_Standalone:      return TEXT("Standalone");
		default:                 return TEXT("?");
		}
	}

	ETargetTeamFilter ParseFilter(const TArray<FString>& Args, int32 Index)
	{
		if (!Args.IsValidIndex(Index))
		{
			return ETargetTeamFilter::Enemy;
		}
		const FString& S = Args[Index];
		if (S.Equals(TEXT("ally"), ESearchCase::IgnoreCase)) { return ETargetTeamFilter::Ally; }
		if (S.Equals(TEXT("all"),  ESearchCase::IgnoreCase)) { return ETargetTeamFilter::All;  }
		return ETargetTeamFilter::Enemy;
	}

	const TCHAR* FilterName(ETargetTeamFilter Filter)
	{
		switch (Filter)
		{
		case ETargetTeamFilter::Ally: return TEXT("Ally");
		case ETargetTeamFilter::All:  return TEXT("All");
		default:                      return TEXT("Enemy");
		}
	}

	float ArgFloat(const TArray<FString>& Args, int32 Index, float Default)
	{
		return Args.IsValidIndex(Index) ? FCString::Atof(*Args[Index]) : Default;
	}

	/** 액터가 서버 원본인지 복제된 프록시인지. 폰이 클라 월드에 없는 건지 가리는 데 쓴다. */
	const TCHAR* RoleName(const AActor* Actor)
	{
		if (!Actor)
		{
			return TEXT("?");
		}
		switch (Actor->GetLocalRole())
		{
		case ROLE_Authority:       return TEXT("Authority");
		case ROLE_AutonomousProxy: return TEXT("Autonomous");
		case ROLE_SimulatedProxy:  return TEXT("Simulated");
		default:                   return TEXT("None");
		}
	}

	const TCHAR* ResponseName(ECollisionResponse R)
	{
		switch (R)
		{
		case ECR_Ignore:  return TEXT("Ignore");
		case ECR_Overlap: return TEXT("Overlap");
		case ECR_Block:   return TEXT("Block");
		default:          return TEXT("?");
		}
	}

	/**
	 * 월드의 모든 ACharacter 를 채널·필터와 무관하게 나열한다.
	 *
	 * "왜 저 폰이 판정에 안 잡히지?" 를 가리는 용도다. 셋 중 하나로 좁혀진다.
	 *   1) 목록에 아예 없다        -> 그 월드에 폰이 없다 (복제/스폰 문제)
	 *   2) 있는데 응답이 Ignore    -> 콜리전 채널 설정 문제
	 *   3) 있고 Overlap 인데 안 잡힘 -> 사거리 또는 팀 필터 문제
	 */
	void Dump(UWorld* World)
	{
		APawn* Pawn = GetLocalPawn(World);
		const FVector Origin = Pawn ? ERTargeting::GetTargetingLocation(Pawn) : FVector::ZeroVector;

		UE_LOG(LogEternalReturn, Log, TEXT("[Targeting] === 월드의 캐릭터 전체 (%s) ==="), NetRoleName(World));
		UE_LOG(LogEternalReturn, Log, TEXT("[Targeting] 기준 = %s (Team %d) / 폰 정면 Yaw %.1f도"),
			*GetNameSafe(Pawn), ERTeamStatics::GetTeamId(Pawn),
			Pawn ? Pawn->GetActorForwardVector().Rotation().Yaw : 0.f);

		int32 Count = 0;
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			ACharacter* C = *It;
			if (!C)
			{
				continue;
			}
			++Count;

			ECollisionResponse Resp = ECR_MAX;
			if (const UCapsuleComponent* Capsule = C->GetCapsuleComponent())
			{
				Resp = Capsule->GetCollisionResponseToChannel(ERCollisionChannel::SkillTarget);
			}

			const FVector P = ERTargeting::GetTargetingLocation(C);
			const float DistM = FVector::Dist2D(Origin, P) / MetersToUU;
			const float Yaw   = (P - Origin).Rotation().Yaw;

			UE_LOG(LogEternalReturn, Log,
				TEXT("[Targeting]   %-52s Team %2d  %6.2fm  Yaw %6.1f도  SkillTarget=%-7s  Role=%s%s"),
				*GetNameSafe(C), ERTeamStatics::GetTeamId(C), DistM, Yaw,
				ResponseName(Resp), RoleName(C),
				(C == Pawn) ? TEXT("  <- 나 자신") : TEXT(""));
		}

		UE_LOG(LogEternalReturn, Log, TEXT("[Targeting] 총 %d개"), Count);

		if (Count <= 1)
		{
			UE_LOG(LogEternalReturn, Warning,
				TEXT("[Targeting] 캐릭터가 %d개뿐이다. PIE 의 Number of Players 를 확인할 것"), Count);
		}
	}

	/** 결과를 로그 + 화면에 그린다. 중앙(Inner)은 초록, 외곽(Hit)은 빨강. */
	void Report(UWorld* World, const FTargetQuery& Q, const FTargetResult& R, const TCHAR* ShapeName)
	{
		UE_LOG(LogEternalReturn, Log,
			TEXT("[Targeting] %s / %s / Filter=%s / Instigator=%s(Team %d)"),
			ShapeName, NetRoleName(World), FilterName(Q.TeamFilter),
			*GetNameSafe(Q.Instigator), ERTeamStatics::GetTeamId(Q.Instigator));

		if (R.IsEmpty())
		{
			UE_LOG(LogEternalReturn, Warning,
				TEXT("[Targeting]   맞은 대상 0개. 대상의 SkillTarget 채널 응답이 Overlap 인지 확인할 것"));
		}

		for (int32 i = 0; i < R.HitActors.Num(); ++i)
		{
			const AActor* Actor = R.HitActors[i];
			const float Dist = R.Distances.IsValidIndex(i) ? R.Distances[i] : -1.f;
			const FVector P = ERTargeting::GetTargetingLocation(Actor);

			// 이 대상이 몇 도 방향에 있는지. ER.Targeting.Yaw 에 그대로 넣으면 조준된다.
			const float Yaw = (P - Q.Origin).Rotation().Yaw;

			UE_LOG(LogEternalReturn, Log,
				TEXT("[Targeting]   [%d] %s  %.2fm  Yaw %.1f도  Team %d"),
				i, *GetNameSafe(Actor), Dist, Yaw, ERTeamStatics::GetTeamId(Actor));

			// 맞은 대상: 캡슐 전체를 감싸 눈에 띄게 + 기준점에서 선을 그어 거리를 보여준다
			DrawDebugCapsule(World, P + FVector(0, 0, 90.f), 90.f, 45.f,
				FQuat::Identity, FColor::Red, false, DrawSeconds, 0, 2.5f);
			DrawDebugLine(World, Q.Origin, P, FColor::Red, false, DrawSeconds, 0, 2.f);
			DrawDebugString(World, P + FVector(0, 0, 200.f),
				FString::Printf(TEXT("[%d] %.2fm"), i, Dist), nullptr, FColor::Red, DrawSeconds, true);
		}

		for (const AActor* Actor : R.InnerHitActors)
		{
			UE_LOG(LogEternalReturn, Log,
				TEXT("[Targeting]   [중앙] %s  Team %d"),
				*GetNameSafe(Actor), ERTeamStatics::GetTeamId(Actor));

			const FVector P = ERTargeting::GetTargetingLocation(Actor);
			DrawDebugCapsule(World, P + FVector(0, 0, 90.f), 90.f, 45.f,
				FQuat::Identity, FColor::Green, false, DrawSeconds, 0, 2.5f);
			DrawDebugLine(World, Q.Origin, P, FColor::Green, false, DrawSeconds, 0, 2.f);
			DrawDebugString(World, P + FVector(0, 0, 200.f), TEXT("중앙"),
				nullptr, FColor::Green, DrawSeconds, true);
		}

		// ⭐ 중복 포함 검사 — 레니 W 에서 한 액터가 양쪽에 들어가면 피해가 두 번 간다
		for (const AActor* Inner : R.InnerHitActors)
		{
			if (R.HitActors.Contains(Inner))
			{
				UE_LOG(LogEternalReturn, Error,
					TEXT("[Targeting]   ⚠ 중복 포함! %s 가 Inner 와 Hit 양쪽에 있다"), *GetNameSafe(Inner));
			}
		}
	}

	void Execute(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		APawn* Pawn = GetLocalPawn(World);
		if (!Pawn)
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[Targeting] 로컬 폰이 없다. PIE 실행 중인지 확인"));
			return;
		}

		if (Args.Num() == 0)
		{
			UE_LOG(LogEternalReturn, Log, TEXT("[Targeting] 사용법:"));
			UE_LOG(LogEternalReturn, Log, TEXT("  ER.Targeting.Test Sphere <반경m> [enemy|ally|all]"));
			UE_LOG(LogEternalReturn, Log, TEXT("  ER.Targeting.Test Cone <사거리m> <전체각도> [필터]"));
			UE_LOG(LogEternalReturn, Log, TEXT("  ER.Targeting.Test Projectile <사거리m> <관통0|1> [필터]"));
			UE_LOG(LogEternalReturn, Log, TEXT("  ER.Targeting.Test Dual <중앙m> <외곽m> [필터]"));
			UE_LOG(LogEternalReturn, Log, TEXT("  ER.Targeting.Test Single <사거리m> [필터]"));
			UE_LOG(LogEternalReturn, Log, TEXT("  ER.Targeting.Test Alpha <거리> <하한> <상한>"));
			UE_LOG(LogEternalReturn, Log, TEXT("  ER.Targeting.Duration <초>   (표시 시간, 기본 12)"));
			UE_LOG(LogEternalReturn, Log, TEXT("  ER.Targeting.Yaw <각도|-1>   (조준 방향. -1이면 폰 정면)"));
			UE_LOG(LogEternalReturn, Log, TEXT("  ER.Targeting.Dump            (월드의 캐릭터 전체 나열)"));
			return;
		}

		const FString& Shape = Args[0];
		const FVector  Origin = ERTargeting::GetTargetingLocation(Pawn);

		// 순수 수학이라 월드가 필요 없다. 따로 처리한다.
		if (Shape.Equals(TEXT("Alpha"), ESearchCase::IgnoreCase))
		{
			const float D = ArgFloat(Args, 1, 0.f);
			const float Lo = ArgFloat(Args, 2, 0.f);
			const float Hi = ArgFloat(Args, 3, 1.f);
			UE_LOG(LogEternalReturn, Log, TEXT("[Targeting] Alpha(%.2f, %.2f, %.2f) = %.4f"),
				D, Lo, Hi, ERTargeting::GetDistanceAlpha(D, Lo, Hi));
			return;
		}

		FTargetQuery Q;
		Q.Instigator = Pawn;
		Q.Origin     = Origin;
		Q.Direction  = (YawOverride >= 0.f)
			? FRotator(0.f, YawOverride, 0.f).Vector()
			: Pawn->GetActorForwardVector();

		// 시셀라 Q 의 하한 1.0m 를 항상 걸어둔다 — 너무 가까운 대상이 제외되는지 같이 본다.
		Q.RangeMin = 1.0f;

		if (Shape.Equals(TEXT("Sphere"), ESearchCase::IgnoreCase))
		{
			Q.Shape      = ESkillTargeting::SelfRadius;
			Q.RangeMax   = ArgFloat(Args, 1, 5.f);
			Q.TeamFilter = ParseFilter(Args, 2);

			DrawOriginMarker(World, Origin);
			DrawGroundCircle(World, Origin, Q.RangeMax * MetersToUU, FColor::Yellow, 3.f);
			DrawRangeMin(World, Origin, Q.RangeMin);
			Report(World, Q, ERTargeting::Query(World, Q), TEXT("Sphere(SelfRadius)"));
		}
		else if (Shape.Equals(TEXT("Cone"), ESearchCase::IgnoreCase))
		{
			Q.Shape      = ESkillTargeting::Cone;
			Q.RangeMax   = ArgFloat(Args, 1, 5.f);
			Q.AngleDeg   = ArgFloat(Args, 2, 65.f);   // 다니엘 Q 기본값
			Q.TeamFilter = ParseFilter(Args, 3);

			DrawOriginMarker(World, Origin);
			DrawRangeMin(World, Origin, Q.RangeMin);

			// 판정이 수평이므로 부채꼴도 바닥에 그린다.
			// 3D 콘을 그리면 실제 판정 범위와 어긋나 보인다.
			FVector Dir = Q.Direction;
			Dir.Z = 0.f;
			Dir.Normalize();

			const float RangeUU  = Q.RangeMax * MetersToUU;
			const float HalfDeg  = Q.AngleDeg * 0.5f;
			const FVector Up     = FVector(0, 0, 1);
			const FVector Base   = Origin + FVector(0, 0, 4.f);

			// 양쪽 경계선
			const FVector L = Base + Dir.RotateAngleAxis(-HalfDeg, Up) * RangeUU;
			const FVector R2 = Base + Dir.RotateAngleAxis(+HalfDeg, Up) * RangeUU;
			DrawDebugLine(World, Base, L,  FColor::Yellow, false, DrawSeconds, 0, 3.f);
			DrawDebugLine(World, Base, R2, FColor::Yellow, false, DrawSeconds, 0, 3.f);

			// 호(arc)
			const int32 Segments = 24;
			FVector Prev = L;
			for (int32 i = 1; i <= Segments; ++i)
			{
				const float A = -HalfDeg + (Q.AngleDeg * i / Segments);
				const FVector P = Base + Dir.RotateAngleAxis(A, Up) * RangeUU;
				DrawDebugLine(World, Prev, P, FColor::Yellow, false, DrawSeconds, 0, 3.f);
				Prev = P;
			}

			DrawDebugString(World, Base + Dir * RangeUU * 0.5f + FVector(0, 0, 60.f),
				FString::Printf(TEXT("%.0f°"), Q.AngleDeg), nullptr, FColor::Yellow, DrawSeconds, true);

			Report(World, Q, ERTargeting::Query(World, Q), TEXT("Cone"));
		}
		else if (Shape.Equals(TEXT("Projectile"), ESearchCase::IgnoreCase))
		{
			Q.Shape      = ESkillTargeting::Projectile;
			Q.RangeMax   = ArgFloat(Args, 1, 10.f);
			Q.bPenetrate = ArgFloat(Args, 2, 0.f) != 0.f;
			Q.TeamFilter = ParseFilter(Args, 3);

			DrawOriginMarker(World, Origin);
			DrawRangeMin(World, Origin, Q.RangeMin);

			FVector Dir = Q.Direction;
			Dir.Z = 0.f;
			Dir.Normalize();

			const float RangeUU  = Q.RangeMax * MetersToUU;
			const float RadiusUU = FMath::Max(Q.ProjectileRadius * MetersToUU, 1.f);
			const FVector Base   = Origin + FVector(0, 0, 4.f);
			const FVector End    = Base + Dir * RangeUU;

			// ⭐ 실제 판정은 반경 RadiusUU 짜리 구를 쓸어보낸다(SweepMultiByChannel).
			//   선 하나만 그리면 두께가 안 보여서 "왜 저게 맞지?" 가 된다.
			//   쓸린 부피를 캡슐로 그린다.
			const FVector Mid = (Base + End) * 0.5f;
			const FQuat   Rot = FRotationMatrix::MakeFromZ(Dir).ToQuat();
			DrawDebugCapsule(World, Mid, RangeUU * 0.5f + RadiusUU, RadiusUU, Rot,
				FColor::Yellow, false, DrawSeconds, 0, 2.f);

			// 중심선도 같이 (방향을 알아보기 쉽게)
			DrawDebugLine(World, Base, End, FColor(255, 255, 0, 120), false, DrawSeconds, 0, 1.f);

			DrawDebugString(World, End + FVector(0, 0, 60.f),
				FString::Printf(TEXT("%.1fm / 두께 %.2fm / %s"),
					Q.RangeMax, Q.ProjectileRadius, Q.bPenetrate ? TEXT("관통") : TEXT("비관통")),
				nullptr, FColor::Yellow, DrawSeconds, true);

			Report(World, Q, ERTargeting::Query(World, Q),
				Q.bPenetrate ? TEXT("Projectile(관통)") : TEXT("Projectile(비관통)"));
		}
		else if (Shape.Equals(TEXT("Dual"), ESearchCase::IgnoreCase))
		{
			Q.Shape       = ESkillTargeting::DualRadius;
			Q.RadiusInner = ArgFloat(Args, 1, 1.25f);   // 레니 W 기본값
			Q.RadiusOuter = ArgFloat(Args, 2, 2.25f);
			Q.RangeMax    = Q.RadiusOuter;
			Q.RangeMin    = 0.f;                        // 이중 반경은 하한을 쓰지 않는다
			Q.TeamFilter  = ParseFilter(Args, 3);

			DrawOriginMarker(World, Origin);
			DrawGroundCircle(World, Origin, Q.RadiusInner * MetersToUU, FColor::Green,  3.f);
			DrawGroundCircle(World, Origin, Q.RadiusOuter * MetersToUU, FColor::Yellow, 3.f);

			DrawDebugString(World, Origin + FVector(Q.RadiusInner * MetersToUU, 0, 30.f),
				FString::Printf(TEXT("중앙 %.2fm"), Q.RadiusInner), nullptr, FColor::Green, DrawSeconds, true);
			DrawDebugString(World, Origin + FVector(Q.RadiusOuter * MetersToUU, 0, 30.f),
				FString::Printf(TEXT("외곽 %.2fm"), Q.RadiusOuter), nullptr, FColor::Yellow, DrawSeconds, true);

			Report(World, Q, ERTargeting::Query(World, Q), TEXT("DualRadius"));
		}
		else if (Shape.Equals(TEXT("Single"), ESearchCase::IgnoreCase))
		{
			Q.RangeMax   = ArgFloat(Args, 1, 5.f);
			Q.TeamFilter = ParseFilter(Args, 2);

			// 대상 지정 판정은 "누구를" 이 필요하다. 원형으로 훑어 가장 가까운 하나를 고른다.
			FTargetQuery Finder = Q;
			Finder.Shape = ESkillTargeting::SelfRadius;
			const FTargetResult Found = ERTargeting::Query(World, Finder);
			if (Found.HitActors.IsEmpty())
			{
				UE_LOG(LogEternalReturn, Warning, TEXT("[Targeting] Single: 사거리 안에 후보가 없다"));
				return;
			}

			Q.Shape            = ESkillTargeting::SingleTarget;
			Q.DesignatedTarget = Found.HitActors[0];

			DrawOriginMarker(World, Origin);
			DrawGroundCircle(World, Origin, Q.RangeMax * MetersToUU, FColor::Yellow, 3.f);
			DrawRangeMin(World, Origin, Q.RangeMin);
			Report(World, Q, ERTargeting::Query(World, Q), TEXT("SingleTarget"));
		}
		else
		{
			UE_LOG(LogEternalReturn, Warning, TEXT("[Targeting] 모르는 형상: %s"), *Shape);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GERTargetingDurationCmd(
	TEXT("ER.Targeting.Duration"),
	TEXT("[임시] 디버그 표시 시간(초). 예: ER.Targeting.Duration 30"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* /*World*/)
		{
			if (Args.Num() > 0)
			{
				ERTargetingDebug::DrawSeconds = FMath::Max(FCString::Atof(*Args[0]), 0.1f);
			}
			UE_LOG(LogEternalReturn, Log, TEXT("[Targeting] 표시 시간 = %.1f초"), ERTargetingDebug::DrawSeconds);
		}));

static FAutoConsoleCommandWithWorldAndArgs GERTargetingDumpCmd(
	TEXT("ER.Targeting.Dump"),
	TEXT("[임시] 월드의 모든 캐릭터를 팀/거리/채널응답/Role 과 함께 나열한다"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& /*Args*/, UWorld* World)
		{
			ERTargetingDebug::Dump(World);
		}));

static FAutoConsoleCommandWithWorldAndArgs GERTargetingYawCmd(
	TEXT("ER.Targeting.Yaw"),
	TEXT("[임시] 조준 방향(월드 Yaw, 도). -1 이면 폰 정면. 예: ER.Targeting.Yaw 90"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* /*World*/)
		{
			if (Args.Num() > 0)
			{
				ERTargetingDebug::YawOverride = FCString::Atof(*Args[0]);
			}
			if (ERTargetingDebug::YawOverride < 0.f)
			{
				UE_LOG(LogEternalReturn, Log, TEXT("[Targeting] 조준 = 폰 정면"));
			}
			else
			{
				UE_LOG(LogEternalReturn, Log, TEXT("[Targeting] 조준 Yaw = %.1f도"), ERTargetingDebug::YawOverride);
			}
		}));

static FAutoConsoleCommandWithWorldAndArgs GERTargetingTestCmd(
	TEXT("ER.Targeting.Test"),
	TEXT("[임시] 판정 형상을 실행하고 결과를 그린다. 인자 없이 치면 사용법이 나온다."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ERTargetingDebug::Execute));

#endif // !UE_BUILD_SHIPPING
