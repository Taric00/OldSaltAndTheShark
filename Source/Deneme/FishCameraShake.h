// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * A tiny procedural camera shake: a few out-of-phase sines under a fast-decaying envelope. Add a shake, tick it every
 * frame and apply the result to the camera component's relative transform. Shakes stack (the stronger one wins).
 */
struct FFishCameraShake
{
	/** Starts (or boosts) a shake. Strength ~1 is a firm bump, 0.2 a light kick. */
	void Add(float InStrength, float InDuration)
	{
		if (InStrength >= CurrentStrength())
		{
			Strength = InStrength;
			Duration = FMath::Max(InDuration, 0.05f);
			Time = 0.f;
			Phase = FVector(FMath::FRand(), FMath::FRand(), FMath::FRand()) * UE_TWO_PI;
		}
	}

	/** Advances the shake and returns the offset to apply this frame. */
	void Tick(float Dt, FVector& OutLocation, FRotator& OutRotation)
	{
		OutLocation = FVector::ZeroVector;
		OutRotation = FRotator::ZeroRotator;
		if (Strength <= 0.f)
		{
			return;
		}

		Time += Dt;
		if (Time >= Duration)
		{
			Strength = 0.f;
			return;
		}

		const float Alpha = 1.f - Time / Duration;
		const float Envelope = Alpha * Alpha * Strength;

		OutLocation = FVector(
			FMath::Sin(Time * 41.f + Phase.X),
			FMath::Sin(Time * 33.f + Phase.Y),
			FMath::Sin(Time * 47.f + Phase.Z)) * (2.5f * Envelope);

		OutRotation = FRotator(
			FMath::Sin(Time * 37.f + Phase.Y) * 1.1f * Envelope,
			FMath::Sin(Time * 29.f + Phase.Z) * 0.8f * Envelope,
			FMath::Sin(Time * 31.f + Phase.X) * 1.7f * Envelope);
	}

private:
	float CurrentStrength() const
	{
		return Strength <= 0.f ? 0.f : Strength * FMath::Square(1.f - Time / Duration);
	}

	float Strength = 0.f;
	float Duration = 0.f;
	float Time = 0.f;
	FVector Phase = FVector::ZeroVector;
};

/**
 * Drives the "dizzy" post process after a hit: a quick rise, then a long eased decay with a slow pulse in it, like a
 * heartbeat. Tick returns the intensity (0 = off, 1 = full) to hand to the post process material.
 */
struct FFishHitEffect
{
	void Add(float InStrength, float InDuration)
	{
		Strength = FMath::Max(Strength * (1.f - Age / FMath::Max(Duration, 0.01f)), InStrength);
		Duration = FMath::Max(InDuration, 0.2f);
		Age = 0.f;
		Active = true;
	}

	float Tick(float Dt)
	{
		if (!Active)
		{
			return 0.f;
		}

		Age += Dt;
		if (Age >= Duration)
		{
			Active = false;
			Strength = 0.f;
			return 0.f;
		}

		const float Rise = FMath::Clamp(Age / 0.07f, 0.f, 1.f);
		const float Decay = FMath::Pow(1.f - Age / Duration, 1.6f);
		const float Pulse = 0.84f + 0.16f * FMath::Sin(Age * 7.5f);
		return FMath::Clamp(Strength * Rise * Decay * Pulse, 0.f, 1.5f);
	}

private:
	float Strength = 0.f;
	float Duration = 1.f;
	float Age = 0.f;
	bool Active = false;
};
