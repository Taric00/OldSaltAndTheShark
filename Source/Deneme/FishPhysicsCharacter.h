// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FishCameraShake.h"
#include "FishInteractionInterfaces.h"
#include "FishLocomotionInterface.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "FishPhysicsCharacter.generated.h"

class AHarpoon;
class AWaterSplash;
class UCameraComponent;
class UCapsuleComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USkeletalMeshComponent;
class USpringArmComponent;

/**
 * Third person character that is a real physics body: a simulating capsule driven by forces.
 *
 * - A stiff spring-damper holds the capsule at ride height above the ground (found with a sphere sweep), so steps,
 *   slopes and landings are absorbed by physics instead of snapping.
 * - Movement is a velocity controller: a high gain pulls the velocity to the wanted one, clamped by acceleration and
 *   braking limits. High gain = tight response, no sliding and no overshoot.
 * - The body is a normal rigid body: it is pushed by, and pushes, other simulating objects. Roll and pitch are locked,
 *   yaw is steered through angular velocity.
 * - The mesh is planted on the ground and leans into acceleration and turns.
 *
 * Controls: WASD, Left Shift (run), Space (jump), mouse (camera).
 */
UCLASS()
class DENEME_API AFishPhysicsCharacter : public APawn, public IFishLocomotionInterface, public IFishPromptProvider, public IFishAimProvider
{
	GENERATED_BODY()

public:
	AFishPhysicsCharacter();

	// IFishAimProvider
	virtual float GetAimAlpha() const override { return AimAlpha; }
	virtual float GetHitMarkerAlpha() const override;

	// IFishPromptProvider: text of the interactable in range
	virtual FText GetPromptText() const override { return InteractionPrompt; }

	/** Called by a vehicle: parks the character at the seat with physics off. */
	void EnterVehicle(USceneComponent* Seat);

	/** Called by a vehicle: puts the character back into the world with the vehicle's velocity. */
	void LeaveVehicle(const FTransform& ExitTransform, const FVector& InheritedVelocity, AActor* ExitedPlatform = nullptr);

	bool IsInVehicle() const { return bInVehicle; }

	/** A short camera bump: boat hits, recoil. Strength ~1 is firm, 0.2 is light. */
	void AddCameraShake(float Strength, float Duration = 0.4f) { CameraShake.Add(Strength, Duration); }

	/** Dizzy post process: darkened edges, swaying picture, colours drifting apart. Strength ~1 is a full hit. */
	void AddHitEffect(float Strength) { HitEffect.Add(Strength, HitEffectDuration); }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual FVector GetVelocity() const override;

	USkeletalMeshComponent* GetMesh() const { return Mesh; }
	UCapsuleComponent* GetCapsule() const { return Capsule; }

	// IFishLocomotionInterface
	virtual FVector GetLocomotionVelocity() const override;
	virtual bool HasMoveInput() const override { return bHasMoveInput; }
	virtual bool IsAirborne() const override { return !bGrounded; }
	virtual bool IsJumping() const override { return bJumping; }
	virtual bool IsRunningLocomotion() const override { return bWantsToRun; }
	virtual bool IsAiming() const override { return bAiming; }
	virtual float GetAimPitch() const override;
	virtual float GetAimYaw() const override;
	virtual bool IsTurningInPlaceLocomotion() const override { return bTurningInPlace; }
	virtual float GetTurnInPlaceRateLocomotion() const override { return TurnRateCurrent; }
	virtual float GetFloorZ() const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	/** Placeholder rifle, in the right hand while the rifle is the chosen weapon. Replace the mesh with the real one. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	/** The harpoon gun body, in the right hand while the harpoon is the chosen weapon. Stays in hand even reloading. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HarpoonHandMesh;

	/** The loaded bolt/bullet sitting in the harpoon gun. Hidden to simulate "no ammo" after a shot, until reloaded. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HarpoonAmmoMesh;

	/** Strap on the back the weapon that is not in use hangs from. It swings like cloth. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> BackSling;

	/** The rifle on the back (while the harpoon is in hand). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BackRifleMesh;

	/** The thin streak a rifle shot leaves for a moment. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TracerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	// ------------------------------------------------------------ body
	/** Body mass (kg). Decides how hard the character shoves physics objects and how hard they shove back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Body", meta = (ClampMin = "1"))
	float BodyMass = 85.f;

	/** Extra gravity multiplier on top of the world gravity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Body", meta = (ClampMin = "0"))
	float GravityScale = 1.4f;

	// ------------------------------------------------------------ movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement", meta = (ClampMin = "0"))
	float WalkSpeed = 300.f;

	/** The Mannequin walk / jog clips are authored for 300 / 600 cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement", meta = (ClampMin = "0"))
	float RunSpeed = 600.f;

	/** How quickly the top speed changes between walk and run (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement", meta = (ClampMin = "0.1"))
	float SpeedChangeRate = 6.f;

	/** How hard the controller pulls the velocity to the wanted one (1/s). Higher = tighter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement", meta = (ClampMin = "0.1"))
	float VelocityGain = 14.f;

	/** Limit on the acceleration while a key is held (cm/s^2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement", meta = (ClampMin = "1"))
	float MaxAcceleration = 2200.f;

	/** Limit on the deceleration when keys are released (cm/s^2). Higher = stops dead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement", meta = (ClampMin = "1"))
	float MaxBraking = 3200.f;

	/** 0..1, how much of the ground control is left in the air. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement", meta = (ClampMin = "0", ClampMax = "1"))
	float AirControl = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement")
	float JumpVelocity = 450.f;

	// ------------------------------------------------------------ ground spring
	/** How far above the ground the capsule bottom rides (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Ground", meta = (ClampMin = "0"))
	float RideHeight = 4.f;

	/** Spring stiffness (1/s^2). Higher = stiffer, smaller dip when landing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Ground", meta = (ClampMin = "1"))
	float SpringStiffness = 900.f;

	/** Spring damping (1/s). About 2 * sqrt(stiffness) is critical: no bounce. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Ground", meta = (ClampMin = "0"))
	float SpringDamping = 60.f;

	/** The spring can never push harder than this (cm/s^2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Ground", meta = (ClampMin = "0"))
	float MaxSpringAcceleration = 6000.f;

	/** Steepest walkable slope (deg). Steeper ground counts as a wall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Ground", meta = (ClampMin = "0", ClampMax = "89"))
	float MaxSlopeAngle = 50.f;

	/**
	 * Ground is still "under the feet" within this distance of the ride height (cm). Bigger = sticks over steps,
	 * and keeps the character grounded (so it still rides along) through a boat's normal wave bobbing instead of
	 * losing the deck at the bottom of a trough and being left behind in the air.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Ground", meta = (ClampMin = "0"))
	float GroundSnapDistance = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Ground", meta = (ClampMin = "1"))
	float GroundProbeRadius = 22.f;

	// ------------------------------------------------------------ turning
	/**
	 * On (default, like the Third Person template): the body turns to face the direction it moves and stands still
	 * when idle. Off: strafe style, the body always faces the camera and turns in place when it is left behind.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Turning")
	bool bOrientToMovement = true;

	/** Turn speed (deg/s) towards the movement direction. The Third Person template uses 500. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Turning", meta = (ClampMin = "1", EditCondition = "bOrientToMovement"))
	float OrientRotationSpeed = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Turning", meta = (ClampMin = "1"))
	float BodyTurnSpeed = 540.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Turning", meta = (ClampMin = "1"))
	float TurnInPlaceStartAngle = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Turning", meta = (ClampMin = "0"))
	float TurnInPlaceStopAngle = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Turning", meta = (ClampMin = "1"))
	float TurnInPlaceSpeed = 170.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Turning", meta = (ClampMin = "0.1"))
	float TurnInPlaceResponse = 9.f;

	// ------------------------------------------------------------ body motion
	/** Degrees of forward lean per cm/s^2 of acceleration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Body Motion")
	float LeanPitchPerAcceleration = 0.0035f;

	/** Degrees of sideways lean per cm/s^2 of lateral acceleration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Body Motion")
	float LeanRollPerAcceleration = 0.004f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Body Motion")
	float MaxLean = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Body Motion", meta = (ClampMin = "1"))
	float LeanStiffness = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Body Motion", meta = (ClampMin = "0.1"))
	float LeanDamping = 11.f;

	/** How fast the mesh follows the ground height (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Body Motion", meta = (ClampMin = "1"))
	float MeshHeightResponse = 30.f;

	// ------------------------------------------------------------ camera
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Camera")
	float CameraDistance = 340.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Camera")
	float CameraPitchMin = -60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Camera")
	float CameraPitchMax = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Camera")
	float LookSensitivity = 1.f;

	/** Flips the mouse up / down direction of the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Camera")
	bool bInvertLookPitch = true;

	// ------------------------------------------------------------ platform and interaction
	/** How well the character keeps up with a moving deck (1/s). Higher = glued to the deck. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Platform", meta = (ClampMin = "0"))
	float PlatformAccelerationSmoothing = 12.f;

	// ------------------------------------------------------------ aiming and weapon
	/** Walk speed while aiming (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Aim", meta = (ClampMin = "0"))
	float AimWalkSpeed = 220.f;

	/** How quickly the camera moves in and out of the aim (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Aim", meta = (ClampMin = "0.1"))
	float AimResponse = 9.f;

	/** Camera distance while aiming (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Aim", meta = (ClampMin = "0"))
	float AimCameraDistance = 240.f;

	/** Camera offset while aiming: over the right shoulder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Aim")
	FVector AimSocketOffset = FVector(0.0, 65.0, 75.0);

	/** Camera offset when not aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Aim")
	FVector NormalSocketOffset = FVector(0.0, 0.0, 60.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Aim", meta = (ClampMin = "10", ClampMax = "170"))
	float NormalFOV = 90.f;

	/** Narrower field of view while aiming = zoom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Aim", meta = (ClampMin = "10", ClampMax = "170"))
	float AimFOV = 66.f;

	/** On: shooting needs the aim button held. Off: the left click shoots any time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Aim")
	bool bRequireAimToFire = true;

	/** Bone the weapon is attached to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Aim")
	FName WeaponSocket = TEXT("hand_r");

	/** The shot line starts here, in the weapon's local space (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Aim")
	FVector MuzzleOffset = FVector(45.0, 0.0, 0.0);

	// ------------------------------------------------------------ weapons in hand
	/** How the shotgun (Shutgun mesh) sits in the hand socket. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Hand")
	FVector RifleHandLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Hand")
	FRotator RifleHandRotation = FRotator(0.0, 90.0, 0.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Hand")
	FVector RifleHandBaseScale = FVector(0.054, 0.054, 0.054);

	/** How the harpoon gun body (HarpoonGovde mesh) sits in the hand socket. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Hand")
	FVector HarpoonHandLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Hand")
	FRotator HarpoonHandRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Hand")
	FVector HarpoonHandBaseScale = FVector(0.044, 0.044, 0.044);

	/** How the loaded bolt (HarpoonMermi mesh) sits relative to the hand socket (same space as HarpoonHandLocation/Rotation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Hand")
	FVector HarpoonAmmoLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Hand")
	FRotator HarpoonAmmoRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Hand")
	FVector HarpoonAmmoBaseScale = FVector(0.044, 0.044, 0.044);

		/** How far the left-click line trace reaches (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Combat", meta = (ClampMin = "0"))
	float TraceDistance = 9000.f;

	/** Post process material of the dizzy screen after a hit (M_PP_HitDizzy). Leave empty for none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Hit Feel")
	TObjectPtr<UMaterialInterface> HitEffectMaterial;

	/** Seconds the dizzy screen takes to fade out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Hit Feel", meta = (ClampMin = "0.2"))
	float HitEffectDuration = 2.6f;

	/** Camera kick on every shot (0 = none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Combat", meta = (ClampMin = "0"))
	float ShotShakeStrength = 0.3f;

	/** Draws the shot for a moment so it can be seen while tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Combat")
	bool bDrawDebugTrace = false;

	/** How long the drawn shot stays on screen (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Combat", meta = (ClampMin = "0"))
	float DebugTraceDuration = 4.f;

	/** Seconds the character has to wait before the next shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Combat", meta = (ClampMin = "0"))
	float FireCooldown = 0.35f;

	/** How long the crosshair's hit-marker ("X") flashes after hitting the shark (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Combat", meta = (ClampMin = "0"))
	float HitMarkerDuration = 0.25f;

	/** The harpoon that is thrown on left click (BP_Harpoon). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Harpoon")
	TSubclassOf<AHarpoon> HarpoonClass;

	/** Seconds until a new harpoon is on the back after a miss. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Harpoon", meta = (ClampMin = "0"))
	float ReloadTime = 6.f;

	/** The same after a harpoon that hit (the barrel is already tied, so normally no wait). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Harpoon", meta = (ClampMin = "0"))
	float ReloadTimeAfterHit = 0.f;

	/** Bone the spare harpoon rests on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Harpoon")
	FName BackSocket = TEXT("spine_03");

	/** Where the back strap sits (relative to BackSocket). Both back weapons hang from it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Back")
	FVector BackHarpoonOffset = FVector(12.0, -24.0, 0.0);

	/** How the harpoon lies on the back strap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Back")
	FRotator BackHarpoonRotation = FRotator(40.0, 0.0, 0.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Back")
	FVector BackHarpoonScale = FVector(0.044, 0.044, 0.044);

	/** How the rifle lies on the back strap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Back")
	FRotator BackRifleRotation = FRotator(40.0, 0.0, 0.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Back")
	FVector BackRifleScale = FVector(0.054, 0.054, 0.054);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BackHarpoonMesh;

	// ------------------------------------------------------------ back weapon swing (cloth feel)
	/** Sideways swing per turning speed (deg per deg/s). The weapon on the back lags behind turns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Back")
	float BackSwingPerTurn = 0.045f;

	/** Swing away from the back per acceleration (deg per cm/s²): it lifts when stopping and starting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Back")
	float BackSwingPerAccel = 0.006f;

	/** Bounce with each step while moving (deg at run speed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Back")
	float BackStepBob = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Back", meta = (ClampMin = "0"))
	float BackSwingMax = 24.f;

	/** Spring of the swing. Lower stiffness = slower, softer; lower damping = more wobble. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Back", meta = (ClampMin = "1"))
	float BackSwingStiffness = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons|Back", meta = (ClampMin = "0"))
	float BackSwingDamping = 8.f;

	// ------------------------------------------------------------ weapon choice
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons")
	FKey HarpoonKey = EKeys::One;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons")
	FKey RifleKey = EKeys::Two;

	/** Seconds a weapon change takes (no shooting meanwhile). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Weapons", meta = (ClampMin = "0"))
	float SwitchTime = 0.3f;

	UFUNCTION(BlueprintPure, Category = "Fish|Weapons")
	bool IsRifleSelected() const { return bRifleSelected; }

	// ------------------------------------------------------------ rifle
	/** Health one bullet takes from the shark (the shark has 100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Rifle", meta = (ClampMin = "0"))
	float RifleDamage = 12.f;

	/** Seconds between shots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Rifle", meta = (ClampMin = "0.05"))
	float RifleCooldown = 0.55f;

	/** How deep under the surface a bullet still hurts (cm). The barrel shows where to shoot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Rifle", meta = (ClampMin = "0"))
	float RifleMaxDepth = 450.f;

	/** Camera kick and the upward jump of the aim per shot (deg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Rifle", meta = (ClampMin = "0"))
	float RifleShakeStrength = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Rifle", meta = (ClampMin = "0"))
	float RifleRecoilPitch = 1.4f;

	/** Where the shot streak starts, as forward/right/up (cm) from the weapon's hand socket, along the
	 * character's own facing (not the gun mesh's local axes, and not affected by its hand-pose scale). Push
	 * X out until it leaves from the barrel's tip instead of the hand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Rifle")
	FVector RifleMuzzleOffset = FVector(60.0, 0.0, 0.0);

	/** How long the shot streak stays and how thick it starts (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Rifle", meta = (ClampMin = "0"))
	float TracerTime = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Rifle", meta = (ClampMin = "0.1"))
	float TracerThickness = 2.5f;

	/** Material of the shot streak (bright unlit looks best, e.g. M_Spray). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Rifle")
	TObjectPtr<UMaterialInterface> TracerMaterial;

	/** Splash played where a bullet goes into the water (spout, drops, foam rings). Empty = the default AWaterSplash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Rifle")
	TSubclassOf<AWaterSplash> BulletSplashClass;

	/** Size of that splash (1 = default). 0 = no splash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Rifle", meta = (ClampMin = "0"))
	float BulletSplashScale = 1.f;

	/** Cuts the rope of the harpoon that is stuck in a shark. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Harpoon")
	FKey CutRopeKey = EKeys::R;

	UFUNCTION(BlueprintPure, Category = "Fish|Harpoon")
	bool HasHarpoonOut() const { return ActiveHarpoon != nullptr; }

	/** How often the surroundings are scanned for something to interact with (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Interaction", meta = (ClampMin = "0.02"))
	float InteractionScanInterval = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Debug")
	bool bShowDebug = false;

private:
	void FireHarpoon();
	void FireRifle();
	void UpdateWeapons(const APlayerController* PC, float Dt);
	void UpdateWeaponVisuals(float Dt);
	void UpdateBackSwing(float Dt);
	void UpdateAim(float Dt);
	void SetPlatform(AActor* NewPlatform);
	void UpdateInteraction(bool bInteractPressed, float Dt);
	void TickInVehicle(float Dt);

	void LookYaw(float Value);
	void LookPitch(float Value);
	void ReadInput(float& OutForward, float& OutRight, bool& bOutJump);
	void UpdateGround();
	void ApplyMovementForces(const FVector& MoveInput, float Dt);
	void ApplyYaw(const FVector& MoveInput, float Dt);
	void UpdateMesh(float Dt);

	FHitResult GroundHit;
	bool bInVehicle = false;
	float InteractCooldownUntil = 0.f;
	float FireCooldownUntil = 0.f;
	float InteractionScanTimer = 0.f;
	float HitMarkerUntil = -1.f;

	UPROPERTY(Transient)
	TObjectPtr<AHarpoon> ActiveHarpoon;
	bool bHarpoonWasOut = false;
	bool bLastHarpoonHit = false;

	// weapons
	bool bRifleSelected = false;
	float SwitchUntil = 0.f;
	float RifleReadyAt = 0.f;
	float TracerAge = 100.f;
	FVector TracerStart = FVector::ZeroVector;
	FVector TracerEnd = FVector::ZeroVector;
	FVector HarpoonHandScale = FVector::OneVector;
	FVector HarpoonAmmoScale = FVector::OneVector;
	FVector RifleHandScale = FVector::OneVector;

	// back swing spring: X = sideways in the plane of the back, Y = away from the back
	FVector2D BackSwing = FVector2D::ZeroVector;
	FVector2D BackSwingVelocity = FVector2D::ZeroVector;
	float PrevBodyYaw = 0.f;
	float StepPhase = 0.f;
	FText InteractionPrompt;

	UPROPERTY(Transient)
	TObjectPtr<AActor> InteractTarget;

	UPROPERTY(Transient)
	TObjectPtr<AActor> PlatformActor;

	FVector PlatformVelocity = FVector::ZeroVector;
	FVector PrevPlatformVelocity = FVector::ZeroVector;
	FVector PlatformAccelFiltered = FVector::ZeroVector;
	float PlatformYawRate = 0.f;
	bool bAiming = false;
	float AimAlpha = 0.f;
	FFishCameraShake CameraShake;
	FFishHitEffect HitEffect;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HitEffectMID;
	bool bGrounded = false;
	bool bJumping = false;
	bool bHasMoveInput = false;
	bool bWantsToRun = false;
	bool bTurningInPlace = false;
	float TurnRateCurrent = 0.f;
	float CurrentMaxSpeed = 0.f;
	float GroundDistance = 0.f;
	float MeshDrop = 0.f;
	float JumpSuppressUntil = 0.f;

	FVector PreviousVelocity = FVector::ZeroVector;
	FVector2D LeanAccelFiltered = FVector2D::ZeroVector;
	FVector2D Lean = FVector2D::ZeroVector;
	FVector2D LeanVelocity = FVector2D::ZeroVector;
};
