// Copyright Epic Games, Inc. All Rights Reserved.

#include "FishAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "FishLocomotionInterface.h"
#include "GameFramework/Pawn.h"

void UFishAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwnerPawn = TryGetPawnOwner();
}

float UFishAnimInstance::TraceFootOffset(const FName& Bone) const
{
	const USkeletalMeshComponent* Mesh = GetOwningComponent();
	APawn* Pawn = OwnerPawn.Get();
	IFishLocomotionInterface* Locomotion = Cast<IFishLocomotionInterface>(Pawn);
	if (!Mesh || !Locomotion || !GetWorld())
	{
		return 0.f;
	}

	// Floor level the animation assumes: where the character believes the ground is.
	const float FloorZ = Locomotion->GetFloorZ();

	const FVector FootLocation = Mesh->GetSocketLocation(Bone);
	const FVector Start(FootLocation.X, FootLocation.Y, FloorZ + TraceUp);
	const FVector End(FootLocation.X, FootLocation.Y, FloorZ - TraceDown);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FootIK), false, Pawn);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, Params))
	{
		return Hit.ImpactPoint.Z - FloorZ;
	}
	return 0.f;
}

void UFishAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerPawn.IsValid())
	{
		OwnerPawn = TryGetPawnOwner();
	}
	APawn* Pawn = OwnerPawn.Get();
	IFishLocomotionInterface* Locomotion = Cast<IFishLocomotionInterface>(Pawn);
	if (!Locomotion)
	{
		return;
	}

	const FVector Velocity = Locomotion->GetLocomotionVelocity();
	const FVector Velocity2D(Velocity.X, Velocity.Y, 0.f);
	Speed = Velocity2D.Size();

	const FVector LocalVelocity = Pawn->GetActorRotation().UnrotateVector(Velocity2D);
	Direction = Speed > 1.f ? FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X)) : 0.f;

	bShouldMove = (Speed > 3.f && Locomotion->HasMoveInput()) || Speed > 40.f;
	bIsFalling = Locomotion->IsAirborne();
	bIsJumping = bIsFalling && Locomotion->IsJumping();
	bIsRunning = Locomotion->IsRunningLocomotion();
	bIsAiming = Locomotion->IsAiming();
	AimPitch = FMath::FInterpTo(AimPitch, FMath::Clamp(Locomotion->GetAimPitch(), -90.f, 90.f), DeltaSeconds, AimInterpSpeed);
	AimYaw = FMath::FInterpTo(AimYaw, FMath::Clamp(Locomotion->GetAimYaw(), -90.f, 90.f), DeltaSeconds, AimInterpSpeed);

	bTurningInPlace = Locomotion->IsTurningInPlaceLocomotion();
	TurnInPlaceRate = Locomotion->GetTurnInPlaceRateLocomotion();
	TurnPlayRate = FMath::GetMappedRangeValueClamped(FVector2D(0.0, 150.0), FVector2D(0.35, 0.8), FMath::Abs(TurnInPlaceRate));

	// ------------------------------------------------------------ foot IK
	const float Target = bIsFalling ? 0.f : 1.f;
	FootIKAlpha = FMath::FInterpTo(FootIKAlpha, Target, DeltaSeconds, 10.f);

	float WantedLeft = 0.f;
	float WantedRight = 0.f;
	if (!bIsFalling)
	{
		WantedLeft = TraceFootOffset(LeftFootBone);
		WantedRight = TraceFootOffset(RightFootBone);
	}
	LeftOffset = FMath::FInterpTo(LeftOffset, WantedLeft, DeltaSeconds, FootInterpSpeed);
	RightOffset = FMath::FInterpTo(RightOffset, WantedRight, DeltaSeconds, FootInterpSpeed);

	// Pelvis follows the lower foot so a foot on a lower step can still reach it.
	const float WantedPelvis = FMath::Min(0.f, FMath::Min(WantedLeft, WantedRight));
	PelvisZ = FMath::FInterpTo(PelvisZ, WantedPelvis, DeltaSeconds, PelvisInterpSpeed);

	// The IK effectors are expressed in the local space of the ik_foot bones (pose target, not moved by the leg IK),
	// so the world-up offset has to be rotated into that bone's frame.
	const USkeletalMeshComponent* Mesh = GetOwningComponent();
	const FQuat LeftIKRotation = Mesh ? Mesh->GetSocketTransform(LeftIKBone, RTS_Component).GetRotation() : FQuat::Identity;
	const FQuat RightIKRotation = Mesh ? Mesh->GetSocketTransform(RightIKBone, RTS_Component).GetRotation() : FQuat::Identity;
	LeftFootEffectorOffset = LeftIKRotation.UnrotateVector(FVector(0.f, 0.f, LeftOffset));
	RightFootEffectorOffset = RightIKRotation.UnrotateVector(FVector(0.f, 0.f, RightOffset));
	PelvisOffset = FVector(0.f, 0.f, PelvisZ);
}
