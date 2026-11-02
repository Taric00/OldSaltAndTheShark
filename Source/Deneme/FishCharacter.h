// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FishLocomotionInterface.h"
#include "GameFramework/Character.h"
#include "FishCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;

/**
 * Third person character driven by the Character Movement Component.
 *
 * - WASD moves relative to the camera, Left Shift runs, Space jumps, mouse looks around.
 * - While moving the body swings round to face the camera. Standing still, the camera can turn freely and the
 *   body only follows once the angle gets large enough (turn in place). UFishAnimInstance reads bTurningInPlace.
 * - Acceleration, braking, ground friction and mass live on the movement component and are tweakable from here,
 *   so the character picks up speed and stops with weight instead of snapping.
 */
UCLASS()
class DENEME_API AFishCharacter : public ACharacter, public IFishLocomotionInterface
{
	GENERATED_BODY()

public:
	AFishCharacter();

	// IFishLocomotionInterface
	virtual FVector GetLocomotionVelocity() const override { return GetVelocity(); }
	virtual bool HasMoveInput() const override;
	virtual bool IsAirborne() const override;
	virtual bool IsJumping() const override;
	virtual bool IsRunningLocomotion() const override { return bWantsToRun; }
	virtual bool IsTurningInPlaceLocomotion() const override { return bTurningInPlace; }
	virtual float GetTurnInPlaceRateLocomotion() const override { return TurnRateCurrent; }
	virtual float GetFloorZ() const override;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "Fish")
	bool IsTurningInPlace() const { return bTurningInPlace; }

	/** Signed yaw speed (deg/s) of the current turn in place, positive = clockwise seen from above. */
	UFUNCTION(BlueprintPure, Category = "Fish")
	float GetTurnInPlaceRate() const { return TurnRateCurrent; }

	UFUNCTION(BlueprintPure, Category = "Fish")
	bool IsRunning() const { return bWantsToRun; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	// ------------------------------------------------------------ movement
	/** Speed when walking (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement", meta = (ClampMin = "0"))
	float WalkSpeed = 300.f;

	/** Speed while Left Shift is held (cm/s). The Mannequin walk / jog clips are authored for 300 / 600. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement", meta = (ClampMin = "0"))
	float RunSpeed = 600.f;

	/** How quickly the top speed itself changes between walk and run (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement", meta = (ClampMin = "0.1"))
	float SpeedChangeRate = 3.f;

	/** How fast the character picks up speed (cm/s^2). Lower = heavier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement", meta = (ClampMin = "1"))
	float Acceleration = 650.f;

	/** How fast the character stops when no key is held (cm/s^2). Lower = slides longer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement", meta = (ClampMin = "1"))
	float BrakingDeceleration = 900.f;

	/** Ground grip while steering. Higher = changes direction quicker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement", meta = (ClampMin = "0"))
	float GroundFriction = 5.f;

	/** Body mass (kg). Used when pushing physics objects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement", meta = (ClampMin = "1"))
	float BodyMass = 85.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement")
	float JumpVelocity = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement")
	float GravityScale = 1.4f;

	// ------------------------------------------------------------ turning
	/** Turn speed (deg/s) of the body towards the camera while moving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Turning", meta = (ClampMin = "1"))
	float BodyTurnSpeed = 360.f;

	/** Angle between camera and body (deg) that triggers a turn in place while standing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Turning", meta = (ClampMin = "1"))
	float TurnInPlaceStartAngle = 70.f;

	/** The turn ends when the remaining angle drops below this (deg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Turning", meta = (ClampMin = "0"))
	float TurnInPlaceStopAngle = 4.f;

	/** Maximum speed (deg/s) of a turn in place. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Turning", meta = (ClampMin = "1"))
	float TurnInPlaceSpeed = 150.f;

	/** How fast the turn speed ramps (1/s). Lower = the body eases in and out more. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Turning", meta = (ClampMin = "0.1"))
	float TurnInPlaceResponse = 5.f;

	// ------------------------------------------------------------ camera
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Camera")
	float CameraDistance = 340.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Camera")
	float CameraPitchMin = -60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Camera")
	float CameraPitchMax = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Camera")
	float LookSensitivity = 1.f;

private:
	void LookYaw(float Value);
	void LookPitch(float Value);
	void ApplyMovementSettings();

	bool bTurningInPlace = false;
	bool bWantsToRun = false;
	float TurnRateCurrent = 0.f;
	float TargetMaxSpeed = 0.f;
};
