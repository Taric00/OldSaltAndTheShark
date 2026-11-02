// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FishInteractionInterfaces.generated.h"

class APawn;

/** Something the character can walk up to and use with the interact key (the helm, a rod rack, a door ...). */
UINTERFACE(MinimalAPI)
class UFishInteractable : public UInterface
{
	GENERATED_BODY()
};

class DENEME_API IFishInteractable
{
	GENERATED_BODY()

public:
	/** World point the interaction distance is measured to. */
	virtual FVector GetInteractionLocation() const = 0;

	/** How close the interactor has to be (cm). */
	virtual float GetInteractionRadius() const { return 200.f; }

	/** Text shown on screen while the interactor is in range. */
	virtual FText GetInteractionText() const = 0;

	virtual bool CanInteract(const APawn* Interactor) const { return true; }

	virtual void Interact(APawn* Interactor) = 0;
};

/** A moving surface (a boat deck) that carries characters standing on it. */
UINTERFACE(MinimalAPI)
class UFishPlatform : public UInterface
{
	GENERATED_BODY()
};

class DENEME_API IFishPlatform
{
	GENERATED_BODY()

public:
	/** Velocity of the point of the platform that is currently at WorldLocation (includes rotation). */
	virtual FVector GetPlatformVelocityAtLocation(const FVector& WorldLocation) const = 0;

	/** Yaw turn rate of the platform (deg/s). */
	virtual float GetPlatformYawRate() const = 0;
};

/** A pawn that wants a line of text in the middle of the screen (see AFishHUD). */
UINTERFACE(MinimalAPI)
class UFishPromptProvider : public UInterface
{
	GENERATED_BODY()
};

class DENEME_API IFishPromptProvider
{
	GENERATED_BODY()

public:
	/** Empty text hides the prompt. */
	virtual FText GetPromptText() const = 0;
};

/** A pawn that can aim; drives the crosshair widget. */
UINTERFACE(MinimalAPI)
class UFishAimProvider : public UInterface
{
	GENERATED_BODY()
};

class DENEME_API IFishAimProvider
{
	GENERATED_BODY()

public:
	/** 0 = not aiming, 1 = fully aimed (eased, so the crosshair can fade). */
	virtual float GetAimAlpha() const = 0;

	/** 0..1, how much of the hit-marker flash is left (1 = just hit, 0 = none). Default: no hit marker. */
	virtual float GetHitMarkerAlpha() const { return 0.f; }
};

/** Something with a hull that can be damaged; drives the hull status widget. */
UINTERFACE(MinimalAPI)
class UFishHullStatus : public UInterface
{
	GENERATED_BODY()
};

class DENEME_API IFishHullStatus
{
	GENERATED_BODY()

public:
	/** 1 = undamaged, 0 = destroyed. */
	virtual float GetHullHealth01() const = 0;
};
