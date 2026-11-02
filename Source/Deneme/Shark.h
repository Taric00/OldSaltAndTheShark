// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shark.generated.h"

class ABoatPawn;
class UCapsuleComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ESharkState : uint8
{
	/** Swims deep, in a circle around the boat, waiting. */
	Circling,
	/** Rises near the surface and keeps circling: the fin is visible, the attack is coming. */
	Stalking,
	/** Rushes at the boat and breaches. The player can still hit it now. */
	Attacking,
	/** Dives away. Goes to a normal circle after a strike or a miss, to a far circle after being hit. */
	Retreating,
	/** Far from the boat, gathering strength. The next attack is faster. */
	Recovering,
	/** Out of health: rolls over and drifts up to the surface. */
	Dead
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSharkStateChanged, ESharkState, NewState, ESharkState, OldState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSharkEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSharkDamaged, float, Health01);

/**
 * A shark that stalks the boat.
 *
 * Loop: circle deep -> stalk near the surface -> attack -> (hit the boat | get hit by the player) -> retreat ->
 *   after a strike or a miss:  circle again at the normal distance and wait for the next attack
 *   after being hit:           recover on a far circle, then come back faster
 *
 * The player hits it with a line trace on the Shark channel (see AFishPhysicsCharacter) which calls NotifyPlayerHit.
 * It is moved kinematically, like the boat, so every number below can be tuned freely.
 */
UCLASS()
class DENEME_API AShark : public AActor
{
	GENERATED_BODY()

public:
	AShark();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Called when a harpoon sticks in the shark. Always drives it off into a retreat, whatever it was doing. */
	UFUNCTION(BlueprintCallable, Category = "Shark")
	void NotifyPlayerHit(const FVector& ImpactPoint);

	/**
	 * A rifle bullet. Costs health but does not drive the shark off: it only flinches and keeps doing what it was doing.
	 * ShotDirection is the bullet's direction (unit vector).
	 */
	UFUNCTION(BlueprintCallable, Category = "Shark")
	void ApplyRifleHit(float Damage, const FVector& ImpactPoint, const FVector& ShotDirection);

	UFUNCTION(BlueprintPure, Category = "Shark")
	float GetHealth01() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Shark")
	bool IsDead() const { return State == ESharkState::Dead; }

	/** A bullet hit it (health 0..1 after the hit). */
	UPROPERTY(BlueprintAssignable, Category = "Shark")
	FSharkDamaged OnDamaged;

	/** Health ran out. */
	UPROPERTY(BlueprintAssignable, Category = "Shark")
	FSharkEvent OnDied;

	/** What a harpoon sticks to: it follows the swimming body. */
	USceneComponent* GetStickAnchor() const { return SwimPivot; }

	UFUNCTION(BlueprintPure, Category = "Shark")
	ESharkState GetState() const { return State; }

	/** True while a hit from the player makes the shark back off. */
	UFUNCTION(BlueprintPure, Category = "Shark")
	bool IsVulnerable() const { return State == ESharkState::Stalking || State == ESharkState::Attacking; }

	UPROPERTY(BlueprintAssignable, Category = "Shark")
	FSharkStateChanged OnStateChanged;

	/** The shark rammed the boat. */
	UPROPERTY(BlueprintAssignable, Category = "Shark")
	FSharkEvent OnStrikeBoat;

	/** The player's hit drove the shark off. */
	UPROPERTY(BlueprintAssignable, Category = "Shark")
	FSharkEvent OnHitByPlayer;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCapsuleComponent> HitBox;

	/** Everything that visibly swims (wags with the swim cycle). The body mesh sits under this. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SwimPivot;

	/** The shark's body (Kopekbaligi_Boyali). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** The boat it hunts. Found automatically when empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Target")
	TObjectPtr<ABoatPawn> TargetBoat;

	// ------------------------------------------------------------ circling
	/** Distance from the boat while circling (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Circling", meta = (ClampMin = "100"))
	float OrbitRadius = 1000.f;

	/** Distance after being hit (cm). Bigger than OrbitRadius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Circling", meta = (ClampMin = "100"))
	float FarOrbitRadius = 2000.f;

	/** How far under the surface it swims while circling (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Circling", meta = (ClampMin = "0"))
	float SwimDepth = 300.f;

	/** Swim speed while circling (cm/s). The boat's own speed is added so it can keep up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Circling", meta = (ClampMin = "1"))
	float CircleSpeed = 420.f;

	/** Shortest and longest wait between attacks (s), counted from the moment it starts circling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Circling", meta = (ClampMin = "0"))
	float AttackDelayMin = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Circling", meta = (ClampMin = "0"))
	float AttackDelayMax = 14.f;

	// ------------------------------------------------------------ attack
	/** How deep it is while stalking (cm). Small = the fin cuts the surface and can be seen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Attack", meta = (ClampMin = "0"))
	float StalkDepth = 40.f;

	/** How long it stalks before it rushes (s). This is the player's warning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Attack", meta = (ClampMin = "0"))
	float StalkTime = 2.f;

	/** Rush speed (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Attack", meta = (ClampMin = "1"))
	float AttackSpeed = 850.f;

	/** Attack speed multiplier after a recovery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Attack", meta = (ClampMin = "1"))
	float PoweredAttackMultiplier = 1.4f;

	/** It breaches (rises out of the water) within this distance of the boat (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Attack", meta = (ClampMin = "1"))
	float SurfaceRange = 600.f;

	/** How high the body centre rises above the water when it strikes (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Attack")
	float BreachHeight = 70.f;

	/** The strike happens when the nose is this close to the hull (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Attack", meta = (ClampMin = "0"))
	float StrikeDistance = 30.f;

	/** Speed (cm/s), roll and pitch kick (deg/s) the boat gets from a strike. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Attack")
	float StrikeImpulse = 450.f;

	/** Hull damage of one strike (the hull has 100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Attack", meta = (ClampMin = "0"))
	float StrikeDamage = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Attack")
	float StrikeRollKick = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Attack")
	float StrikePitchKick = 25.f;

	// ------------------------------------------------------------ retreat and recovery
	/** Dive speed when backing off (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Retreat", meta = (ClampMin = "1"))
	float RetreatSpeed = 800.f;

	/** How long it gathers strength on the far circle (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Retreat", meta = (ClampMin = "0"))
	float ChargeTime = 14.f;

	// ------------------------------------------------------------ movement feel
	/** How quickly the velocity follows the wanted one (1/s). Lower = wider, lazier turns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Movement", meta = (ClampMin = "0.1"))
	float Agility = 2.5f;

	/** How quickly the body turns to face where it swims (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Movement", meta = (ClampMin = "0.1"))
	float TurnResponse = 3.f;

	/** Tail wag: angle (deg) and speed (rad/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Movement")
	float WagAngle = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Movement")
	float WagSpeed = 4.f;

	// ------------------------------------------------------------ health
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Health", meta = (ClampMin = "1"))
	float MaxHealth = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Shark|Health")
	float Health = 100.f;

	/** Speed a bullet knocks into the shark along the shot (cm/s). Only a flinch: it keeps its course. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Health", meta = (ClampMin = "0"))
	float BulletKick = 260.f;

	/** How much the body twitches on a hit (extra tail wag, deg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Health", meta = (ClampMin = "0"))
	float HitTwitch = 22.f;

	/** Seconds the dead shark takes to roll over and come up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Health", meta = (ClampMin = "0.1"))
	float DeathRiseTime = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shark|Debug")
	bool bShowDebug = false;

private:
	void SetState(ESharkState NewState);
	FVector OrbitPoint(const FVector& Location, const FVector& Center, float Radius, float Depth) const;
	float SurfaceZ(const FVector& Location) const;
	void Strike();

	ESharkState State = ESharkState::Circling;
	float StateTime = 0.f;
	float WaitTime = 10.f;
	float OrbitDirection = 1.f;
	bool bRetreatFar = false;
	bool bPowered = false;
	FVector Velocity = FVector::ZeroVector;
	float Twitch = 0.f;
	FRotator DeathStartRotation = FRotator::ZeroRotator;
	void TickDead(float Dt);
};
