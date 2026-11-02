// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "FishAnimInstance.generated.h"

class AFishCharacter;
class UCharacterMovementComponent;

/**
 * Feeds the character's Animation Blueprint. Everything the graph needs (blend space inputs, state machine rules,
 * foot IK offsets) is computed here in C++ and exposed as read-only variables, so the graph itself stays plain wiring.
 */
UCLASS()
class DENEME_API UFishAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// ------------------------------------------------------------ locomotion
	/** Ground speed (cm/s). Blend Space Y axis. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Speed = 0.f;

	/** Movement direction relative to the body, -180..180 (0 = forward, 90 = right). Blend Space X axis. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Direction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bShouldMove = false;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsRunning = false;

	/** Airborne and still rising: plays the jump start, not the fall loop. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsJumping = false;

	/** Holding the aim button: the rifle stance is used instead of the normal locomotion. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsAiming = false;

	/** Where the camera looks relative to the body (deg): feeds the aim offset that bends the upper body. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float AimPitch = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float AimYaw = 0.f;

	/** How quickly AimPitch / AimYaw follow the camera (1/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion", meta = (ClampMin = "0.1"))
	float AimInterpSpeed = 14.f;

	// ------------------------------------------------------------ turn in place
	UPROPERTY(BlueprintReadOnly, Category = "Turn In Place")
	bool bTurningInPlace = false;

	/** Signed yaw speed (deg/s) of the turn in place. */
	UPROPERTY(BlueprintReadOnly, Category = "Turn In Place")
	float TurnInPlaceRate = 0.f;

	/** Play rate for the stepping animation used while turning, scales with the turn speed. */
	UPROPERTY(BlueprintReadOnly, Category = "Turn In Place")
	float TurnPlayRate = 0.6f;

	// ------------------------------------------------------------ foot IK
	/** Vertical foot offsets (cm) in component space. Plug into the Two Bone IK effectors. */
	UPROPERTY(BlueprintReadOnly, Category = "Foot IK")
	FVector LeftFootEffectorOffset = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Foot IK")
	FVector RightFootEffectorOffset = FVector::ZeroVector;

	/** How far the pelvis drops (cm, <= 0) so the lower foot can reach the ground. */
	UPROPERTY(BlueprintReadOnly, Category = "Foot IK")
	FVector PelvisOffset = FVector::ZeroVector;

	/** 1 on the ground, 0 in the air. Blend weight of the IK. */
	UPROPERTY(BlueprintReadOnly, Category = "Foot IK")
	float FootIKAlpha = 1.f;

	// ------------------------------------------------------------ settings
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK")
	FName LeftFootBone = TEXT("foot_l");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK")
	FName RightFootBone = TEXT("foot_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK")
	FName LeftIKBone = TEXT("ik_foot_l");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK")
	FName RightIKBone = TEXT("ik_foot_r");

	/** Distance from the foot bone to the sole (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK")
	float FootHeight = 8.f;

	/** How far above / below the character's feet the ground trace reaches (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK")
	float TraceUp = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK")
	float TraceDown = 60.f;

	/** How fast the feet follow the ground (1/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK")
	float FootInterpSpeed = 18.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK")
	float PelvisInterpSpeed = 8.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foot IK")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

private:
	float TraceFootOffset(const FName& Bone) const;

	TWeakObjectPtr<APawn> OwnerPawn;

	float LeftOffset = 0.f;
	float RightOffset = 0.f;
	float PelvisZ = 0.f;
};
