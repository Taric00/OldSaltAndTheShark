// Copyright Epic Games, Inc. All Rights Reserved.

#include "Shark.h"

#include "BoatPawn.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "FishCollision.h"
#include "UObject/ConstructorHelpers.h"

AShark::AShark()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	// The hit box is what the player's trace sees (generous, so the shot is forgiving). Sized and placed to
	// wrap the real body mesh (304 cm nose to tail, pivot a little below and behind its visual centre).
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	HitBox = CreateDefaultSubobject<UCapsuleComponent>(TEXT("HitBox"));
	HitBox->SetupAttachment(Root);
	HitBox->InitCapsuleSize(55.f, 155.f);
	HitBox->SetRelativeLocation(FVector(4.0, 0.0, 18.0));
	HitBox->SetRelativeRotation(FRotator(90.0, 0.0, 0.0)); // lie along the shark forward axis
	HitBox->SetCollisionObjectType(ECC_WorldDynamic);
	HitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitBox->SetCollisionResponseToChannel(ECC_Shark, ECR_Block);
	HitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

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

void AShark::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	if (!TargetBoat)
	{
		for (TActorIterator<ABoatPawn> It(GetWorld()); It; ++It)
		{
			TargetBoat = *It;
			break;
		}
	}

	OrbitDirection = FMath::RandBool() ? 1.f : -1.f;
	if (TargetBoat)
	{
		// Start already deep and on the circle.
		const FVector Center = TargetBoat->GetActorLocation();
		SetActorLocation(OrbitPoint(GetActorLocation(), Center, OrbitRadius, SwimDepth));
	}
	SetState(ESharkState::Circling);
}

float AShark::SurfaceZ(const FVector& Location) const
{
	return TargetBoat ? TargetBoat->GetSurfaceZ(FVector2D(Location.X, Location.Y)) : 0.f;
}

FVector AShark::OrbitPoint(const FVector& Location, const FVector& Center, float Radius, float Depth) const
{
	const FVector2D Relative(Location.X - Center.X, Location.Y - Center.Y);
	const float Angle = FMath::Atan2(Relative.Y, Relative.X);

	// Aim a little ahead along the circle so the shark keeps swimming round it.
	const float Lead = FMath::Clamp(CircleSpeed * 0.7f / FMath::Max(Radius, 1.f), 0.2f, 0.9f);
	const float TargetAngle = Angle + OrbitDirection * Lead;

	FVector Point(Center.X + FMath::Cos(TargetAngle) * Radius, Center.Y + FMath::Sin(TargetAngle) * Radius, 0.f);
	Point.Z = SurfaceZ(Point) - Depth;
	return Point;
}

void AShark::SetState(ESharkState NewState)
{
	const ESharkState OldState = State;
	State = NewState;
	StateTime = 0.f;

	if (NewState == ESharkState::Circling)
	{
		WaitTime = FMath::FRandRange(AttackDelayMin, FMath::Max(AttackDelayMin, AttackDelayMax));
		OrbitDirection = FMath::RandBool() ? 1.f : -1.f;
	}

	OnStateChanged.Broadcast(NewState, OldState);
}

void AShark::NotifyPlayerHit(const FVector& ImpactPoint)
{
	// A harpoon sticking in it always drives it off, wherever it was in its loop (even just circling deep).
	if (IsDead())
	{
		return;
	}

	// Driven off: dive at once and back off to a far circle to recover.
	bRetreatFar = true;
	bPowered = false;
	Velocity.Z -= 450.f;
	OnHitByPlayer.Broadcast();
	SetState(ESharkState::Retreating);
}

void AShark::ApplyRifleHit(float Damage, const FVector& ImpactPoint, const FVector& ShotDirection)
{
	if (IsDead() || Damage <= 0.f)
	{
		return;
	}

	Health = FMath::Max(0.f, Health - Damage);
	Velocity += ShotDirection.GetSafeNormal() * BulletKick;
	Twitch = 1.f;
	OnDamaged.Broadcast(GetHealth01());

	if (Health <= 0.f)
	{
		DeathStartRotation = GetActorRotation();
		SetState(ESharkState::Dead);
		OnDied.Broadcast();
	}
}

void AShark::TickDead(float Dt)
{
	// Slows down, rolls belly up and drifts to just under the surface.
	const float T = FMath::Clamp(StateTime / DeathRiseTime, 0.f, 1.f);
	const float Eased = FMath::SmoothStep(0.f, 1.f, T);

	Velocity = FMath::VInterpTo(Velocity, FVector::ZeroVector, Dt, 1.2f);
	FVector Location = GetActorLocation() + Velocity * Dt;
	Location.Z = FMath::FInterpTo(Location.Z, SurfaceZ(Location) - 25.f, Dt, 0.8f);
	SetActorLocation(Location);

	FRotator Rotation = DeathStartRotation;
	Rotation.Pitch = FMath::Lerp(DeathStartRotation.Pitch, 0.f, Eased);
	Rotation.Roll = FMath::Lerp(DeathStartRotation.Roll, 180.f, Eased);
	SetActorRotation(Rotation);

	// The tail gives a few last, slowing flicks.
	const float Wag = FMath::Sin(GetWorld()->GetTimeSeconds() * WagSpeed * 0.6f) * WagAngle * (1.f - Eased);
	SwimPivot->SetRelativeRotation(FRotator(0.f, Wag, 0.f));
}

void AShark::Strike()
{
	if (TargetBoat)
	{
		const FVector Toward = (TargetBoat->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		const float Side = FVector::DotProduct(TargetBoat->GetActorRightVector(), Toward);
		TargetBoat->ApplyImpulse(Toward * StrikeImpulse, -Side * StrikeRollKick, StrikePitchKick);
		TargetBoat->ApplyHullDamage(StrikeDamage);
	}

	// The strike spends the power.
	bRetreatFar = false;
	bPowered = false;
	OnStrikeBoat.Broadcast();
	SetState(ESharkState::Retreating);
}

void AShark::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Dt = FMath::Clamp(DeltaSeconds, 0.001f, 0.05f);
	if (!TargetBoat)
	{
		return;
	}

	StateTime += Dt;

	if (State == ESharkState::Dead)
	{
		TickDead(Dt);
		return;
	}

	const FVector Location = GetActorLocation();
	const FVector Center = TargetBoat->GetActorLocation();
	const float BoatSpeed = TargetBoat->GetBoatVelocity().Size2D();
	const float RadialDistance = FVector2D::Distance(FVector2D(Location.X, Location.Y), FVector2D(Center.X, Center.Y));

	FVector Target = Location;
	float Speed = CircleSpeed;

	switch (State)
	{
	case ESharkState::Circling:
		Target = OrbitPoint(Location, Center, OrbitRadius, SwimDepth);
		Speed = CircleSpeed + BoatSpeed;
		if (StateTime >= WaitTime)
		{
			SetState(ESharkState::Stalking);
		}
		break;

	case ESharkState::Stalking:
		Target = OrbitPoint(Location, Center, OrbitRadius, StalkDepth);
		Speed = CircleSpeed * 1.15f + BoatSpeed;
		if (StateTime >= StalkTime)
		{
			SetState(ESharkState::Attacking);
		}
		break;

	case ESharkState::Attacking:
	{
		// Straight at the boat, rising to the surface as it closes in.
		const float T = FMath::Clamp(RadialDistance / SurfaceRange, 0.f, 1.f);
		Target = FVector(Center.X, Center.Y, 0.f);
		Target.Z = SurfaceZ(Target) + FMath::Lerp(BreachHeight, -StalkDepth, T);
		Speed = AttackSpeed * (bPowered ? PoweredAttackMultiplier : 1.f) + BoatSpeed;

		const FVector Nose = Location + GetActorForwardVector() * 230.f;
		const FBox HullBounds = TargetBoat->GetRootComponent()->Bounds.GetBox();
		if (HullBounds.ComputeSquaredDistanceToPoint(Nose) <= FMath::Square(StrikeDistance))
		{
			Strike();
		}
		break;
	}

	case ESharkState::Retreating:
	{
		const float Radius = bRetreatFar ? FarOrbitRadius : OrbitRadius;
		Target = OrbitPoint(Location, Center, Radius, SwimDepth * 1.3f);
		Speed = RetreatSpeed + BoatSpeed;

		const bool bDeepEnough = Location.Z < SurfaceZ(Location) - SwimDepth * 0.7f;
		const bool bOnCircle = FMath::Abs(RadialDistance - Radius) < 250.f;
		if (StateTime > 0.8f && bDeepEnough && bOnCircle)
		{
			if (bRetreatFar)
			{
				SetState(ESharkState::Recovering);
			}
			else
			{
				SetState(ESharkState::Circling);
			}
		}
		break;
	}

	case ESharkState::Recovering:
		Target = OrbitPoint(Location, Center, FarOrbitRadius, SwimDepth);
		Speed = CircleSpeed + BoatSpeed;
		if (StateTime >= ChargeTime)
		{
			bPowered = true;
			bRetreatFar = false;
			SetState(ESharkState::Stalking);
		}
		break;

	case ESharkState::Dead:
		break;
	}

	// Steer: the velocity chases the wanted one, so turns are wide and lazy.
	const FVector Desired = (Target - Location).GetSafeNormal() * Speed;
	Velocity = FMath::VInterpTo(Velocity, Desired, Dt, Agility);
	Velocity = Velocity.GetClampedToMaxSize(Speed * 1.25f);

	SetActorLocation(Location + Velocity * Dt);

	if (Velocity.SizeSquared() > 25.f * 25.f)
	{
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Velocity.Rotation(), Dt, TurnResponse));
	}

	// Swim cycle, plus a quick hard twitch after a bullet.
	Twitch = FMath::FInterpTo(Twitch, 0.f, Dt, 5.f);
	const float Time = GetWorld()->GetTimeSeconds();
	const float Wag = FMath::Sin(Time * WagSpeed) * WagAngle + FMath::Sin(Time * 38.f) * HitTwitch * Twitch;
	SwimPivot->SetRelativeRotation(FRotator(0.f, Wag, 0.f));

	if (bShowDebug && GEngine)
	{
		const UEnum* StateEnum = StaticEnum<ESharkState>();
		GEngine->AddOnScreenDebugMessage(
			reinterpret_cast<uint64>(this), 0.f, FColor::Orange,
			FString::Printf(TEXT("Shark: %s  t=%.1f/%.1f  radius %.0f  z %.0f"),
				*StateEnum->GetNameStringByValue(static_cast<int64>(State)), StateTime, WaitTime, RadialDistance, Location.Z));
	}
}
