// Copyright Epic Games, Inc. All Rights Reserved.

#include "MenuShark.h"

#include "BoatPawn.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

AMenuShark::AMenuShark()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	PathSpline = CreateDefaultSubobject<USplineComponent>(TEXT("PathSpline"));
	PathSpline->SetupAttachment(Root);
	PathSpline->ClearSplinePoints(false);
	PathSpline->AddSplinePoint(FVector(1200.0, 0.0, 0.0), ESplineCoordinateSpace::Local, false);
	PathSpline->AddSplinePoint(FVector(0.0, 1200.0, 0.0), ESplineCoordinateSpace::Local, false);
	PathSpline->AddSplinePoint(FVector(-1200.0, 0.0, 0.0), ESplineCoordinateSpace::Local, false);
	PathSpline->AddSplinePoint(FVector(0.0, -1200.0, 0.0), ESplineCoordinateSpace::Local, false);
	PathSpline->SetClosedLoop(true, false);
	for (int32 Index = 0; Index < PathSpline->GetNumberOfSplinePoints(); ++Index)
	{
		PathSpline->SetSplinePointType(Index, ESplinePointType::Curve, false);
	}
	PathSpline->UpdateSpline();

	SwimPivot = CreateDefaultSubobject<USceneComponent>(TEXT("SwimPivot"));
	SwimPivot->SetupAttachment(Root);

	// The mesh's own nose-to-tail axis is its local Y (not X); Yaw -90 turns that to face the actor's forward (+X).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SharkMesh(TEXT("/Game/Models/KopekBaligi/Kopekbaligi_Boyali.Kopekbaligi_Boyali"));
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(SwimPivot);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));
	if (SharkMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(SharkMesh.Object);
	}
}

void AMenuShark::BeginPlay()
{
	Super::BeginPlay();

	for (TActorIterator<ABoatPawn> It(GetWorld()); It; ++It)
	{
		ReferenceBoat = *It;
		break;
	}

	// Snapshot now, before the actor moves at all: PathSpline is a child component, so every query below reads
	// its points through this frozen frame, never the actor's own (about to start moving) live transform.
	HomeTransform = GetActorTransform();

	DistanceAlongSpline = 0.f;
	if (PathSpline && PathSpline->GetNumberOfSplinePoints() > 1)
	{
		const FVector LocalPoint = PathSpline->GetLocationAtDistanceAlongSpline(0.f, ESplineCoordinateSpace::Local);
		FVector Location = HomeTransform.TransformPosition(LocalPoint);
		Location.Z = SurfaceZ(Location) - SwimDepth;

		const FVector LocalDirection = PathSpline->GetDirectionAtDistanceAlongSpline(0.f, ESplineCoordinateSpace::Local);
		const FVector Direction = HomeTransform.TransformVectorNoScale(LocalDirection);
		CurrentYaw = Direction.SizeSquared2D() > KINDA_SMALL_NUMBER ? Direction.Rotation().Yaw : GetActorRotation().Yaw;
		SetActorLocationAndRotation(Location, FRotator(0.f, CurrentYaw, 0.f));
	}
}

float AMenuShark::SurfaceZ(const FVector& Location) const
{
	return ReferenceBoat ? ReferenceBoat->GetSurfaceZ(FVector2D(Location.X, Location.Y)) : 0.f;
}

void AMenuShark::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Dt = FMath::Clamp(DeltaSeconds, 0.001f, 0.05f);
	if (!PathSpline || PathSpline->GetNumberOfSplinePoints() < 2)
	{
		return;
	}

	const float SplineLength = PathSpline->GetSplineLength();
	if (SplineLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	DistanceAlongSpline += SwimSpeed * Dt;
	if (bLoop)
	{
		DistanceAlongSpline = FMath::Fmod(DistanceAlongSpline, SplineLength);
		if (DistanceAlongSpline < 0.f)
		{
			DistanceAlongSpline += SplineLength;
		}
	}
	else
	{
		DistanceAlongSpline = FMath::Min(DistanceAlongSpline, SplineLength);
	}

	const FVector LocalTarget = PathSpline->GetLocationAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::Local);
	FVector TargetLocation = HomeTransform.TransformPosition(LocalTarget);
	TargetLocation.Z = SurfaceZ(TargetLocation) - SwimDepth;

	const FVector LocalDirection = PathSpline->GetDirectionAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::Local);
	const FVector Direction = HomeTransform.TransformVectorNoScale(LocalDirection);
	const float TargetYaw = Direction.SizeSquared2D() > KINDA_SMALL_NUMBER ? Direction.Rotation().Yaw : CurrentYaw;

	const float YawDelta = FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw);
	const float Alpha = FMath::Clamp(TurnResponse * Dt, 0.f, 1.f);
	CurrentYaw = CurrentYaw + YawDelta * Alpha;

	const float YawRate = Dt > KINDA_SMALL_NUMBER ? YawDelta * Alpha / Dt : 0.f;
	YawRateSmoothed = FMath::FInterpTo(YawRateSmoothed, YawRate, Dt, 3.f);
	const float Bank = FMath::Clamp(-YawRateSmoothed * BankPerYawRate, -MaxBank, MaxBank);

	SetActorLocationAndRotation(TargetLocation, FRotator(0.f, CurrentYaw, 0.f));

	// Swim cycle plus a lean into the turn - both purely visual, on the pivot only.
	const float Time = GetWorld()->GetTimeSeconds();
	const float Wag = FMath::Sin(Time * WagSpeed) * WagAngle;
	SwimPivot->SetRelativeRotation(FRotator(0.f, Wag, Bank));
}
