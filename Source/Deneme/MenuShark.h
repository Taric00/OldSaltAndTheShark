// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MenuShark.generated.h"

class ABoatPawn;
class USplineComponent;
class USceneComponent;
class UStaticMeshComponent;

/**
 * A purely decorative shark for the menu scene: it swims laps along PathSpline (draw/edit its points in the
 * level to set the route and direction yourself) at a fixed depth below the water surface. It never targets,
 * damages or even notices the boat - no attack, no health, nothing beyond following the path and looping.
 */
UCLASS()
class DENEME_API AMenuShark : public AActor
{
	GENERATED_BODY()

public:
	AMenuShark();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	/** Draw/edit the points of this spline in the level: the shark follows it in order and loops back to the
	 * start. Comes with a default circle around the actor's own placement so there is something to see before
	 * you redraw it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> PathSpline;

	/** Everything that visibly swims (wags with the swim cycle, leans into turns). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SwimPivot;

	/** The shark's body (Kopekbaligi_Boyali). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** How far under the water surface it swims (cm). The surface follows the same waves the boat rides, so
	 * a small depth keeps the fin breaking the surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Shark", meta = (ClampMin = "0"))
	float SwimDepth = 45.f;

	/** Speed along the spline (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Shark", meta = (ClampMin = "1"))
	float SwimSpeed = 160.f;

	/** On: loops back to the start of the spline. Off: stops at the far end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Shark")
	bool bLoop = true;

	/** How quickly the body turns to face the spline's direction (1/s, eased - not an instant snap). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Shark", meta = (ClampMin = "0.1"))
	float TurnResponse = 1.5f;

	/** How far it leans into a turn (deg per deg/s of yaw rate). Purely a visual lean: it does not affect the path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Shark", meta = (ClampMin = "0"))
	float BankPerYawRate = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Shark", meta = (ClampMin = "0", ClampMax = "45"))
	float MaxBank = 18.f;

	/** Tail wag: angle (deg) and speed (rad/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Shark")
	float WagAngle = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Shark")
	float WagSpeed = 4.f;

private:
	float SurfaceZ(const FVector& Location) const;

	UPROPERTY(Transient)
	TObjectPtr<ABoatPawn> ReferenceBoat;

	/** The actor's own transform at spawn, frozen: the spline is a child component, so once the actor starts
	 * moving along it, querying the spline in world space would feed its own motion back into the path (it
	 * would drift away forever). Local-space spline points are transformed through this fixed frame instead. */
	FTransform HomeTransform = FTransform::Identity;

	float DistanceAlongSpline = 0.f;
	float CurrentYaw = 0.f;
	float YawRateSmoothed = 0.f;
};
