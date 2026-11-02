// Copyright Epic Games, Inc. All Rights Reserved.

#include "FishCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"

AFishCharacter::AFishCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 90.f);

	// The body is turned by hand in Tick (strafe style), not by the movement component.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// Mannequin meshes face +Y and have their origin at the feet.
	GetMesh()->SetRelativeLocation(FVector(0.0, 0.0, -90.0));
	GetMesh()->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(RootComponent);
	CameraArm->TargetArmLength = CameraDistance;
	CameraArm->SocketOffset = FVector(0.0, 0.0, 60.0);
	CameraArm->bUsePawnControlRotation = true;
	CameraArm->bEnableCameraLag = true;
	CameraArm->CameraLagSpeed = 12.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);

	ApplyMovementSettings();
}

bool AFishCharacter::HasMoveInput() const
{
	return !GetCharacterMovement()->GetCurrentAcceleration().IsNearlyZero();
}

bool AFishCharacter::IsAirborne() const
{
	return GetCharacterMovement()->IsFalling();
}

bool AFishCharacter::IsJumping() const
{
	return GetCharacterMovement()->IsFalling() && GetVelocity().Z > 0.f;
}

float AFishCharacter::GetFloorZ() const
{
	return GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

void AFishCharacter::ApplyMovementSettings()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;
	Move->bUseControllerDesiredRotation = false;
	Move->MaxAcceleration = Acceleration;
	Move->BrakingDecelerationWalking = BrakingDeceleration;
	Move->GroundFriction = GroundFriction;
	Move->BrakingFrictionFactor = 1.f;
	Move->bUseSeparateBrakingFriction = true;
	Move->Mass = BodyMass;
	Move->JumpZVelocity = JumpVelocity;
	Move->GravityScale = GravityScale;
	Move->AirControl = 0.2f;
	Move->bEnablePhysicsInteraction = true;
	Move->MaxWalkSpeed = WalkSpeed;
	Move->NavAgentProps.bCanCrouch = false;
}

void AFishCharacter::BeginPlay()
{
	Super::BeginPlay();

	ApplyMovementSettings();
	TargetMaxSpeed = WalkSpeed;
	CameraArm->TargetArmLength = CameraDistance;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->ViewPitchMin = CameraPitchMin;
			PC->PlayerCameraManager->ViewPitchMax = CameraPitchMax;
		}
		PC->SetControlRotation(FRotator(-10.f, GetActorRotation().Yaw, 0.f));
	}
}

void AFishCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxisKey(EKeys::MouseX, this, &AFishCharacter::LookYaw);
	PlayerInputComponent->BindAxisKey(EKeys::MouseY, this, &AFishCharacter::LookPitch);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_RightX, this, &AFishCharacter::LookYaw);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_RightY, this, &AFishCharacter::LookPitch);
}

void AFishCharacter::LookYaw(float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		AddControllerYawInput(Value * LookSensitivity);
	}
}

void AFishCharacter::LookPitch(float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		AddControllerPitchInput(Value * LookSensitivity);
	}
}

void AFishCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	const auto Down = [PC](const FKey& Key) { return PC->IsInputKeyDown(Key) ? 1.f : 0.f; };

	float Forward = Down(EKeys::W) + Down(EKeys::Up) - Down(EKeys::S) - Down(EKeys::Down);
	float Right = Down(EKeys::D) + Down(EKeys::Right) - Down(EKeys::A) - Down(EKeys::Left);
	Forward = FMath::Clamp(Forward + PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY), -1.f, 1.f);
	Right = FMath::Clamp(Right + PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX), -1.f, 1.f);

	bWantsToRun = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::Gamepad_LeftThumbstick);
	if (PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom))
	{
		Jump();
	}

	const FRotator ControlYawRotation(0.f, PC->GetControlRotation().Yaw, 0.f);
	const FVector MoveDir = ControlYawRotation.RotateVector(FVector(Forward, Right, 0.f));
	if (!MoveDir.IsNearlyZero())
	{
		AddMovementInput(MoveDir.GetClampedToMaxSize(1.f));
	}

	// Walk / run blend, so the top speed eases between the two instead of jumping.
	UCharacterMovementComponent* Move = GetCharacterMovement();
	TargetMaxSpeed = bWantsToRun ? RunSpeed : WalkSpeed;
	Move->MaxWalkSpeed = FMath::FInterpTo(Move->MaxWalkSpeed, TargetMaxSpeed, DeltaSeconds, SpeedChangeRate);

	// Body rotation.
	const float ControlYaw = PC->GetControlRotation().Yaw;
	const float ActorYaw = GetActorRotation().Yaw;
	const float Delta = FMath::FindDeltaAngleDegrees(ActorYaw, ControlYaw);
	const bool bMoving = !MoveDir.IsNearlyZero() || GetVelocity().SizeSquared2D() > 25.f * 25.f || Move->IsFalling();

	if (bMoving)
	{
		bTurningInPlace = false;
		TurnRateCurrent = 0.f;
		SetActorRotation(FRotator(0.f, FMath::FixedTurn(ActorYaw, ControlYaw, BodyTurnSpeed * DeltaSeconds), 0.f));
		return;
	}

	if (!bTurningInPlace && FMath::Abs(Delta) > TurnInPlaceStartAngle)
	{
		bTurningInPlace = true;
	}

	if (bTurningInPlace)
	{
		// Slow down when close to the target so the turn settles instead of snapping.
		const float Remaining = FMath::Abs(Delta);
		const float SpeedScale = FMath::Clamp(Remaining / 30.f, 0.25f, 1.f);
		const float TargetRate = FMath::Sign(Delta) * TurnInPlaceSpeed * SpeedScale;
		TurnRateCurrent = FMath::FInterpTo(TurnRateCurrent, TargetRate, DeltaSeconds, TurnInPlaceResponse);

		const float Step = FMath::Clamp(TurnRateCurrent * DeltaSeconds, -Remaining, Remaining);
		SetActorRotation(FRotator(0.f, ActorYaw + Step, 0.f));

		if (Remaining - FMath::Abs(Step) < TurnInPlaceStopAngle)
		{
			bTurningInPlace = false;
			TurnRateCurrent = 0.f;
		}
	}
	else
	{
		TurnRateCurrent = 0.f;
	}
}
