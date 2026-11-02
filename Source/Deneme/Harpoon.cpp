// Copyright Epic Games, Inc. All Rights Reserved.

#include "Harpoon.h"

#include "BoatPawn.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "FishCollision.h"
#include "Materials/MaterialInterface.h"
#include "Shark.h"
#include "WaterSplash.h"
#include "UObject/ConstructorHelpers.h"

AHarpoon::AHarpoon()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	// Thin placeholder shaft: 90 cm long, 4 cm across, lying along +X with the tip at the origin.
	TipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TipMesh"));
	TipMesh->SetupAttachment(Root);
	TipMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TipMesh->SetRelativeLocation(FVector(-45.0, 0.0, 0.0));
	TipMesh->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
	TipMesh->SetRelativeScale3D(FVector(0.04, 0.04, 0.9));
	if (CylinderMesh.Succeeded())
	{
		TipMesh->SetStaticMesh(CylinderMesh.Object);
	}

	RopeTie = CreateDefaultSubobject<USceneComponent>(TEXT("RopeTie"));
	RopeTie->SetupAttachment(Root);
	RopeTie->SetRelativeLocation(FVector(-88.0, 0.0, 0.0));

	InitialLifeSpan = 0.f;
}

void AHarpoon::BeginPlay()
{
	Super::BeginPlay();

	for (TActorIterator<ABoatPawn> It(GetWorld()); It; ++It)
	{
		Boat = *It;
		break;
	}

	UStaticMesh* RopeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	for (int32 Index = 0; Index < RopeSegments; ++Index)
	{
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
		Piece->SetStaticMesh(RopeMesh);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetCastShadow(false);
		if (RopeMaterial)
		{
			Piece->SetMaterial(0, RopeMaterial);
		}
		Piece->SetupAttachment(Root);
		Piece->SetAbsolute(true, true, true);
		Piece->RegisterComponent();
		Piece->SetVisibility(false);
		RopePieces.Add(Piece);
	}

	UStaticMesh* DropMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	for (int32 Index = 0; Index < SprayPoolSize; ++Index)
	{
		UStaticMeshComponent* Drop = NewObject<UStaticMeshComponent>(this);
		Drop->SetStaticMesh(DropMesh);
		Drop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Drop->SetCastShadow(false);
		if (SprayMaterial)
		{
			Drop->SetMaterial(0, SprayMaterial);
		}
		Drop->SetupAttachment(Root);
		Drop->SetAbsolute(true, true, true);
		Drop->RegisterComponent();
		Drop->SetVisibility(false);
		FSprayDrop& Entry = Spray.AddDefaulted_GetRef();
		Entry.Mesh = Drop;
	}

	if (Boat && Boat->GetBarrelMesh())
	{
		BarrelRelative = Boat->GetBarrelMesh()->GetRelativeTransform();
	}
}

void AHarpoon::Launch(const FVector& Direction)
{
	Velocity = Direction.GetSafeNormal() * LaunchSpeed;
	SetActorRotation(Velocity.Rotation());
	Phase = EPhase::Flying;
	PhaseTime = 0.f;

	if (Boat && Boat->GetBarrelRopePoint())
	{
		const float Distance = FVector::Distance(Boat->GetBarrelRopePoint()->GetComponentLocation(), RopeTie->GetComponentLocation());
		const float Slack = FMath::Max(RopeLength - Distance, 0.f);
		Sag = FMath::Min(MaxSag, FMath::Sqrt(3.f * FMath::Max(Distance, 1.f) * Slack / 8.f));
	}
	for (UStaticMeshComponent* Piece : RopePieces)
	{
		Piece->SetVisibility(true);
	}
}

float AHarpoon::SurfaceZAt(const FVector& Location) const
{
	return Boat ? Boat->GetSurfaceZ(FVector2D(Location.X, Location.Y)) : 0.f;
}

void AHarpoon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Dt = FMath::Clamp(DeltaSeconds, 0.001f, 0.05f);
	if (!Boat || !Boat->GetBarrelMesh())
	{
		Destroy();
		return;
	}

	PhaseTime += Dt;
	switch (Phase)
	{
	case EPhase::Flying:
		TickFlight(Dt);
		break;
	case EPhase::Stuck:
		TickStuck(Dt);
		break;
	case EPhase::Returning:
		TickReturn(Dt);
		break;
	}

	if (!IsActorBeingDestroyed() && Phase != EPhase::Returning)
	{
		UpdateRope(Dt);
	}
	UpdateSpray(Dt);
}

// ---------------------------------------------------------------- spray
void AHarpoon::EmitSpray(const FVector& Origin, const FVector& InitialVelocity, int32 Count, float Spread, float UpSpeed)
{
	for (int32 Emitted = 0; Emitted < Count; ++Emitted)
	{
		FSprayDrop* Free = Spray.FindByPredicate([](const FSprayDrop& Drop) { return !Drop.bActive; });
		if (!Free)
		{
			return;
		}
		const FVector Side = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal() * FMath::FRandRange(0.3f, 1.f) * Spread;
		Free->Velocity = InitialVelocity + Side + FVector(0.f, 0.f, UpSpeed * FMath::FRandRange(0.6f, 1.2f));
		Free->Age = 0.f;
		Free->Life = SprayLife * FMath::FRandRange(0.7f, 1.2f);
		Free->Size = SprayDropSize * FMath::FRandRange(0.5f, 1.2f);
		Free->bActive = true;
		Free->Mesh->SetWorldLocation(Origin + FVector(FMath::FRandRange(-8.f, 8.f), FMath::FRandRange(-8.f, 8.f), 0.f));
		Free->Mesh->SetVisibility(true);
	}
}

void AHarpoon::UpdateSpray(float Dt)
{
	for (FSprayDrop& Drop : Spray)
	{
		if (!Drop.bActive)
		{
			continue;
		}
		Drop.Age += Dt;
		if (Drop.Age >= Drop.Life)
		{
			Drop.bActive = false;
			Drop.Mesh->SetVisibility(false);
			continue;
		}

		Drop.Velocity.Z -= 980.f * Dt;
		const FVector Location = Drop.Mesh->GetComponentLocation() + Drop.Velocity * Dt;
		Drop.Mesh->SetWorldLocation(Location);

		// Round and bright at first, then it shrinks away; a drop that falls back into the water is gone.
		const float Alpha = Drop.Age / Drop.Life;
		const float Scale = Drop.Size / 100.f * (1.f - Alpha * Alpha);
		Drop.Mesh->SetWorldScale3D(FVector(Scale, Scale, Scale * (1.f + FMath::Clamp(Drop.Velocity.Z / 600.f, 0.f, 0.6f))));
		if (Drop.Velocity.Z < 0.f && Location.Z < SurfaceZAt(Location))
		{
			Drop.bActive = false;
			Drop.Mesh->SetVisibility(false);
		}
	}
}

// ---------------------------------------------------------------- flight
void AHarpoon::TickFlight(float Dt)
{
	const FVector From = GetActorLocation();
	const bool bUnderwater = From.Z < SurfaceZAt(From);

	Velocity.Z -= 980.f * GravityScale * Dt;
	if (bUnderwater)
	{
		if (WaterTime <= 0.f && WaterSplashScale > 0.f)
		{
			// The moment it goes in: a splash, a little bigger than a bullet's.
			const FTransform SplashTransform(FRotator::ZeroRotator, From);
			if (AWaterSplash* Splash = GetWorld()->SpawnActorDeferred<AWaterSplash>(AWaterSplash::StaticClass(), SplashTransform))
			{
				Splash->Scale = WaterSplashScale;
				Splash->FinishSpawning(SplashTransform);
			}
		}
		Velocity *= FMath::Exp(-WaterDrag * Dt);
		WaterTime += Dt;
	}

	FVector To = From + Velocity * Dt;

	// Did it reach the shark on the way?
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HarpoonSweep), false, this);
	Params.AddIgnoredActor(GetOwner());
	if (GetWorld()->SweepSingleByChannel(Hit, From, To, FQuat::Identity, ECC_Shark, FCollisionShape::MakeSphere(HitRadius), Params))
	{
		if (AShark* Shark = Cast<AShark>(Hit.GetActor()))
		{
			StickInto(Shark, Hit);
			return;
		}
	}

	// The rope is the leash: the harpoon cannot fly farther than it allows.
	const FVector RopeStart = Boat->GetBarrelRopePoint()->GetComponentLocation();
	const FVector Offset = To - RopeStart;
	if (Offset.SizeSquared() > FMath::Square(RopeLength))
	{
		const FVector Out = Offset.GetSafeNormal();
		To = RopeStart + Out * RopeLength;
		Velocity -= Out * FMath::Max(0.f, FVector::DotProduct(Velocity, Out));
		Velocity *= FMath::Exp(-3.f * Dt);
	}

	SetActorLocation(To);
	if (Velocity.SizeSquared() > 100.f * 100.f)
	{
		SetActorRotation(Velocity.Rotation());
	}

	if (PhaseTime > MaxFlightTime || WaterTime > SinkTime)
	{
		Finish();
	}
}

void AHarpoon::StickInto(AShark* Shark, const FHitResult& Hit)
{
	const FVector Direction = Velocity.GetSafeNormal();
	Phase = EPhase::Stuck;
	PhaseTime = 0.f;
	Velocity = FVector::ZeroVector;
	StuckShark = Shark;
	bHitShark = true;

	// Tip in the skin plus a little deeper, then ride along on the shark.
	SetActorLocationAndRotation(Hit.ImpactPoint + Direction * StickDepth, Direction.Rotation());
	AttachToComponent(Shark->GetStickAnchor(), FAttachmentTransformRules::KeepWorldTransform);

	Shark->NotifyPlayerHit(Hit.ImpactPoint);
	OnHitShark.Broadcast(Shark);
}

// ---------------------------------------------------------------- stuck: barrel and rope
void AHarpoon::TickStuck(float Dt)
{
	if (!StuckShark || (AutoReleaseTime > 0.f && PhaseTime > AutoReleaseTime))
	{
		Release();
		return;
	}
	DriveBarrel(Dt);
}

FVector AHarpoon::ConstrainBarrel(const FVector& Position, const FVector& Tip, bool& bOutTaut) const
{
	// Only ever called once the harpoon is stuck (from DriveBarrel, which TickStuck alone drives), so the
	// shorter post-stick leash applies here, not the throw's RopeLength.
	const FVector Offset = Position - Tip;
	const float Distance = Offset.Size();
	bOutTaut = Distance >= StuckRopeLength - 0.5f;
	if (Distance <= StuckRopeLength)
	{
		return Position;
	}

	// Keep the height, shorten the horizontal distance until the rope length fits.
	const float Dz = Offset.Z;
	const float Reach2 = FMath::Square(StuckRopeLength) - Dz * Dz;
	const FVector2D Flat = FVector2D(Offset.X, Offset.Y).GetSafeNormal();
	if (Reach2 <= 0.f || Flat.IsNearlyZero())
	{
		return Tip + Offset / Distance * StuckRopeLength;
	}
	const float Reach = FMath::Sqrt(Reach2);
	return FVector(Tip.X + Flat.X * Reach, Tip.Y + Flat.Y * Reach, Position.Z);
}

void AHarpoon::DetachBarrel()
{
	UStaticMeshComponent* Barrel = Boat->GetBarrelMesh();
	Barrel->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	BarrelBaseRotation = FQuat(FRotator(0.0, Barrel->GetComponentRotation().Yaw, 0.0));
	bBarrelFree = true;
}

void AHarpoon::ReattachBarrel()
{
	UStaticMeshComponent* Barrel = Boat ? Boat->GetBarrelMesh() : nullptr;
	if (!Barrel || !Boat->GetBarrelHome())
	{
		return;
	}
	Barrel->AttachToComponent(Boat->GetBarrelHome(), FAttachmentTransformRules::KeepRelativeTransform);
	Barrel->SetRelativeTransform(BarrelRelative);
	bBarrelFree = false;
	BarrelState = EBarrelState::OnDeck;
	BarrelVelocity = FVector::ZeroVector;
}

void AHarpoon::DriveBarrel(float Dt)
{
	UStaticMeshComponent* Barrel = Boat->GetBarrelMesh();
	const FVector Tip = RopeTie->GetComponentLocation();
	const FVector Position = Barrel->GetComponentLocation();
	bool bTaut = false;

	if (BarrelState == EBarrelState::OnDeck)
	{
		// Slack: it sits where it is. Taut: the rope drags it across the deck.
		const FVector Wanted = ConstrainBarrel(Position, Tip, bTaut);
		if (bTaut)
		{
			Barrel->SetWorldLocation(Wanted);
		}

		const FVector Home = Boat->GetBarrelHome()->GetComponentLocation();
		if (FVector::Dist2D(Wanted, Home) > DeckEdgeDistance)
		{
			// Over the side.
			BarrelVelocity = ((Wanted - Position) / Dt).GetClampedToMaxSize2D(MaxBarrelSpeed);
			BarrelVelocity.Z = LeaveDeckHop;
			DetachBarrel();
			BarrelState = EBarrelState::Falling;
		}
	}
	else
	{
		FVector New = Position;
		const float Surface = SurfaceZAt(Position) + FloatHeight;

		if (BarrelState == EBarrelState::Falling)
		{
			BarrelVelocity.Z -= BarrelGravity * Dt;
			BarrelVelocity.X *= FMath::Exp(-0.3f * Dt);
			BarrelVelocity.Y *= FMath::Exp(-0.3f * Dt);
			New += BarrelVelocity * Dt;
		}
		else
		{
			const float Drag = FMath::Exp(-BarrelWaterDrag * Dt);
			BarrelVelocity.X *= Drag;
			BarrelVelocity.Y *= Drag;
			BarrelVelocity.Z = 0.f;
			New += BarrelVelocity * Dt;
			New.Z = FMath::FInterpTo(Position.Z, Surface, Dt, BuoyancyResponse);
		}

		const FVector Constrained = ConstrainBarrel(New, Tip, bTaut);
		if (bTaut)
		{
			// Inelastic: the barrel just goes where the rope lets it. Nothing springs back.
			BarrelVelocity.X = (Constrained.X - Position.X) / Dt;
			BarrelVelocity.Y = (Constrained.Y - Position.Y) / Dt;
			const FVector Flat = BarrelVelocity.GetClampedToMaxSize2D(MaxBarrelSpeed);
			BarrelVelocity.X = Flat.X;
			BarrelVelocity.Y = Flat.Y;
		}
		New = Constrained;

		if (BarrelState == EBarrelState::Falling && New.Z <= Surface && BarrelVelocity.Z < 0.f)
		{
			BarrelState = EBarrelState::Floating;
			BarrelVelocity *= 0.5f;
			if (bBarrelWake && SplashWidth > 0.f)
			{
				// Splash: a round patch of foam where it lands.
				const FVector Flat(0.f, 1.f, 0.f);
				Boat->AddWakeSegment(New - Flat * 4.f, New + Flat * 4.f, SplashWidth, 1.f);
				EmitSpray(FVector(New.X, New.Y, Surface), FVector::ZeroVector, SprayPoolSize / 2 + 6, 320.f, 520.f);
			}
		}

		if (BarrelState == EBarrelState::Floating && bBarrelWake)
		{
			PaintBarrelWake(Position, New, Dt);
		}

		// A tight rope tips the barrel toward the shark.
		Tension = FMath::FInterpTo(Tension, bTaut && BarrelState == EBarrelState::Floating ? 1.f : 0.f, Dt, 4.f);
		FQuat Rotation = BarrelBaseRotation;
		const FVector2D ToTip = FVector2D(Tip.X - New.X, Tip.Y - New.Y).GetSafeNormal();
		if (!ToTip.IsNearlyZero() && Tension > KINDA_SMALL_NUMBER)
		{
			const FVector Axis = FVector::CrossProduct(FVector::UpVector, FVector(ToTip.X, ToTip.Y, 0.f));
			Rotation = FQuat(Axis.GetSafeNormal(), FMath::DegreesToRadians(LeanAngle * Tension)) * BarrelBaseRotation;
		}
		Barrel->SetWorldLocationAndRotation(New, Rotation);
	}
}

// ---------------------------------------------------------------- barrel wake
void AHarpoon::PaintBarrelWake(const FVector& From, const FVector& To, float Dt)
{
	const FVector Delta = FVector(To.X - From.X, To.Y - From.Y, 0.f);
	const float Distance = Delta.Size();
	const float Speed = Distance / Dt;
	if (Speed < WakeMinSpeed || Distance < KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector Direction = Delta / Distance;
	const FVector Right(-Direction.Y, Direction.X, 0.f);
	const float Strength = FMath::Clamp(Speed / WakeFullSpeed, 0.f, 1.f);

	PrevWakeTravel = WakeTravel;
	WakeTravel += Distance;

	// Three streaks behind the barrel that swell and drift a little, so they do not look ruled.
	const FVector Stern = Direction * -22.f;
	for (int32 Index = -1; Index <= 1; ++Index)
	{
		const float Seed = Index * 7.3f;
		const auto Side = [&](float Travel)
		{
			return Index * WakeSpread + FMath::PerlinNoise1D(Travel * 0.004f + Seed + 100.f) * 7.f;
		};
		const float Noise = FMath::PerlinNoise1D(WakeTravel * 0.006f + Seed);
		const float Width = WakeWidth * (Index == 0 ? 1.f : 0.6f) * (1.f + Noise * 0.4f);
		Boat->AddWakeSegment(From + Stern + Right * Side(PrevWakeTravel), To + Stern + Right * Side(WakeTravel),
			Width, Strength * (Index == 0 ? 0.95f : 0.75f));
	}

	// A foam patch round the body: the barrel is cutting through the water.
	if (BowFoamWidth > 0.f)
	{
		Boat->AddWakeSegment(From, To, BowFoamWidth, Strength * 0.55f);
	}

	// Dashed rails on both sides, wider apart the faster it goes: a cartoon bow wave.
	if (RailWidth > 0.f && FMath::Fmod(WakeTravel, DashLength) < DashLength * DashDuty)
	{
		const float Offset = FMath::Lerp(RailOffset.X, RailOffset.Y, Strength);
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const FVector Lateral = Right * (Side * Offset);
			Boat->AddWakeSegment(From + Lateral, To + Lateral, RailWidth, Strength);
		}
	}

	// Foam bubbles of random size scattered in the wake.
	BubbleAccum += BubbleRate * Strength * Dt;
	for (; BubbleAccum >= 1.f; BubbleAccum -= 1.f)
	{
		const FVector Spot = To - Direction * FMath::FRandRange(25.f, 190.f) + Right * FMath::FRandRange(-60.f, 60.f);
		Boat->AddWakeSegment(Spot, Spot + Direction * 3.f, FMath::FRandRange(5.f, 17.f), FMath::FRandRange(0.7f, 1.f));
	}

	// Drops thrown up and forward from the nose.
	SprayAccum += SprayRate * Strength * Dt;
	const int32 Drops = FMath::FloorToInt(SprayAccum);
	SprayAccum -= Drops;
	if (Drops > 0)
	{
		EmitSpray(FVector(To.X, To.Y, SurfaceZAt(To) + 4.f) + Direction * 24.f, Direction * (Speed * 0.3f), Drops, 90.f, 150.f + 220.f * Strength);
	}
}

// ---------------------------------------------------------------- rope
void AHarpoon::UpdateRope(float Dt)
{
	if (RopePieces.Num() == 0 || !Boat->GetBarrelRopePoint())
	{
		return;
	}

	const FVector A = Boat->GetBarrelRopePoint()->GetComponentLocation();
	const FVector B = RopeTie->GetComponentLocation();
	const float Distance = FVector::Dist(A, B);

	// A loose rope hangs in a parabola whose depth follows the slack. The depth changes smoothly and never
	// overshoots, so the rope settles instead of swinging. Whichever length is actually in play right now -
	// the throw's reach while it is flying, the shorter post-stick leash once it has hit - is what the slack
	// is measured against, so the rope never LOOKS longer than the barrel is actually allowed to be dragged.
	const float ActiveRopeLength = Phase == EPhase::Stuck ? StuckRopeLength : RopeLength;
	const float Slack = FMath::Max(ActiveRopeLength - Distance, 0.f);
	const float TargetSag = FMath::Min(MaxSag, FMath::Sqrt(3.f * FMath::Max(Distance, 1.f) * Slack / 8.f));
	Sag = FMath::FInterpTo(Sag, TargetSag, Dt, SagResponse);

	const int32 Count = RopePieces.Num();
	const float Thickness = RopeThickness / 100.f;
	FVector Previous = A;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float T = static_cast<float>(Index + 1) / static_cast<float>(Count);
		const FVector Next = FMath::Lerp(A, B, T) - FVector(0.f, 0.f, Sag * 4.f * T * (1.f - T));

		const FVector Delta = Next - Previous;
		const float Length = Delta.Size();
		if (Length > KINDA_SMALL_NUMBER)
		{
			const FRotator Rotation = FRotationMatrix::MakeFromZ(Delta / Length).Rotator();
			RopePieces[Index]->SetWorldTransform(FTransform(Rotation, (Previous + Next) * 0.5f, FVector(Thickness, Thickness, Length * 1.03f / 100.f)));
		}
		Previous = Next;
	}
}

// ---------------------------------------------------------------- ending
void AHarpoon::Release()
{
	if (Phase == EPhase::Returning)
	{
		return;
	}
	if (Phase == EPhase::Flying || !bBarrelFree)
	{
		Finish();
		return;
	}

	Phase = EPhase::Returning;
	ReturnAlpha = 0.f;
	BarrelStart = Boat->GetBarrelMesh()->GetComponentLocation();
	BarrelBaseRotation = Boat->GetBarrelMesh()->GetComponentQuat();

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	TipMesh->SetVisibility(false);
	for (UStaticMeshComponent* Piece : RopePieces)
	{
		Piece->SetVisibility(false);
	}
}

void AHarpoon::TickReturn(float Dt)
{
	ReturnAlpha += Dt / ReturnTime;
	const float Alpha = FMath::Clamp(ReturnAlpha, 0.f, 1.f);
	const float Eased = FMath::SmoothStep(0.f, 1.f, Alpha);

	const FTransform Home = BarrelRelative * Boat->GetBarrelHome()->GetComponentTransform();
	FVector Position = FMath::Lerp(BarrelStart, Home.GetLocation(), Eased);
	Position.Z += FMath::Sin(Eased * PI) * ReturnHop;
	Boat->GetBarrelMesh()->SetWorldLocationAndRotation(Position, FQuat::Slerp(BarrelBaseRotation, Home.GetRotation(), Eased));

	if (ReturnAlpha >= 1.f)
	{
		Finish();
	}
}

void AHarpoon::Finish()
{
	if (bBarrelFree)
	{
		ReattachBarrel();
	}
	OnFinished.Broadcast();
	Destroy();
}
