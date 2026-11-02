// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterSplash.h"

#include "BoatPawn.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AWaterSplash::AWaterSplash()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SprayMaterial(TEXT("/Game/Materials/M_Spray.M_Spray"));
	if (SprayMaterial.Succeeded())
	{
		Material = SprayMaterial.Object;
	}
}

void AWaterSplash::BeginPlay()
{
	Super::BeginPlay();

	for (TActorIterator<ABoatPawn> It(GetWorld()); It; ++It)
	{
		Boat = *It;
		break;
	}

	const FVector Origin = GetActorLocation();
	WaterZ = Boat ? Boat->GetSurfaceZ(FVector2D(Origin.X, Origin.Y)) : Origin.Z;

	const auto MakePiece = [this](UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
		Piece->SetStaticMesh(Mesh);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetCastShadow(false);
		if (Material)
		{
			Piece->SetMaterial(0, Material);
		}
		Piece->SetupAttachment(RootComponent);
		Piece->SetAbsolute(true, true, true);
		Piece->RegisterComponent();
		return Piece;
	};

	// Spout: a cylinder standing on the water.
	if (SpoutHeight > 0.f)
	{
		Spout = MakePiece(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		Spout->SetVisibility(false);
	}

	// Drops: thrown up and outward, a little random each.
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	for (int32 Index = 0; Index < DropCount; ++Index)
	{
		FDrop& Drop = Drops.AddDefaulted_GetRef();
		Drop.Mesh = MakePiece(Sphere);
		const float Angle = (Index + FMath::FRand() * 0.6f) / FMath::Max(DropCount, 1) * 2.f * PI;
		const FVector Side(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		Drop.Velocity = (Side * DropSpread * FMath::FRandRange(0.4f, 1.1f)
			+ FVector(0.f, 0.f, FMath::FRandRange(DropUpSpeed.X, DropUpSpeed.Y))) * FMath::Sqrt(Scale);
		Drop.Size = DropSize * Scale * FMath::FRandRange(0.6f, 1.3f);
		Drop.Life = DropLife * FMath::FRandRange(0.75f, 1.2f);
		Drop.bActive = true;
		Drop.Mesh->SetWorldLocation(FVector(Origin.X, Origin.Y, WaterZ) + Side * 3.f * Scale);
		Drop.Mesh->SetWorldScale3D(FVector(Drop.Size / 100.f));
	}

	PaintStar();

	float LastRing = 0.f;
	for (const float Delay : RingDelays)
	{
		LastRing = FMath::Max(LastRing, Delay);
	}
	Duration = FMath::Max3(SpoutTime, DropLife * 1.25f, LastRing) + 0.1f;
}

void AWaterSplash::PaintStar() const
{
	if (!Boat || StarSize <= 0.f)
	{
		return;
	}
	// Three short crossing strokes: a little round-ish burst instead of a square.
	const FVector Center = GetActorLocation();
	const float Half = StarSize * Scale * 0.5f;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const float Angle = Index * PI / 3.f + 0.4f;
		const FVector Arm(FMath::Cos(Angle) * Half, FMath::Sin(Angle) * Half, 0.f);
		Boat->AddWakeSegment(Center - Arm, Center + Arm, StarSize * Scale * 0.4f, 1.f);
	}
}

void AWaterSplash::PaintRing(float Radius, float Strength) const
{
	if (!Boat)
	{
		return;
	}
	const FVector Center = GetActorLocation();
	const int32 Segments = 18;
	FVector Previous = Center + FVector(Radius, 0.f, 0.f);
	for (int32 Index = 1; Index <= Segments; ++Index)
	{
		const float Angle = Index * 2.f * PI / Segments;
		const FVector Next = Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
		Boat->AddWakeSegment(Previous, Next, RingWidth * Scale, Strength);
		Previous = Next;
	}
}

void AWaterSplash::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Dt = FMath::Clamp(DeltaSeconds, 0.001f, 0.05f);
	Age += Dt;
	const FVector Origin = GetActorLocation();

	// Rings, each when its time comes.
	while (RingsPainted < RingRadii.Num() && RingsPainted < RingDelays.Num() && Age >= RingDelays[RingsPainted])
	{
		const float Fade = 1.f - 0.18f * RingsPainted;
		PaintRing(RingRadii[RingsPainted] * Scale, RingStrength * Fade);
		++RingsPainted;
	}

	// Spout: shoots up fast, then sinks back and thins.
	if (Spout)
	{
		const float T = Age / SpoutTime;
		if (T < 1.f)
		{
			const float Height = SpoutHeight * Scale * FMath::Sin(FMath::Pow(T, 0.6f) * PI);
			const float Width = SpoutWidth * Scale * (1.f - 0.6f * T) / 100.f;
			Spout->SetWorldLocationAndRotation(FVector(Origin.X, Origin.Y, WaterZ + Height * 0.5f), FRotator::ZeroRotator);
			Spout->SetWorldScale3D(FVector(Width, Width, FMath::Max(Height, 0.1f) / 100.f));
			Spout->SetVisibility(Height > 1.f);
		}
		else
		{
			Spout->SetVisibility(false);
		}
	}

	// Drops: fly, shrink, vanish when they fall back into the water.
	for (FDrop& Drop : Drops)
	{
		if (!Drop.bActive)
		{
			continue;
		}
		Drop.Velocity.Z -= 980.f * Dt;
		const FVector Location = Drop.Mesh->GetComponentLocation() + Drop.Velocity * Dt;
		Drop.Mesh->SetWorldLocation(Location);

		const float Alpha = Age / Drop.Life;
		if (Alpha >= 1.f || (Drop.Velocity.Z < 0.f && Location.Z < WaterZ))
		{
			Drop.bActive = false;
			Drop.Mesh->SetVisibility(false);
			continue;
		}
		const float Size = Drop.Size / 100.f * (1.f - Alpha * Alpha);
		Drop.Mesh->SetWorldScale3D(FVector(Size, Size, Size * (1.f + FMath::Clamp(FMath::Abs(Drop.Velocity.Z) / 500.f, 0.f, 0.8f))));
	}

	if (Age >= Duration)
	{
		Destroy();
	}
}
