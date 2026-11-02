// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaterSplash.generated.h"

class ABoatPawn;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * A small stylized splash where something hits the water (a bullet, a harpoon that misses).
 * Spawn it on the water surface; it plays once and removes itself.
 *
 *  - a thin spout of water that shoots up and falls back
 *  - drops thrown up around it
 *  - a small foam star on the water and foam rings that appear one after the other, going outward
 *    (painted into the wake map, so they melt away with the rest of the foam)
 */
UCLASS()
class DENEME_API AWaterSplash : public AActor
{
	GENERATED_BODY()

public:
	AWaterSplash();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Makes the whole splash bigger or smaller (1 = bullet size). Set before or right after spawning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash", meta = (ClampMin = "0.1"))
	float Scale = 1.f;

	/** Bright material for the spout and the drops (M_Spray). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash")
	TObjectPtr<UMaterialInterface> Material;

	// ------------------------------------------------------------ spout
	/** Height and width of the water spout (cm) and how long it is up (s). 0 height = no spout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash|Spout", meta = (ClampMin = "0"))
	float SpoutHeight = 65.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash|Spout", meta = (ClampMin = "0.1"))
	float SpoutWidth = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash|Spout", meta = (ClampMin = "0.05"))
	float SpoutTime = 0.32f;

	// ------------------------------------------------------------ drops
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash|Drops", meta = (ClampMin = "0", ClampMax = "64"))
	int32 DropCount = 12;

	/** Slowest and fastest upward speed of a drop (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash|Drops")
	FVector2D DropUpSpeed = FVector2D(260.0, 520.0);

	/** Sideways speed of the drops (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash|Drops", meta = (ClampMin = "0"))
	float DropSpread = 90.f;

	/** Size of a drop (cm) and how long it lives (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash|Drops", meta = (ClampMin = "0.5"))
	float DropSize = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash|Drops", meta = (ClampMin = "0.05"))
	float DropLife = 0.55f;

	// ------------------------------------------------------------ foam on the water
	/** Size of the foam star where it went in (cm). 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash|Foam", meta = (ClampMin = "0"))
	float StarSize = 14.f;

	/** Radius of each foam ring (cm) and when it appears after the hit (s). Same number of entries in both. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash|Foam")
	TArray<float> RingRadii = {16.f, 32.f, 52.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash|Foam")
	TArray<float> RingDelays = {0.f, 0.12f, 0.3f};

	/** Line width (cm) and strength (0..1) of the rings. Later rings come out a little fainter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash|Foam", meta = (ClampMin = "1"))
	float RingWidth = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash|Foam", meta = (ClampMin = "0", ClampMax = "1"))
	float RingStrength = 0.95f;

private:
	struct FDrop
	{
		TObjectPtr<UStaticMeshComponent> Mesh;
		FVector Velocity = FVector::ZeroVector;
		float Size = 0.f;
		float Life = 0.f;
		bool bActive = false;
	};

	void PaintRing(float Radius, float Strength) const;
	void PaintStar() const;

	UPROPERTY(Transient)
	TObjectPtr<ABoatPawn> Boat;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Spout;

	TArray<FDrop> Drops;
	int32 RingsPainted = 0;
	float Age = 0.f;
	float WaterZ = 0.f;
	float Duration = 1.f;
};
