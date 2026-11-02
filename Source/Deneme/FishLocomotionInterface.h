// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FishLocomotionInterface.generated.h"

UINTERFACE(MinimalAPI)
class UFishLocomotionInterface : public UInterface
{
	GENERATED_BODY()
};

/** What UFishAnimInstance needs to know about a pawn, so the same Animation Blueprint serves every character type. */
class DENEME_API IFishLocomotionInterface
{
	GENERATED_BODY()

public:
	virtual FVector GetLocomotionVelocity() const = 0;
	virtual bool HasMoveInput() const = 0;
	/** True while no walkable ground is under the feet. */
	virtual bool IsAirborne() const = 0;
	/** True from the jump until the apex or the landing. */
	virtual bool IsJumping() const = 0;
	virtual bool IsRunningLocomotion() const = 0;
	/** True while the character holds the aim button (rifle stance). */
	virtual bool IsAiming() const { return false; }
	/** Camera pitch relative to the body (deg, positive = looking up). Drives the aim offset. */
	virtual float GetAimPitch() const { return 0.f; }
	/** Camera yaw relative to the body (deg, positive = to the right). */
	virtual float GetAimYaw() const { return 0.f; }
	virtual bool IsTurningInPlaceLocomotion() const = 0;
	virtual float GetTurnInPlaceRateLocomotion() const = 0;
	/** World Z of the ground under the character (used as the foot IK reference). */
	virtual float GetFloorZ() const = 0;
};
