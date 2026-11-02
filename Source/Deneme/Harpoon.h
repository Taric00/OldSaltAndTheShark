// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Harpoon.generated.h"

class ABoatPawn;
class AShark;
class UStaticMesh;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHarpoonHitShark, AShark*, Shark);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHarpoonEvent);

/**
 * A thrown harpoon, the rope behind it and the barrel on the other end of that rope.
 *
 * Flight: the harpoon flies on a flat arc, tied to the boat's barrel. The rope limits how far it can go.
 * Hit:    the harpoon sticks in the shark (it rides along on the shark) and the rope pulls the barrel off the deck.
 *         The barrel then floats on the water above the shark, so the shark can be tracked even when it is deep.
 * Miss:   it sinks and the rope is gone again.
 *
 * The rope has no simulation of its own that could jiggle: the barrel is held by a hard length limit
 * (position projection, nothing bounces back) and the rope shape is a smoothly changing sag curve.
 * Everything that can be tuned is a property below; make a Blueprint child (BP_Harpoon) to change them.
 */
UCLASS()
class DENEME_API AHarpoon : public AActor
{
	GENERATED_BODY()

public:
	AHarpoon();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Throw it. Direction is a unit vector, the speed comes from LaunchSpeed. */
	UFUNCTION(BlueprintCallable, Category = "Harpoon")
	void Launch(const FVector& Direction);

	/** Cut the rope: the harpoon is gone and the barrel goes back to the boat. */
	UFUNCTION(BlueprintCallable, Category = "Harpoon")
	void Release();

	UFUNCTION(BlueprintPure, Category = "Harpoon")
	bool IsStuck() const { return Phase == EPhase::Stuck; }

	/** True once the harpoon went into a shark (stays true after the rope is cut). */
	UFUNCTION(BlueprintPure, Category = "Harpoon")
	bool DidHitShark() const { return bHitShark; }

	/** The shark the harpoon is stuck in (null while flying). */
	UFUNCTION(BlueprintPure, Category = "Harpoon")
	AShark* GetStuckShark() const { return StuckShark; }

	/** The harpoon went into a shark. */
	UPROPERTY(BlueprintAssignable, Category = "Harpoon")
	FHarpoonHitShark OnHitShark;

	/** The harpoon is over (missed, cut or returned). Fires just before it is destroyed. */
	UPROPERTY(BlueprintAssignable, Category = "Harpoon")
	FHarpoonEvent OnFinished;

protected:
	// ------------------------------------------------------------ components
	/** The actor's origin is the tip of the harpoon. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	/** The visible harpoon tip. Thin placeholder, replace the mesh with the real one (it should point along +X, origin at the tip). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TipMesh;

	/** The rope is tied here, at the back of the harpoon. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RopeTie;

	// ------------------------------------------------------------ flight
	/** Launch speed (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Flight", meta = (ClampMin = "100"))
	float LaunchSpeed = 5500.f;

	/** 0 = flies straight, 1 = falls like a stone. A little gives a believable arc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Flight", meta = (ClampMin = "0"))
	float GravityScale = 0.3f;

	/** Radius of the sweep that decides whether it hit the shark (cm). Bigger = more forgiving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Flight", meta = (ClampMin = "0"))
	float HitRadius = 18.f;

	/** How fast it slows down once it is in the water (1/s). Low enough that a shot aimed at a shark deep
	 * underwater (full orbit radius and depth) still has the reach to actually get there before it stops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Flight", meta = (ClampMin = "0"))
	float WaterDrag = 2.2f;

	/** A miss is removed this long after it hits the water (s). Long enough to cover the whole underwater
	 * travel a deep shot needs; a real miss still sinks to a stop well before this and just waits it out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Flight", meta = (ClampMin = "0"))
	float SinkTime = 2.f;

	/** A flight lasts at most this long (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Flight", meta = (ClampMin = "0.5"))
	float MaxFlightTime = 4.f;

	/** Size of the splash when a missed harpoon goes into the water (1 = bullet splash, 0 = none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Flight", meta = (ClampMin = "0"))
	float WaterSplashScale = 1.7f;

	/** How far the tip goes into the shark's body (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Flight", meta = (ClampMin = "0"))
	float StickDepth = 28.f;

	// ------------------------------------------------------------ rope
	/** Length of the rope (cm). Also the farthest the harpoon can fly and the farthest the barrel can be dragged from it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Rope", meta = (ClampMin = "100"))
	float RopeLength = 1600.f;

	/** Rope thickness (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Rope", meta = (ClampMin = "0.2"))
	float RopeThickness = 2.4f;

	/** Pieces the visible rope is made of. More = rounder curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Rope", meta = (ClampMin = "2", ClampMax = "64"))
	int32 RopeSegments = 24;

	/** Largest sag of a loose rope (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Rope", meta = (ClampMin = "0"))
	float MaxSag = 220.f;

	/** How fast the sag follows the slack (1/s). Low = soft, lazy rope. It never overshoots, so the rope never wobbles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Rope", meta = (ClampMin = "0.1"))
	float SagResponse = 5.f;

	/** Material of the rope (leave empty for the default grey). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Rope")
	TObjectPtr<UMaterialInterface> RopeMaterial;

	// ------------------------------------------------------------ barrel
	/** Deck distance the barrel slides from its place before it goes over the side (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel", meta = (ClampMin = "0"))
	float DeckEdgeDistance = 95.f;

	/** The barrel never moves faster than this (cm/s), however hard the rope pulls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel", meta = (ClampMin = "100"))
	float MaxBarrelSpeed = 1800.f;

	/** Upward kick when the barrel leaves the deck (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel")
	float LeaveDeckHop = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel", meta = (ClampMin = "0"))
	float BarrelGravity = 980.f;

	/** Height of the barrel's centre above the water while it floats (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel")
	float FloatHeight = 6.f;

	/** How fast the barrel settles to the water level (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel", meta = (ClampMin = "0.1"))
	float BuoyancyResponse = 9.f;

	/** Water drag on the floating barrel (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel", meta = (ClampMin = "0"))
	float BarrelWaterDrag = 1.6f;

	/** How far the barrel leans toward the shark when the rope is tight (deg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel", meta = (ClampMin = "0"))
	float LeanAngle = 14.f;

	/** How long the barrel needs to get back to the boat after the rope is cut (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel", meta = (ClampMin = "0.1"))
	float ReturnTime = 1.1f;

	/** Arc height of that trip (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel")
	float ReturnHop = 90.f;

	/** The rope cuts itself after this long on a shark (s). 0 = it stays until the player cuts it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel", meta = (ClampMin = "0"))
	float AutoReleaseTime = 0.f;

	// ------------------------------------------------------------ barrel wake
	/** The barrel leaves foam streaks on the water while it moves, like a fast boat. Off = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel Wake")
	bool bBarrelWake = true;

	/** Barrel speed (cm/s) at which the wake is at full strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel Wake", meta = (ClampMin = "1"))
	float WakeFullSpeed = 500.f;

	/** Below this speed (cm/s) there is no wake. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel Wake", meta = (ClampMin = "0"))
	float WakeMinSpeed = 70.f;

	/** Sideways distance between the three streaks behind the barrel (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel Wake", meta = (ClampMin = "0"))
	float WakeSpread = 26.f;

	/** Width of the middle streak (cm). The side ones are thinner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel Wake", meta = (ClampMin = "1"))
	float WakeWidth = 12.f;

	/** Width of the foam patch around the barrel that shows it is cutting through the water (cm). 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel Wake", meta = (ClampMin = "0"))
	float BowFoamWidth = 55.f;

	/** Splash patch where the barrel hits the water after falling off the boat (cm across). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel Wake", meta = (ClampMin = "0"))
	float SplashWidth = 110.f;

	/** Two dashed foam rails run along the sides of the barrel, like a cartoon bow wave. Width in cm, 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel Wake|Style", meta = (ClampMin = "0"))
	float RailWidth = 8.f;

	/** Distance of the rails from the barrel at slow speed and at full speed (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel Wake|Style", meta = (ClampMin = "0"))
	FVector2D RailOffset = FVector2D(32.0, 62.0);

	/** Length of one dash plus its gap (cm) and the part of it that is foam (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel Wake|Style", meta = (ClampMin = "10"))
	float DashLength = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel Wake|Style", meta = (ClampMin = "0.1", ClampMax = "1"))
	float DashDuty = 0.6f;

	/** Foam bubbles dropped behind the barrel per second at full speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel Wake|Style", meta = (ClampMin = "0"))
	float BubbleRate = 26.f;

	/** Water drops thrown up by the barrel per second at full speed (0 = none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel Wake|Style", meta = (ClampMin = "0"))
	float SprayRate = 38.f;

	/** How many drops can exist at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel Wake|Style", meta = (ClampMin = "0", ClampMax = "200"))
	int32 SprayPoolSize = 48;

	/** How long a drop lives (s) and how big it starts (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel Wake|Style", meta = (ClampMin = "0.1"))
	float SprayLife = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel Wake|Style", meta = (ClampMin = "1"))
	float SprayDropSize = 11.f;

	/** Material of the drops (bright, unlit white looks best). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harpoon|Barrel Wake|Style")
	TObjectPtr<UMaterialInterface> SprayMaterial;

private:
	enum class EPhase : uint8 { Flying, Stuck, Returning };
	enum class EBarrelState : uint8 { OnDeck, Falling, Floating };

	void TickFlight(float Dt);
	void TickStuck(float Dt);
	void TickReturn(float Dt);
	void StickInto(AShark* Shark, const FHitResult& Hit);
	void Finish();

	/** Keeps the barrel within the rope's reach of the tip (3D length, the barrel keeps its height). */
	FVector ConstrainBarrel(const FVector& Position, const FVector& Tip, bool& bOutTaut) const;
	void DriveBarrel(float Dt);
	void DetachBarrel();
	void ReattachBarrel();
	void UpdateRope(float Dt);
	void PaintBarrelWake(const FVector& From, const FVector& To, float Dt);
	void EmitSpray(const FVector& Origin, const FVector& InitialVelocity, int32 Count, float Spread, float UpSpeed);
	void UpdateSpray(float Dt);

	struct FSprayDrop
	{
		TObjectPtr<UStaticMeshComponent> Mesh;
		FVector Velocity = FVector::ZeroVector;
		float Age = 0.f;
		float Life = 0.f;
		float Size = 0.f;
		bool bActive = false;
	};
	TArray<FSprayDrop> Spray;
	float SprayAccum = 0.f;
	float BubbleAccum = 0.f;
	float SurfaceZAt(const FVector& Location) const;

	EPhase Phase = EPhase::Flying;
	EBarrelState BarrelState = EBarrelState::OnDeck;

	UPROPERTY(Transient)
	TObjectPtr<ABoatPawn> Boat;

	UPROPERTY(Transient)
	TObjectPtr<AShark> StuckShark;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> RopePieces;

	FVector Velocity = FVector::ZeroVector;
	float PhaseTime = 0.f;
	float WaterTime = 0.f;
	float Sag = 0.f;

	// barrel
	FVector BarrelVelocity = FVector::ZeroVector;
	FVector BarrelStart = FVector::ZeroVector;
	FTransform BarrelRelative;
	FQuat BarrelBaseRotation = FQuat::Identity;
	float Tension = 0.f;
	float ReturnAlpha = 0.f;
	bool bBarrelFree = false;
	bool bHitShark = false;
	float WakeTravel = 0.f;
	float PrevWakeTravel = 0.f;
};
