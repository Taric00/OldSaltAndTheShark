// Copyright Epic Games, Inc. All Rights Reserved.

#include "FishPhysicsCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "DrawDebugHelpers.h"
#include "FishCollision.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "BoatPawn.h"
#include "EngineUtils.h"
#include "Harpoon.h"
#include "WaterSplash.h"
#include "Shark.h"

AFishPhysicsCharacter::AFishPhysicsCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(34.f, 90.f);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
	Capsule->SetSimulatePhysics(true);
	Capsule->SetEnableGravity(true);
	Capsule->SetLinearDamping(0.f);
	Capsule->SetAngularDamping(4.f);
	Capsule->SetMassOverrideInKg(NAME_None, BodyMass, true);
	Capsule->BodyInstance.bUseCCD = true;
	// Stay upright: only yaw is free. Yaw itself is steered through angular velocity.
	Capsule->SetConstraintMode(EDOFMode::SixDOF);
	Capsule->BodyInstance.bLockXRotation = true;
	Capsule->BodyInstance.bLockYRotation = true;
	Capsule->BodyInstance.bLockZRotation = false;
	RootComponent = Capsule;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh0"));
	Mesh->SetupAttachment(Capsule);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeLocation(FVector(0.0, 0.0, -90.0));
	Mesh->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));
	Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);

	// Rifle slot: the shotgun mesh in the right hand. Hidden until the player aims.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShotgunMeshFinder(TEXT("/Game/Models/Guns/pistol/Shutgun.Shutgun"));
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(Mesh, WeaponSocket);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetRelativeLocation(RifleHandLocation);
	WeaponMesh->SetRelativeRotation(RifleHandRotation);
	WeaponMesh->SetRelativeScale3D(RifleHandBaseScale);
	WeaponMesh->SetVisibility(false);
	if (ShotgunMeshFinder.Succeeded())
	{
		WeaponMesh->SetStaticMesh(ShotgunMeshFinder.Object);
	}
	else if (CubeMesh.Succeeded())
	{
		WeaponMesh->SetStaticMesh(CubeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HarpoonBodyMeshFinder(TEXT("/Game/Models/Guns/Harpoon/HarpoonGovde.HarpoonGovde"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HarpoonBulletMeshFinder(TEXT("/Game/Models/Guns/Harpoon/HarpoonMermi.HarpoonMermi"));

	// Harpoon gun body in the hand: stays in hand whether loaded or not.
	HarpoonHandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HarpoonHandMesh"));
	HarpoonHandMesh->SetupAttachment(Mesh, WeaponSocket);
	HarpoonHandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HarpoonHandMesh->SetRelativeLocation(HarpoonHandLocation);
	HarpoonHandMesh->SetRelativeRotation(HarpoonHandRotation);
	HarpoonHandMesh->SetRelativeScale3D(HarpoonHandBaseScale);
	HarpoonHandMesh->SetVisibility(false);

	// The loaded bolt: a separate piece so it alone can be hidden to show the gun is out of ammo.
	HarpoonAmmoMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HarpoonAmmoMesh"));
	HarpoonAmmoMesh->SetupAttachment(Mesh, WeaponSocket);
	HarpoonAmmoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HarpoonAmmoMesh->SetRelativeLocation(HarpoonAmmoLocation);
	HarpoonAmmoMesh->SetRelativeRotation(HarpoonAmmoRotation);
	HarpoonAmmoMesh->SetRelativeScale3D(HarpoonAmmoBaseScale);
	HarpoonAmmoMesh->SetVisibility(false);

	// The weapon that is not in use hangs on a strap on the back. The strap swings; the weapons sit on it.
	BackSling = CreateDefaultSubobject<USceneComponent>(TEXT("BackSling"));
	BackSling->SetupAttachment(Mesh, BackSocket);

	BackHarpoonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackHarpoonMesh"));
	BackHarpoonMesh->SetupAttachment(BackSling);
	BackHarpoonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackHarpoonMesh->SetCastShadow(false);

	BackRifleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackRifleMesh"));
	BackRifleMesh->SetupAttachment(BackSling);
	BackRifleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackRifleMesh->SetCastShadow(false);
	BackRifleMesh->SetVisibility(false);

	// Rifle shot streak: a cylinder stretched from the muzzle to the hit, in world space.
	TracerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TracerMesh"));
	TracerMesh->SetupAttachment(Capsule);
	TracerMesh->SetUsingAbsoluteLocation(true);
	TracerMesh->SetUsingAbsoluteRotation(true);
	TracerMesh->SetUsingAbsoluteScale(true);
	TracerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TracerMesh->SetCastShadow(false);
	TracerMesh->SetVisibility(false);

	if (HarpoonBodyMeshFinder.Succeeded())
	{
		HarpoonHandMesh->SetStaticMesh(HarpoonBodyMeshFinder.Object);
		BackHarpoonMesh->SetStaticMesh(HarpoonBodyMeshFinder.Object);
	}
	else if (CylinderMesh.Succeeded())
	{
		HarpoonHandMesh->SetStaticMesh(CylinderMesh.Object);
		BackHarpoonMesh->SetStaticMesh(CylinderMesh.Object);
	}
	if (HarpoonBulletMeshFinder.Succeeded())
	{
		HarpoonAmmoMesh->SetStaticMesh(HarpoonBulletMeshFinder.Object);
	}
	else if (CylinderMesh.Succeeded())
	{
		HarpoonAmmoMesh->SetStaticMesh(CylinderMesh.Object);
	}
	if (CylinderMesh.Succeeded())
	{
		TracerMesh->SetStaticMesh(CylinderMesh.Object);
	}
	if (ShotgunMeshFinder.Succeeded())
	{
		BackRifleMesh->SetStaticMesh(ShotgunMeshFinder.Object);
	}
	else if (CubeMesh.Succeeded())
	{
		BackRifleMesh->SetStaticMesh(CubeMesh.Object);
	}

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(Capsule);
	CameraArm->TargetArmLength = CameraDistance;
	CameraArm->SocketOffset = FVector(0.0, 0.0, 60.0);
	CameraArm->bUsePawnControlRotation = true;
	CameraArm->bEnableCameraLag = true;
	CameraArm->CameraLagSpeed = 14.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);

	bUseControllerRotationYaw = false;
}

void AFishPhysicsCharacter::BeginPlay()
{
	Super::BeginPlay();

	Capsule->SetMassOverrideInKg(NAME_None, BodyMass, true);
	CameraArm->TargetArmLength = CameraDistance;
	CurrentMaxSpeed = WalkSpeed;
	BackSling->SetRelativeLocation(BackHarpoonOffset);
	BackHarpoonMesh->SetRelativeLocationAndRotation(FVector::ZeroVector, BackHarpoonRotation);
	BackHarpoonMesh->SetRelativeScale3D(BackHarpoonScale);
	BackRifleMesh->SetRelativeLocationAndRotation(FVector::ZeroVector, BackRifleRotation);
	BackRifleMesh->SetRelativeScale3D(BackRifleScale);

	// Re-apply the hand weapon transforms: properties can be tuned without touching the constructor.
	WeaponMesh->SetRelativeLocationAndRotation(RifleHandLocation, RifleHandRotation);
	WeaponMesh->SetRelativeScale3D(RifleHandBaseScale);
	HarpoonHandMesh->SetRelativeLocationAndRotation(HarpoonHandLocation, HarpoonHandRotation);
	HarpoonHandMesh->SetRelativeScale3D(HarpoonHandBaseScale);
	HarpoonAmmoMesh->SetRelativeLocationAndRotation(HarpoonAmmoLocation, HarpoonAmmoRotation);
	HarpoonAmmoMesh->SetRelativeScale3D(HarpoonAmmoBaseScale);

	HarpoonHandScale = HarpoonHandMesh->GetRelativeScale3D();
	HarpoonAmmoScale = HarpoonAmmoMesh->GetRelativeScale3D();
	RifleHandScale = WeaponMesh->GetRelativeScale3D();
	PrevBodyYaw = GetActorRotation().Yaw;
	if (TracerMaterial)
	{
		TracerMesh->SetMaterial(0, TracerMaterial);
	}
	MeshDrop = Capsule->GetScaledCapsuleHalfHeight();

	// The dizzy screen is a post process material on this camera, switched on only while it has an effect.
	if (HitEffectMaterial)
	{
		HitEffectMID = UMaterialInstanceDynamic::Create(HitEffectMaterial, this);
		Camera->PostProcessSettings.AddBlendable(HitEffectMID, 1.f);
		Camera->PostProcessBlendWeight = 0.f;
	}

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

void AFishPhysicsCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxisKey(EKeys::MouseX, this, &AFishPhysicsCharacter::LookYaw);
	PlayerInputComponent->BindAxisKey(EKeys::MouseY, this, &AFishPhysicsCharacter::LookPitch);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_RightX, this, &AFishPhysicsCharacter::LookYaw);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_RightY, this, &AFishPhysicsCharacter::LookPitch);
}

void AFishPhysicsCharacter::LookYaw(float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		AddControllerYawInput(Value * LookSensitivity);
	}
}

void AFishPhysicsCharacter::LookPitch(float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		AddControllerPitchInput(Value * LookSensitivity * (bInvertLookPitch ? -1.f : 1.f));
	}
}

FVector AFishPhysicsCharacter::GetVelocity() const
{
	return Capsule->GetPhysicsLinearVelocity();
}

FVector AFishPhysicsCharacter::GetLocomotionVelocity() const
{
	// Relative to the deck: standing still on a moving boat is idle, not walking.
	return Capsule->GetPhysicsLinearVelocity() - PlatformVelocity;
}

float AFishPhysicsCharacter::GetFloorZ() const
{
	if (bGrounded)
	{
		return GroundHit.ImpactPoint.Z;
	}
	return Capsule->GetComponentLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
}

void AFishPhysicsCharacter::ReadInput(float& OutForward, float& OutRight, bool& bOutJump)
{
	OutForward = 0.f;
	OutRight = 0.f;
	bOutJump = false;
	bWantsToRun = false;

	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	const auto Down = [PC](const FKey& Key) { return PC->IsInputKeyDown(Key) ? 1.f : 0.f; };

	OutForward = Down(EKeys::W) + Down(EKeys::Up) - Down(EKeys::S) - Down(EKeys::Down);
	OutRight = Down(EKeys::D) + Down(EKeys::Right) - Down(EKeys::A) - Down(EKeys::Left);
	OutForward = FMath::Clamp(OutForward + PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY), -1.f, 1.f);
	OutRight = FMath::Clamp(OutRight + PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX), -1.f, 1.f);

	bWantsToRun = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::Gamepad_LeftThumbstick);
	bOutJump = PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom);
}

void AFishPhysicsCharacter::UpdateGround()
{
	const UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();

	const FVector Center = Capsule->GetComponentLocation();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Reach = HalfHeight + RideHeight + GroundSnapDistance + 40.f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FishGround), false, this);
	FHitResult Hit;
	const bool bHit = World->SweepSingleByChannel(
		Hit, Center, Center - FVector(0.f, 0.f, Reach), FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(GroundProbeRadius), Params);

	bGrounded = false;
	if (bHit && !Hit.bStartPenetrating)
	{
		const float WalkableZ = FMath::Cos(FMath::DegreesToRadians(MaxSlopeAngle));
		const float Distance = Center.Z - Hit.ImpactPoint.Z;
		if (Hit.ImpactNormal.Z >= WalkableZ && Distance <= HalfHeight + RideHeight + GroundSnapDistance && Now >= JumpSuppressUntil)
		{
			bGrounded = true;
			GroundHit = Hit;
			GroundDistance = Distance;
		}
	}

	if (bGrounded)
	{
		bJumping = false;

		// Standing on a moving deck? Then everything below is relative to it. In the air the last platform is
		// kept, so a jump from a moving boat does not lose the boat's speed.
		AActor* HitActor = GroundHit.GetActor();
		SetPlatform(Cast<IFishPlatform>(HitActor) ? HitActor : nullptr);
	}
}

void AFishPhysicsCharacter::SetPlatform(AActor* NewPlatform)
{
	if (PlatformActor == NewPlatform)
	{
		return;
	}

	// The platform must move before we read its velocity.
	if (PlatformActor)
	{
		RemoveTickPrerequisiteActor(PlatformActor);
	}
	PlatformActor = NewPlatform;
	if (PlatformActor)
	{
		AddTickPrerequisiteActor(PlatformActor);
	}

	const IFishPlatform* Platform = Cast<IFishPlatform>(PlatformActor.Get());
	PlatformVelocity = Platform ? Platform->GetPlatformVelocityAtLocation(Capsule->GetComponentLocation()) : FVector::ZeroVector;
	PrevPlatformVelocity = PlatformVelocity;
	PlatformAccelFiltered = FVector::ZeroVector;
}

void AFishPhysicsCharacter::ApplyMovementForces(const FVector& MoveInput, float Dt)
{
	const float Mass = Capsule->GetMass();
	const FVector Velocity = Capsule->GetPhysicsLinearVelocity();

	const FVector Normal = bGrounded ? FVector(GroundHit.ImpactNormal) : FVector::UpVector;
	const float InputSize = MoveInput.Size();

	// Wanted velocity follows the slope, so walking uphill and downhill keeps its ground speed.
	FVector DesiredDir = MoveInput;
	if (bGrounded && InputSize > KINDA_SMALL_NUMBER)
	{
		DesiredDir = FVector::VectorPlaneProject(MoveInput, Normal).GetSafeNormal() * InputSize;
	}

	// Moving deck: aim for the deck's velocity plus our own walking, and push along with the deck's acceleration.
	PlatformYawRate = 0.f;
	if (const IFishPlatform* Platform = Cast<IFishPlatform>(PlatformActor.Get()))
	{
		PlatformVelocity = Platform->GetPlatformVelocityAtLocation(Capsule->GetComponentLocation());
		PlatformYawRate = Platform->GetPlatformYawRate();

		const FVector RawAccel = (PlatformVelocity - PrevPlatformVelocity) / FMath::Max(Dt, KINDA_SMALL_NUMBER);
		PlatformAccelFiltered = FMath::VInterpTo(PlatformAccelFiltered, RawAccel, Dt, PlatformAccelerationSmoothing);
		if (bGrounded)
		{
			Capsule->AddForce(FVector(PlatformAccelFiltered.X, PlatformAccelFiltered.Y, 0.f) * Mass, NAME_None, false);
		}
	}
	else
	{
		PlatformVelocity = FVector::ZeroVector;
		PlatformAccelFiltered = FVector::ZeroVector;
	}
	PrevPlatformVelocity = PlatformVelocity;

	const FVector Desired = FVector(PlatformVelocity.X, PlatformVelocity.Y, 0.f) + DesiredDir * CurrentMaxSpeed;

	FVector Error = Desired - Velocity;
	float Gain = VelocityGain;
	float Limit = InputSize > KINDA_SMALL_NUMBER ? MaxAcceleration : MaxBraking;

	if (bGrounded)
	{
		// Only correct along the ground plane; the spring owns the vertical axis.
		Error = FVector::VectorPlaneProject(Error, Normal);
	}
	else
	{
		Error.Z = 0.f;
		Gain *= AirControl;
		Limit *= AirControl;
	}

	const FVector Acceleration = (Error * Gain).GetClampedToMaxSize(Limit);
	Capsule->AddForce(Acceleration * Mass, NAME_None, false);

	// Ride-height spring: pushes up only, damped, so the capsule settles without bouncing.
	if (bGrounded)
	{
		const float Target = Capsule->GetScaledCapsuleHalfHeight() + RideHeight;
		const float Compression = Target - GroundDistance;
		const float SpringAccel = FMath::Clamp(SpringStiffness * Compression - SpringDamping * (Velocity.Z - PlatformVelocity.Z), 0.f, MaxSpringAcceleration);
		Capsule->AddForce(FVector(0.f, 0.f, SpringAccel * Mass), NAME_None, false);
	}

	// Heavier fall than the world gravity alone.
	if (!FMath::IsNearlyEqual(GravityScale, 1.f))
	{
		const float Gravity = GetWorld()->GetGravityZ();
		Capsule->AddForce(FVector(0.f, 0.f, Gravity * (GravityScale - 1.f) * Mass), NAME_None, false);
	}
}

void AFishPhysicsCharacter::ApplyYaw(const FVector& MoveInput, float Dt)
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	const float ControlYaw = PC->GetControlRotation().Yaw;
	const float ActorYaw = GetActorRotation().Yaw;
	const float Delta = FMath::FindDeltaAngleDegrees(ActorYaw, ControlYaw);
	const FVector Velocity = Capsule->GetPhysicsLinearVelocity() - PlatformVelocity;
	const bool bMoving = bHasMoveInput || Velocity.SizeSquared2D() > 25.f * 25.f || !bGrounded;

	float YawRate = 0.f;
	if (bAiming)
	{
		// Aiming: the body always faces where the camera looks.
		bTurningInPlace = false;
		TurnRateCurrent = 0.f;
		YawRate = FMath::Clamp(Delta * 16.f, -BodyTurnSpeed * 1.5f, BodyTurnSpeed * 1.5f);
	}
	else if (bOrientToMovement)
	{
		// Face where the input points; stand still (no turn in place) when there is no input.
		bTurningInPlace = false;
		TurnRateCurrent = 0.f;
		if (bHasMoveInput)
		{
			const float MoveYaw = MoveInput.Rotation().Yaw;
			const float MoveDelta = FMath::FindDeltaAngleDegrees(ActorYaw, MoveYaw);
			YawRate = FMath::Clamp(MoveDelta * 14.f, -OrientRotationSpeed, OrientRotationSpeed);
		}
	}
	else if (bMoving)
	{
		bTurningInPlace = false;
		TurnRateCurrent = 0.f;
		YawRate = FMath::Clamp(Delta * 14.f, -BodyTurnSpeed, BodyTurnSpeed);
	}
	else
	{
		if (!bTurningInPlace && FMath::Abs(Delta) > TurnInPlaceStartAngle)
		{
			bTurningInPlace = true;
		}

		if (bTurningInPlace)
		{
			const float Remaining = FMath::Abs(Delta);
			const float SpeedScale = FMath::Clamp(Remaining / 30.f, 0.25f, 1.f);
			const float TargetRate = FMath::Sign(Delta) * TurnInPlaceSpeed * SpeedScale;
			TurnRateCurrent = FMath::FInterpTo(TurnRateCurrent, TargetRate, Dt, TurnInPlaceResponse);
			YawRate = TurnRateCurrent;

			if (Remaining < TurnInPlaceStopAngle || Remaining < FMath::Abs(TurnRateCurrent) * Dt)
			{
				bTurningInPlace = false;
				TurnRateCurrent = 0.f;
				YawRate = Delta / FMath::Max(Dt, KINDA_SMALL_NUMBER);
				YawRate = FMath::Clamp(YawRate, -TurnInPlaceSpeed, TurnInPlaceSpeed);
			}
		}
	}

	// A turning deck turns the body with it.
	if (bGrounded && PlatformActor)
	{
		YawRate += PlatformYawRate;
	}

	Capsule->SetPhysicsAngularVelocityInDegrees(FVector(0.f, 0.f, YawRate));
}

void AFishPhysicsCharacter::UpdateMesh(float Dt)
{
	// Relative to the deck, so a moving boat does not make the character lean.
	const FVector Velocity = Capsule->GetPhysicsLinearVelocity() - PlatformVelocity;
	const FVector Accel = (Velocity - PreviousVelocity) / FMath::Max(Dt, KINDA_SMALL_NUMBER);
	PreviousVelocity = Velocity;

	// Lean from how hard the body is being pushed (in the body's own forward / right axes).
	const FRotator YawOnly(0.f, GetActorRotation().Yaw, 0.f);
	const FVector LocalAccel = YawOnly.UnrotateVector(FVector(Accel.X, Accel.Y, 0.f));
	LeanAccelFiltered = FMath::Vector2DInterpTo(LeanAccelFiltered, FVector2D(LocalAccel.X, LocalAccel.Y), Dt, 10.f);

	const FVector2D Target(
		FMath::Clamp(-LeanAccelFiltered.X * LeanPitchPerAcceleration, -MaxLean, MaxLean),
		FMath::Clamp(LeanAccelFiltered.Y * LeanRollPerAcceleration, -MaxLean, MaxLean));

	// Spring-damper per axis: leans in, overshoots slightly, settles.
	LeanVelocity += ((Target - Lean) * LeanStiffness - LeanVelocity * LeanDamping) * Dt;
	Lean += LeanVelocity * Dt;

	// Plant the feet on the ground whatever the capsule spring is doing.
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float DropTarget = bGrounded ? GroundDistance : HalfHeight + RideHeight;
	MeshDrop = FMath::FInterpTo(MeshDrop, DropTarget, Dt, MeshHeightResponse);

	// Lean about the feet, then the usual Mannequin yaw of -90.
	const FQuat LeanQuat = FRotator(Lean.X, 0.f, Lean.Y).Quaternion();
	const FQuat MeshYaw = FRotator(0.f, -90.f, 0.f).Quaternion();
	Mesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -MeshDrop), LeanQuat * MeshYaw);
}

void AFishPhysicsCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// A destroyed harpoon stays a non-null pointer until garbage collection: forget it right away.
	if (ActiveHarpoon && !IsValid(ActiveHarpoon))
	{
		ActiveHarpoon = nullptr;
	}

	const float Dt = FMath::Clamp(DeltaSeconds, 0.001f, 0.05f);

	if (bInVehicle)
	{
		TickInVehicle(Dt);
		return;
	}

	float Forward = 0.f;
	float Right = 0.f;
	bool bJumpPressed = false;
	ReadInput(Forward, Right, bJumpPressed);

	const APlayerController* PC = Cast<APlayerController>(GetController());
	const bool bInteractPressed = PC && GetWorld()->GetTimeSeconds() >= InteractCooldownUntil
		&& (PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Left));
	const FRotator ControlYaw(0.f, PC ? PC->GetControlRotation().Yaw : 0.f, 0.f);
	const FVector MoveInput = ControlYaw.RotateVector(FVector(Forward, Right, 0.f)).GetClampedToMaxSize(1.f);
	bHasMoveInput = !MoveInput.IsNearlyZero(0.05f);

	// Right mouse button held = aim: slower, strafing, camera over the shoulder.
	bAiming = PC && (PC->IsInputKeyDown(EKeys::RightMouseButton) || PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis) > 0.3f);
	if (bAiming)
	{
		bWantsToRun = false;
	}
	UpdateAim(Dt);

	CurrentMaxSpeed = FMath::FInterpTo(CurrentMaxSpeed, bAiming ? AimWalkSpeed : (bWantsToRun ? RunSpeed : WalkSpeed), Dt, SpeedChangeRate);

	UpdateGround();

	if (bJumpPressed && bGrounded)
	{
		FVector Velocity = Capsule->GetPhysicsLinearVelocity();
		Velocity.Z = PlatformVelocity.Z + JumpVelocity;
		Capsule->SetPhysicsLinearVelocity(Velocity);
		JumpSuppressUntil = GetWorld()->GetTimeSeconds() + 0.18f;
		bGrounded = false;
		bJumping = true;
	}
	if (bJumping && Capsule->GetPhysicsLinearVelocity().Z <= PlatformVelocity.Z)
	{
		bJumping = false;
	}

	ApplyMovementForces(MoveInput, Dt);
	ApplyYaw(MoveInput, Dt);
	UpdateMesh(Dt);

	if (bShowDebug && GEngine)
	{
		const FVector Velocity = Capsule->GetPhysicsLinearVelocity();
		GEngine->AddOnScreenDebugMessage(
			reinterpret_cast<uint64>(this), 0.f, FColor::Cyan,
			FString::Printf(TEXT("Speed %.0f  Vz %.0f  Grounded %d  Dist %.1f  Lean %.1f/%.1f"),
				Velocity.Size2D(), Velocity.Z, bGrounded ? 1 : 0, GroundDistance, Lean.X, Lean.Y));
	}

	// A thrown harpoon is spent until it is gone (missed, or the rope was cut); then the next one takes a moment.
	if (ActiveHarpoon && ActiveHarpoon->DidHitShark())
	{
		if (!bLastHarpoonHit)
		{
			HitMarkerUntil = GetWorld()->GetTimeSeconds() + HitMarkerDuration;
		}
		bLastHarpoonHit = true;
	}
	if (bHarpoonWasOut && !ActiveHarpoon)
	{
		// A miss costs the reload time; after a hit the rope was already spent, so the next harpoon is ready at once.
		bHarpoonWasOut = false;
		FireCooldownUntil = GetWorld()->GetTimeSeconds() + (bLastHarpoonHit ? ReloadTimeAfterHit : ReloadTime);
	}
	if (ActiveHarpoon && PC && PC->WasInputKeyJustPressed(CutRopeKey))
	{
		ActiveHarpoon->Release();
	}

	UpdateWeapons(PC, Dt);
	UpdateBackSwing(Dt);

	// Last: using something can hand the player to another pawn.
	UpdateInteraction(bInteractPressed, Dt);
}

void AFishPhysicsCharacter::UpdateWeapons(const APlayerController* PC, float Dt)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!PC)
	{
		return;
	}

	// 1 / 2: pick the weapon. The one in hand goes to the back and the other comes to the hand.
	const bool bWantHarpoon = PC->WasInputKeyJustPressed(HarpoonKey);
	const bool bWantRifle = PC->WasInputKeyJustPressed(RifleKey);
	if ((bWantHarpoon && bRifleSelected) || (bWantRifle && !bRifleSelected))
	{
		bRifleSelected = bWantRifle;
		SwitchUntil = Now + SwitchTime;
		BackSwingVelocity += FVector2D(0.f, 140.f); // the strap jumps when a weapon is slung
	}

	const bool bTrigger = PC->WasInputKeyJustPressed(EKeys::LeftMouseButton) || PC->WasInputKeyJustPressed(EKeys::Gamepad_RightTrigger);
	if (!bTrigger || Now < SwitchUntil || !(bAiming || !bRequireAimToFire))
	{
		return;
	}

	if (bRifleSelected)
	{
		if (Now >= RifleReadyAt)
		{
			RifleReadyAt = Now + RifleCooldown;
			FireRifle();
		}
	}
	else if (!ActiveHarpoon && Now >= FireCooldownUntil)
	{
		FireCooldownUntil = Now + FireCooldown;
		FireHarpoon();
	}
}

void AFishPhysicsCharacter::FireRifle()
{
	// Through the middle of the screen, from the camera; the streak is drawn from the muzzle to where it ended.
	const FVector Start = Camera->GetComponentLocation();
	const FVector Forward = Camera->GetForwardVector();
	const FVector End = Start + Forward * TraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FishRifle), false, this);
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Shark, Params);
	const FVector ShotEnd = bHit ? FVector(Hit.ImpactPoint) : End;

	// Forward/right/up from the character, not the weapon mesh's own (scaled, oddly-rotated) local axes:
	// that way the offset is real world centimeters and does not drift with the hand-pose scale pop.
	const FVector Muzzle = WeaponMesh->GetComponentLocation()
		+ GetActorForwardVector() * RifleMuzzleOffset.X
		+ GetActorRightVector() * RifleMuzzleOffset.Y
		+ GetActorUpVector() * RifleMuzzleOffset.Z;

	ABoatPawn* Boat = nullptr;
	for (TActorIterator<ABoatPawn> It(GetWorld()); It; ++It)
	{
		Boat = *It;
		break;
	}

	// Where the bullet goes into the water: a little foam splash there.
	FVector WaterEntry = ShotEnd;
	bool bEntersWater = false;
	if (Boat && Forward.Z < -0.01f)
	{
		const float WaterZ = Boat->GetSurfaceZ(FVector2D(Start.X, Start.Y));
		const float T = (WaterZ - Start.Z) / Forward.Z;
		if (T > 0.f && T < FVector::Dist(Start, ShotEnd))
		{
			WaterEntry = Start + Forward * T;
			bEntersWater = true;
			if (BulletSplashScale > 0.f)
			{
				const TSubclassOf<AWaterSplash> SplashClass = BulletSplashClass ? BulletSplashClass : TSubclassOf<AWaterSplash>(AWaterSplash::StaticClass());
				const FTransform SplashTransform(FRotator::ZeroRotator, WaterEntry);
				if (AWaterSplash* Splash = GetWorld()->SpawnActorDeferred<AWaterSplash>(SplashClass, SplashTransform))
				{
					Splash->Scale = BulletSplashScale;
					Splash->FinishSpawning(SplashTransform);
				}
			}
		}
	}

	if (bHit)
	{
		if (AShark* Shark = Cast<AShark>(Hit.GetActor()))
		{
			const float Depth = Boat ? Boat->GetSurfaceZ(FVector2D(Hit.ImpactPoint.X, Hit.ImpactPoint.Y)) - Hit.ImpactPoint.Z : 0.f;
			if (Depth <= RifleMaxDepth)
			{
				Shark->ApplyRifleHit(RifleDamage, Hit.ImpactPoint, Forward);
				HitMarkerUntil = GetWorld()->GetTimeSeconds() + HitMarkerDuration;
			}
		}
	}

	// The streak stops at the water: under it the bullet is not seen.
	TracerStart = Muzzle;
	TracerEnd = bEntersWater ? WaterEntry : ShotEnd;
	TracerAge = 0.f;

	if (bDrawDebugTrace)
	{
		DrawDebugLine(GetWorld(), Muzzle, ShotEnd, bHit ? FColor::Red : FColor::Silver, false, DebugTraceDuration, 0, 1.5f);
	}

	// Kick: shake plus the aim jumps up a little.
	CameraShake.Add(RifleShakeStrength, 0.16f);
	if (AController* MyController = GetController())
	{
		FRotator Control = MyController->GetControlRotation();
		Control.Pitch = FRotator::ClampAxis(Control.Pitch + RifleRecoilPitch);
		MyController->SetControlRotation(Control);
	}
}

void AFishPhysicsCharacter::FireHarpoon()
{
	// The aim line runs from the camera through the middle of the screen; the harpoon leaves the hand toward
	// the point where that line ends, so it lands where the crosshair is.
	const FVector Start = Camera->GetComponentLocation();
	const FVector Forward = Camera->GetForwardVector();
	const FVector End = Start + Forward * TraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FishAim), false, this);
	FHitResult Hit;
	const FVector AimPoint = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Shark, Params) ? FVector(Hit.ImpactPoint) : End;

	// The harpoon leaves from the hand that holds it.
	const FVector Muzzle = HarpoonHandMesh->GetComponentLocation();
	FVector Direction = (AimPoint - Muzzle).GetSafeNormal();
	if (FVector::DotProduct(Direction, Forward) < 0.6f)
	{
		Direction = Forward;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const TSubclassOf<AHarpoon> Class = HarpoonClass ? HarpoonClass : TSubclassOf<AHarpoon>(AHarpoon::StaticClass());
	ActiveHarpoon = GetWorld()->SpawnActor<AHarpoon>(Class, Muzzle, Direction.Rotation(), SpawnParams);
	if (ActiveHarpoon)
	{
		bHarpoonWasOut = true;
		bLastHarpoonHit = false;
		ActiveHarpoon->Launch(Direction);
		CameraShake.Add(ShotShakeStrength, 0.18f);
	}
}

void AFishPhysicsCharacter::UpdateInteraction(bool bInteractPressed, float Dt)
{
	InteractionScanTimer -= Dt;
	if (InteractionScanTimer <= 0.f)
	{
		InteractionScanTimer = InteractionScanInterval;

		TArray<AActor*> Candidates;
		UGameplayStatics::GetAllActorsWithInterface(this, UFishInteractable::StaticClass(), Candidates);

		AActor* Best = nullptr;
		float BestDistance = TNumericLimits<float>::Max();
		for (AActor* Candidate : Candidates)
		{
			const IFishInteractable* Interactable = Cast<IFishInteractable>(Candidate);
			if (!Interactable || !Interactable->CanInteract(this))
			{
				continue;
			}
			const float Distance = FVector::Dist(GetActorLocation(), Interactable->GetInteractionLocation());
			if (Distance <= Interactable->GetInteractionRadius() && Distance < BestDistance)
			{
				Best = Candidate;
				BestDistance = Distance;
			}
		}

		InteractTarget = Best;
		const IFishInteractable* BestInteractable = Cast<IFishInteractable>(Best);
		InteractionPrompt = BestInteractable ? BestInteractable->GetInteractionText() : FText::GetEmpty();
	}

	if (bInteractPressed && InteractTarget)
	{
		IFishInteractable* Interactable = Cast<IFishInteractable>(InteractTarget.Get());
		if (Interactable && Interactable->CanInteract(this))
		{
			InteractionPrompt = FText::GetEmpty();
			Interactable->Interact(this);
		}
	}
}

void AFishPhysicsCharacter::EnterVehicle(USceneComponent* Seat)
{
	if (!Seat || bInVehicle)
	{
		return;
	}

	bInVehicle = true;
	InteractTarget = nullptr;
	InteractionPrompt = FText::GetEmpty();
	InteractCooldownUntil = GetWorld()->GetTimeSeconds() + 0.4f;
	SetPlatform(nullptr);

	Capsule->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Capsule->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Capsule->SetSimulatePhysics(false);
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	AttachToComponent(Seat, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeLocation(FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight()));
	SetActorRelativeRotation(FRotator::ZeroRotator);

	PreviousVelocity = FVector::ZeroVector;
	Lean = FVector2D::ZeroVector;
	LeanVelocity = FVector2D::ZeroVector;
	LeanAccelFiltered = FVector2D::ZeroVector;
}

void AFishPhysicsCharacter::LeaveVehicle(const FTransform& ExitTransform, const FVector& InheritedVelocity, AActor* ExitedPlatform)
{
	if (!bInVehicle)
	{
		return;
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	SetActorLocationAndRotation(
		ExitTransform.GetLocation() + FVector(0.f, 0.f, HalfHeight + RideHeight + 2.f),
		FRotator(0.f, ExitTransform.Rotator().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);

	Capsule->SetCollisionProfileName(TEXT("Pawn"));
	Capsule->SetSimulatePhysics(true);
	Capsule->SetPhysicsLinearVelocity(InheritedVelocity);

	// Normally the deck is only recognised as a platform once the next ground trace finds it (UpdateGround, at
	// the top of the next Tick). That is one frame of SetPlatform(nullptr)'s "PlatformVelocity = 0" left over
	// from TickInVehicle, which both resets the walk/run animation to a dead sprint for a frame (it reads the
	// inherited speed as footwork) and, worse, keeps correcting the capsule toward a 0 platform speed on every
	// tick until the trace catches up — easy to lose against a fast boat. Claiming the boat as the platform here,
	// immediately, closes that gap: there is no frame where the character is both moving at the boat's speed and
	// not counted as standing on it.
	SetPlatform(ExitedPlatform);
	PlatformVelocity = InheritedVelocity;
	PrevPlatformVelocity = InheritedVelocity;

	bInVehicle = false;
	MeshDrop = HalfHeight + RideHeight;
	InteractCooldownUntil = GetWorld()->GetTimeSeconds() + 0.4f;
	InteractionScanTimer = 0.f;
}

void AFishPhysicsCharacter::TickInVehicle(float Dt)
{
	// The seat owns the body. Report a calm, grounded idle so the animation settles.
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	bGrounded = true;
	bJumping = false;
	bHasMoveInput = false;
	bAiming = false;
	UpdateAim(Dt);
	bWantsToRun = false;
	bTurningInPlace = false;
	TurnRateCurrent = 0.f;
	PlatformVelocity = FVector::ZeroVector;
	GroundHit.ImpactPoint = Capsule->GetComponentLocation() - FVector(0.f, 0.f, HalfHeight);
	GroundDistance = HalfHeight;
	MeshDrop = HalfHeight;
	Mesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -HalfHeight), FRotator(0.f, -90.f, 0.f));
}

void AFishPhysicsCharacter::UpdateAim(float Dt)
{
	AimAlpha = FMath::FInterpTo(AimAlpha, bAiming ? 1.f : 0.f, Dt, AimResponse);

	// Over-the-shoulder camera that closes in and zooms while aiming.
	CameraArm->TargetArmLength = FMath::Lerp(CameraDistance, AimCameraDistance, AimAlpha);
	CameraArm->SocketOffset = FMath::Lerp(NormalSocketOffset, AimSocketOffset, AimAlpha);
	Camera->SetFieldOfView(FMath::Lerp(NormalFOV, AimFOV, AimAlpha));

	UpdateWeaponVisuals(Dt);

	// Bumps and recoil.
	FVector ShakeLocation;
	FRotator ShakeRotation;
	CameraShake.Tick(Dt, ShakeLocation, ShakeRotation);
	Camera->SetRelativeLocationAndRotation(ShakeLocation, ShakeRotation);

	if (HitEffectMID)
	{
		const float Dizzy = HitEffect.Tick(Dt);
		HitEffectMID->SetScalarParameterValue(TEXT("Intensity"), Dizzy);
		Camera->PostProcessBlendWeight = Dizzy > 0.001f ? 1.f : 0.f;
	}
}

void AFishPhysicsCharacter::UpdateWeaponVisuals(float Dt)
{
	const float Now = GetWorld()->GetTimeSeconds();

	// No UI: the weapon in hand is the chosen one, the other hangs on the back. A thrown or reloading harpoon is
	// simply missing from where it would be.
	const bool bHarpoonReady = !ActiveHarpoon && Now >= FireCooldownUntil;
	const bool bHandsFree = !bInVehicle;
	const bool bHarpoonInHand = bHandsFree && !bRifleSelected;
	// The gun body stays in hand even just after a shot; only the loaded bolt disappears until reloaded.
	HarpoonHandMesh->SetVisibility(bHarpoonInHand);
	HarpoonAmmoMesh->SetVisibility(bHarpoonInHand && bHarpoonReady);
	WeaponMesh->SetVisibility(bHandsFree && bRifleSelected);
	BackHarpoonMesh->SetVisibility((bRifleSelected || bInVehicle) && bHarpoonReady);
	BackRifleMesh->SetVisibility(!bRifleSelected || bInVehicle);

	// The new weapon pops into the hand during the change.
	const float Pop = SwitchTime > 0.f ? FMath::Clamp(1.f - (SwitchUntil - Now) / SwitchTime, 0.f, 1.f) : 1.f;
	const float PopScale = FMath::Lerp(0.4f, 1.f, FMath::InterpEaseOut(0.f, 1.f, Pop, 3.f));
	HarpoonHandMesh->SetRelativeScale3D(HarpoonHandScale * PopScale);
	HarpoonAmmoMesh->SetRelativeScale3D(HarpoonAmmoScale * PopScale);
	WeaponMesh->SetRelativeScale3D(RifleHandScale * PopScale);

	// Rifle streak: thins out over a few frames.
	TracerAge += Dt;
	const float TracerAlpha = TracerTime > 0.f ? 1.f - TracerAge / TracerTime : 0.f;
	const FVector Line = TracerEnd - TracerStart;
	if (TracerAlpha > 0.f && !Line.IsNearlyZero())
	{
		const float Thickness = TracerThickness * TracerAlpha / 100.f;
		TracerMesh->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZ(Line.GetSafeNormal()).Rotator(),
			(TracerStart + TracerEnd) * 0.5f, FVector(Thickness, Thickness, Line.Size() / 100.f)));
		TracerMesh->SetVisibility(true);
	}
	else
	{
		TracerMesh->SetVisibility(false);
	}
}

void AFishPhysicsCharacter::UpdateBackSwing(float Dt)
{
	// How fast the body turns (deg/s) and how hard it is pushed, in the body's own frame.
	const float BodyYaw = GetActorRotation().Yaw;
	const float TurnRate = FMath::FindDeltaAngleDegrees(PrevBodyYaw, BodyYaw) / FMath::Max(Dt, KINDA_SMALL_NUMBER);
	PrevBodyYaw = BodyYaw;

	const FVector Velocity = Capsule->GetPhysicsLinearVelocity() - PlatformVelocity;
	const float Speed = Velocity.Size2D();
	StepPhase += Dt * FMath::Lerp(6.f, 11.f, FMath::Clamp(Speed / 600.f, 0.f, 1.f));
	const float Bob = FMath::Sin(StepPhase) * BackStepBob * FMath::Clamp(Speed / 600.f, 0.f, 1.f);

	// Like cloth: it lags behind the turn and the push, overshoots a little and settles.
	const FVector2D Target(
		FMath::Clamp(-TurnRate * BackSwingPerTurn, -BackSwingMax, BackSwingMax),
		FMath::Clamp(LeanAccelFiltered.X * BackSwingPerAccel + Bob, -BackSwingMax, BackSwingMax));
	BackSwingVelocity += ((Target - BackSwing) * BackSwingStiffness - BackSwingVelocity * BackSwingDamping) * Dt;
	BackSwing += BackSwingVelocity * Dt;
	BackSwing.X = FMath::Clamp(BackSwing.X, -BackSwingMax * 1.5f, BackSwingMax * 1.5f);
	BackSwing.Y = FMath::Clamp(BackSwing.Y, -BackSwingMax * 1.5f, BackSwingMax * 1.5f);

	// Pitch turns the strap in the plane of the back, yaw lifts it away from the back.
	BackSling->SetRelativeRotation(FRotator(BackSwing.X, BackSwing.Y, 0.f));
}

float AFishPhysicsCharacter::GetHitMarkerAlpha() const
{
	if (HitMarkerDuration <= 0.f)
	{
		return 0.f;
	}
	const float Remaining = HitMarkerUntil - GetWorld()->GetTimeSeconds();
	return FMath::Clamp(Remaining / HitMarkerDuration, 0.f, 1.f);
}

float AFishPhysicsCharacter::GetAimPitch() const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	return PC ? FMath::FindDeltaAngleDegrees(0.f, PC->GetControlRotation().Pitch) : 0.f;
}

float AFishPhysicsCharacter::GetAimYaw() const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	return PC ? FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, PC->GetControlRotation().Yaw) : 0.f;
}
