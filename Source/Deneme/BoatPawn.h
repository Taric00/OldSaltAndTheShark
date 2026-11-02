// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FishCameraShake.h"
#include "FishInteractionInterfaces.h"
#include "GameFramework/Pawn.h"
#include "BoatPawn.generated.h"

class AFishPhysicsCharacter;
class UBoxComponent;
class UTexture2D;
class UTextureRenderTarget2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBoatHullDamaged, float, NewHealth01, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBoatHullDestroyed);

/** One streak of the wake: how far from the centre line it runs, how thick it is, how bright. */
USTRUCT(BlueprintType)
struct FWakeLine
{
	GENERATED_BODY()

	/** Sideways distance from the boat's centre line (cm). Negative = left. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wake")
	float SideOffset = 0.f;

	/** Thickness of the streak (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wake", meta = (ClampMin = "1"))
	float Width = 50.f;

	/** 0..1 brightness of the streak. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wake", meta = (ClampMin = "0", ClampMax = "1"))
	float Strength = 1.f;
};
class UCameraComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMaterialParameterCollection;
class USceneComponent;
class USpringArmComponent;
class UStaticMeshComponent;

/**
 * Heavy, outboard-style boat. Thrust acts along the hull and the stern is what steers (like a forklift),
 * so the bow swings around a pivot ahead of the centre and the stern slides outward.
 *
 * Controls: W/S (or Up/Down, left stick Y) = throttle, A/D (or Left/Right, left stick X) = steer, mouse = camera.
 * The pawn is simulated kinematically (no physics body) so every number below is directly tweakable.
 */
UCLASS()
class DENEME_API ABoatPawn : public APawn, public IFishInteractable, public IFishPlatform, public IFishPromptProvider, public IFishHullStatus
{
	GENERATED_BODY()

public:
	ABoatPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// IFishInteractable: the helm
	virtual FVector GetInteractionLocation() const override;
	virtual float GetInteractionRadius() const override { return InteractionRadius; }
	virtual FText GetInteractionText() const override { return InteractionText; }
	virtual bool CanInteract(const APawn* Interactor) const override { return !IsDriven(); }
	virtual void Interact(APawn* Interactor) override;

	// IFishPlatform: carries characters standing on the deck
	virtual FVector GetPlatformVelocityAtLocation(const FVector& WorldLocation) const override;
	virtual float GetPlatformYawRate() const override { return YawRate; }

	// IFishPromptProvider: "leave the boat" while somebody is driving
	virtual FText GetPromptText() const override;

	// IFishHullStatus
	virtual float GetHullHealth01() const override { return MaxHullHealth > 0.f ? FMath::Clamp(HullHealth / MaxHullHealth, 0.f, 1.f) : 0.f; }

	/** Damages the hull. Fires OnHullDamaged, and OnHullDestroyed when it reaches zero. */
	UFUNCTION(BlueprintCallable, Category = "Boat|Hull")
	void ApplyHullDamage(float Damage);

	UFUNCTION(BlueprintCallable, Category = "Boat|Hull")
	void RepairHull(float Amount);

	UPROPERTY(BlueprintAssignable, Category = "Boat|Hull")
	FBoatHullDamaged OnHullDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Boat|Hull")
	FBoatHullDestroyed OnHullDestroyed;

	UFUNCTION(BlueprintPure, Category = "Boat")
	bool IsDriven() const { return DriverCharacter != nullptr; }

	/** Puts the character at the helm and gives the player control of the boat. */
	void BeginDriving(AFishPhysicsCharacter* Character);

	/** Returns control to the character and puts them back on the deck. */
	void EndDriving();

	/** Same formula as the vertex waves in M_StylizedLake, so the boat rides the visible water. */
	UFUNCTION(BlueprintPure, Category = "Boat|Waves")
	float GetWaveHeightAt(const FVector2D& WorldXY) const;

	UFUNCTION(BlueprintPure, Category = "Boat")
	float GetForwardSpeed() const;

	/** World Z of the water surface at the given point, waves included. */
	UFUNCTION(BlueprintPure, Category = "Boat|Waves")
	float GetSurfaceZ(const FVector2D& WorldXY) const { return WaterLevelZ + GetWaveHeightAt(WorldXY); }

	/** Horizontal velocity of the boat (cm/s). */
	UFUNCTION(BlueprintPure, Category = "Boat")
	FVector GetBoatVelocity() const { return Velocity; }

	/** A sudden shove, e.g. a shark hit: changes the speed and kicks the hull so it rocks (deg/s). */
	UFUNCTION(BlueprintCallable, Category = "Boat")
	void ApplyImpulse(const FVector& DeltaVelocity, float RollKick, float PitchKick);

	/** The harpoon barrel. It sits at BarrelHome until a harpoon drags it off. */
	/** Foam for the wake map: a streak between two world points (width in cm, alpha 0..1). Painted by the next wake update. */
	void AddWakeSegment(const FVector& From, const FVector& To, float Width, float Alpha)
	{
		QueuedWake.Add({From, To, Width, Alpha});
	}

	UStaticMeshComponent* GetBarrelMesh() const { return BarrelMesh; }
	USceneComponent* GetBarrelHome() const { return BarrelHome; }
	USceneComponent* GetBarrelRopePoint() const { return BarrelRopePoint; }

protected:
	/** Where the barrel rests on the deck. Move it in the Blueprint to put the barrel somewhere else. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> BarrelHome;

	/** The barrel the harpoon rope is tied to (the Tanker mesh). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BarrelMesh;

	/** The rope is tied here (top of the barrel). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> BarrelRopePoint;

	/** Relative location of the barrel mesh under BarrelHome (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Barrel")
	FVector BarrelMeshLocation = FVector(0.0, 0.0, 33.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Barrel")
	FVector BarrelMeshScale = FVector(0.16, 0.16, 0.16);

	// ------------------------------------------------------------ components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> HullBox;

	/** Receives wave height, wave tilt and the acceleration pitch / lean roll. Put the visual boat under this. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> VisualPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HullMesh;

	/** Placeholder outboard motor, replace or remove when the real one exists. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MotorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	// ------------------------------------------------------------ deck and helm
	/** Walkable deck surface. Only the ground probe and foot IK see it: the character is held up by its own spring. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> DeckCollision;

	/** Invisible railings that keep the character on the boat (they block pawns only). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UBoxComponent>> Rails;

	/** Where the interaction distance is measured from. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> HelmPoint;

	/** The character stands here while driving. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> DriverSeat;

	/** The character steps off here when leaving the helm. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> ExitPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Interaction")
	FText InteractionText = NSLOCTEXT("Boat", "Drive", "E  -  Tekneyi Kullan");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Interaction")
	FText ExitText = NSLOCTEXT("Boat", "Exit", "E  -  Tekneden In");

	/** How close the character has to be to the helm (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Interaction", meta = (ClampMin = "0"))
	float InteractionRadius = 220.f;

	// ------------------------------------------------------------ throttle
	/** Top speed going forward (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Throttle", meta = (ClampMin = "0"))
	float MaxForwardSpeed = 900.f;

	/** Top speed in reverse (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Throttle", meta = (ClampMin = "0"))
	float MaxReverseSpeed = 300.f;

	/** How fast the throttle itself ramps up (1/s). Lower = heavier, slower to get going. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Throttle", meta = (ClampMin = "0.1"))
	float ThrottleResponse = 1.2f;

	/** Water resistance along the hull (1/s). Also sets how long it takes to reach top speed (~3 / value). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Throttle", meta = (ClampMin = "0.01"))
	float ForwardDrag = 0.35f;

	/** Keel resistance sideways (1/s). Lower = the stern slides out more in turns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Throttle", meta = (ClampMin = "0.01"))
	float LateralDrag = 1.1f;

	// ------------------------------------------------------------ steering
	/** Maximum turn rate (deg/s). Keep it low for a heavy boat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Steering", meta = (ClampMin = "0"))
	float MaxYawRate = 28.f;

	/** How fast the tiller moves to the pressed direction (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Steering", meta = (ClampMin = "0.1"))
	float SteerResponse = 1.8f;

	/** How fast the hull picks up / loses its turn rate (1/s). Lower = more inertia. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Steering", meta = (ClampMin = "0.1"))
	float YawResponse = 1.3f;

	/** Speed (cm/s) at which the rudder has full authority. Below it, steering is weaker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Steering", meta = (ClampMin = "1"))
	float SteerFullSpeed = 500.f;

	/** 0..1: how much a standing boat can still turn from the thrust of the motor alone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Steering", meta = (ClampMin = "0", ClampMax = "1"))
	float PropWashSteering = 0.35f;

	/** Distance ahead of the centre the hull pivots around (cm). Bigger = the stern swings out wider. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Steering")
	float PivotForwardOffset = 120.f;

	/** Extra forward drag while turning, 0 = none. Makes the boat shed speed in hard turns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Steering", meta = (ClampMin = "0"))
	float TurnDragFactor = 0.6f;

	// ------------------------------------------------------------ body motion
	/** Degrees of bow-up per cm/s^2 of acceleration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Body Motion")
	float PitchPerAcceleration = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Body Motion")
	float MaxPitch = 10.f;

	/** Higher = the bow reacts quicker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Body Motion", meta = (ClampMin = "1"))
	float PitchStiffness = 30.f;

	/** Lower = the bow overshoots and rocks back more before settling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Body Motion", meta = (ClampMin = "0.1"))
	float PitchDamping = 4.f;

	/** Degrees of roll per cm/s^2 of sideways acceleration. Positive leans outward, negative leans into the turn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Body Motion")
	float RollPerLateralAcceleration = 0.008f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Body Motion")
	float MaxRoll = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Body Motion", meta = (ClampMin = "1"))
	float RollStiffness = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Body Motion", meta = (ClampMin = "0.1"))
	float RollDamping = 3.f;

	// ------------------------------------------------------------ waves (keep in sync with M_StylizedLake)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Waves")
	bool bRideWaves = true;

	/** Z of the water surface at rest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Waves")
	float WaterLevelZ = 0.f;

	/** Material parameter WaveHeight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Waves")
	float WaveHeight = 12.f;

	/** Material parameter WaveFrequency. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Waves")
	float WaveFrequency = 0.004f;

	/** Material parameter WaveSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Waves")
	float WaveSpeed = 1.f;

	/** How much of the wave slope the hull follows (0 = flat, 1 = full). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Waves", meta = (ClampMin = "0"))
	float WaveTiltScale = 1.f;

	/** Speed (cm/s) at which the hull skims over the waves and no longer follows them up and down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Waves", meta = (ClampMin = "1"))
	float WaveDampSpeed = 600.f;

	/** How much of the wave motion is left at WaveDampSpeed and above (0 = glides flat, 1 = no damping). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Waves", meta = (ClampMin = "0", ClampMax = "1"))
	float HighSpeedWaveInfluence = 0.1f;

	/** Low-pass on the wave motion (1/s). Lower = the hull ignores quick wave changes more. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Waves", meta = (ClampMin = "0.1"))
	float WaveResponse = 4.f;

	/** Half length / width of the hull used to sample the wave slope (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Waves")
	FVector2D WaveSampleHalfExtent = FVector2D(200.0, 80.0);

	/** The turning/acceleration lean and the wave tilt add together; this caps the total so a hard turn on top
	 * of a wave slope cannot tip the hull further than intended. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Waves", meta = (ClampMin = "1"))
	float MaxCombinedTilt = 13.f;

	/** Rides this much higher out of the water than the raw wave sample (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Waves", meta = (ClampMin = "0"))
	float HullFreeboard = 22.f;

	/** The hull is never allowed to sink closer than this to the water actually sampled right now, on top of
	 * however much the current tilt already dips it (cm). The safety net behind HullFreeboard for sharp waves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Waves", meta = (ClampMin = "0"))
	float MinHullClearance = 10.f;

	/** Actors the hull must not collide with, e.g. the water mesh. The DenizMesh actor is found automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Waves")
	TArray<TObjectPtr<AActor>> WaterActors;

	// ------------------------------------------------------------ wake (read by M_StylizedLake)
	/**
	 * Top-down map of the whole water area (RT_Wake). Every frame the boat paints its wake streaks into it where
	 * the stern is and fades the old ones, so the foam stays where the boat left it and melts away.
	 * Leave empty to switch the wake off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Wake")
	TObjectPtr<UTextureRenderTarget2D> WakeRenderTarget;

	/** Parameter collection that tells the water material which world area the wake map covers (MPC_Water). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Wake")
	TObjectPtr<UMaterialParameterCollection> WaterParameters;

	/** World centre (X, Y) of the area the wake map covers (cm). Match the water mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Wake")
	FVector2D WakeMapCenter = FVector2D::ZeroVector;

	/** Side length of the square area the wake map covers (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Wake", meta = (ClampMin = "100"))
	float WakeMapSize = 10000.f;

	/** The streaks left behind the stern, each with its own side offset and thickness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Wake")
	TArray<FWakeLine> WakeLines;

	/** On: ignores WakeLines and scatters WakeLineCount thin streaks of random thickness each play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Wake")
	bool bRandomizeWakeLines = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Wake", meta = (ClampMin = "1", ClampMax = "24", EditCondition = "bRandomizeWakeLines"))
	int32 WakeLineCount = 9;

	/** The streaks are spread over this half-width around the centre line (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Wake", meta = (ClampMin = "0", EditCondition = "bRandomizeWakeLines"))
	float WakeSpreadHalfWidth = 135.f;

	/** Thinnest and thickest random streak (cm). Most of them come out thin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Wake", meta = (ClampMin = "1", EditCondition = "bRandomizeWakeLines"))
	float WakeMinWidth = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Wake", meta = (ClampMin = "1", EditCondition = "bRandomizeWakeLines"))
	float WakeMaxWidth = 28.f;

	/** How much each streak swells and shrinks along its length (0 = even, 0.5 = +/-50%). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Wake", meta = (ClampMin = "0", ClampMax = "0.9"))
	float WakeWidthWobble = 0.45f;

	/** How far a streak drifts sideways along its length (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Wake", meta = (ClampMin = "0"))
	float WakeSideWobble = 16.f;

	/** 0 = a new random pattern every time, any other number = the same pattern. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Wake")
	int32 WakeSeed = 0;

	/** Seconds a streak takes to melt away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Wake", meta = (ClampMin = "0.5"))
	float WakeFadeSeconds = 12.f;

	/** Boat speed (cm/s) at which the streaks are painted at full strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Wake", meta = (ClampMin = "1"))
	float WakeFullSpeed = 650.f;

	/** Below this speed (cm/s) the boat leaves no wake. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Wake", meta = (ClampMin = "0"))
	float WakeMinSpeed = 60.f;

	/** Where the streaks start, behind the centre of the boat (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Wake")
	float WakeSternOffset = 230.f;

	/**
	 * The wake map follows the boat: when the boat gets this far from the map centre (fraction of WakeMapSize),
	 * the map is moved under it and the foam already painted is shifted along, so it stays where it was in the world.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Wake", meta = (ClampMin = "0.05", ClampMax = "0.45"))
	float WakeRecenterFraction = 0.2f;

	// ------------------------------------------------------------ motor
	/** The outboard motor turns against the steering, like a real tiller motor (deg at full steering). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Motor")
	float MotorTurnAngle = 32.f;

	/** How quickly the motor follows the steering (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Motor", meta = (ClampMin = "0.1"))
	float MotorTurnResponse = 7.f;

	// ------------------------------------------------------------ hull
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boat|Hull", meta = (ClampMin = "1"))
	float MaxHullHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Hull", meta = (ClampMin = "0"))
	float HullHealth = 100.f;

	// ------------------------------------------------------------ hit feel
	/** Post process material of the dizzy screen after a hit (M_PP_HitDizzy). Leave empty for none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Hit Feel")
	TObjectPtr<UMaterialInterface> HitEffectMaterial;

	/** Strength of the dizzy screen for a 20 damage hit (1 = full). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Hit Feel", meta = (ClampMin = "0"))
	float HitEffectScale = 1.f;

	/** Seconds the dizzy screen takes to fade out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Hit Feel", meta = (ClampMin = "0.2"))
	float HitEffectDuration = 2.6f;

	/** Camera shake strength when the boat is hit (1 = a firm bump). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Hit Feel", meta = (ClampMin = "0"))
	float HitShakeScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Hit Feel", meta = (ClampMin = "0.05"))
	float HitShakeDuration = 0.55f;

	// ------------------------------------------------------------ camera
	/** Seconds the camera takes to glide between the character and the boat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Camera", meta = (ClampMin = "0"))
	float CameraBlendTime = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Camera")
	float CameraPitch = -18.f;

	/** Seconds without mouse movement before the camera swings back behind the boat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Camera")
	float RecenterDelay = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Camera")
	float RecenterSpeed = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Camera")
	float LookSensitivity = 1.f;

	/** Flips the mouse up / down direction of the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Camera")
	bool bInvertLookPitch = true;

	/** Print speed / throttle on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boat|Debug")
	bool bShowDebug = false;

private:
	void LookYaw(float Value);
	void LookPitch(float Value);
	void ReadInput(float& OutThrottle, float& OutSteer) const;
	float SampleWave(const FVector& WorldLocation, float Time) const;

	FVector Velocity = FVector::ZeroVector;
	float ThrottleSmoothed = 0.f;
	float SteerSmoothed = 0.f;
	float YawRate = 0.f;
	float AccelFiltered = 0.f;
	float LateralAccelFiltered = 0.f;
	float Pitch = 0.f;
	float PitchVelocity = 0.f;
	float Roll = 0.f;
	float RollVelocity = 0.f;
	float TimeSinceLook = 100.f;

	UPROPERTY(Transient)
	TObjectPtr<AFishPhysicsCharacter> DriverCharacter;

	float DrivingStartTime = 0.f;
	void UpdateWake(float Dt);
	FVector2D WakeToPixel(const FVector& WorldLocation, const FVector2D& CanvasSize) const;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> WhiteTexture;

	FVector PrevSternLocation = FVector::ZeroVector;
	FVector PrevRight = FVector::RightVector;
	bool bHavePrevStern = false;

	/** Moves the wake map under NewCenter and carries the painted foam along (through WakeScratch). */
	void RecenterWakeMap(const FVector2D& NewCenter);
	void PushWakeMapToWater();

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> WakeScratch;

	FRotator MotorBaseRotation = FRotator::ZeroRotator;
	float MotorYaw = 0.f;

	void BuildWakeLines();

	struct FQueuedWake
	{
		FVector From;
		FVector To;
		float Width;
		float Alpha;
	};
	TArray<FQueuedWake> QueuedWake;
	TArray<FWakeLine> ActiveWakeLines;
	float WakeTravel = 0.f;
	float PrevWakeTravel = 0.f;

	FFishCameraShake CameraShake;
	FFishHitEffect HitEffect;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HitEffectMID;

	/** Gives the camera a still copy of the current view to glide away from (the old pawn's camera is unreliable). */
	static void GlideViewTo(APlayerController* PC, AActor* NewTarget, float BlendTime, const FVector& FromLocation, const FRotator& FromRotation, float FromFOV);
	FTransform PrevPivotTransform;
	FTransform CurrPivotTransform;
	float LastDt = 0.016f;
	float WaveZSmoothed = 0.f;
	float WavePitchSmoothed = 0.f;
	float WaveRollSmoothed = 0.f;
};
