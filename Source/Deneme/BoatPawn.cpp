// Copyright Epic Games, Inc. All Rights Reserved.

#include "BoatPawn.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "FishPhysicsCharacter.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Sine in cycles, like the material's Sine node with period 1. */
	float SineCycles(float Cycles)
	{
		return FMath::Sin(Cycles * UE_TWO_PI);
	}
}

ABoatPawn::ABoatPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	// Sized to BoatGovde's real outer hull (length ~469, width ~188, height ~170; actor origin sits at the
	// mesh's own waterline pivot, about 2/5 up from the keel).
	HullBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HullBox"));
	HullBox->SetBoxExtent(FVector(235.0, 95.0, 90.0));
	HullBox->SetCollisionProfileName(TEXT("Pawn"));
	// Characters on the deck must never stop the boat.
	HullBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	RootComponent = HullBox;

	VisualPivot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualPivot"));
	VisualPivot->SetupAttachment(HullBox);

	// Deck: only visible to traces (ground probe, foot IK), no physical contact. Fitted to the real hull's
	// interior floor (measured by tracing down into BoatGovde: a flat bottom around Z=-15, usable width about
	// +/-28 and usable length about +/-150 before the hull curves up into the bow/stern points).
	DeckCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("DeckCollision"));
	DeckCollision->SetupAttachment(VisualPivot);
	DeckCollision->SetBoxExtent(FVector(150.0, 28.0, 4.0));
	DeckCollision->SetRelativeLocation(FVector(0.0, 0.0, -15.0));
	DeckCollision->SetCollisionObjectType(ECC_WorldDynamic);
	DeckCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	DeckCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	DeckCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DeckCollision->SetHiddenInGame(true);

	// Railings: block pawns only, so ground traces and the camera ignore them. Snug against the deck's edges.
	const auto MakeRail = [this](const TCHAR* Name, const FVector& Extent, const FVector& Location)
	{
		UBoxComponent* Rail = CreateDefaultSubobject<UBoxComponent>(Name);
		Rail->SetupAttachment(VisualPivot);
		Rail->SetBoxExtent(Extent);
		Rail->SetRelativeLocation(Location);
		Rail->SetCollisionObjectType(ECC_WorldDynamic);
		Rail->SetCollisionResponseToAllChannels(ECR_Ignore);
		Rail->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Rail->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Rail->SetHiddenInGame(true);
		Rails.Add(Rail);
	};
	MakeRail(TEXT("RailLeft"), FVector(153.0, 3.0, 40.0), FVector(0.0, -29.0, 29.0));
	MakeRail(TEXT("RailRight"), FVector(153.0, 3.0, 40.0), FVector(0.0, 29.0, 29.0));
	MakeRail(TEXT("RailBow"), FVector(3.0, 30.0, 40.0), FVector(153.0, 0.0, 29.0));
	MakeRail(TEXT("RailStern"), FVector(3.0, 30.0, 40.0), FVector(-153.0, 0.0, 29.0));

	HelmPoint = CreateDefaultSubobject<USceneComponent>(TEXT("HelmPoint"));
	HelmPoint->SetupAttachment(VisualPivot);
	HelmPoint->SetRelativeLocation(FVector(-110.0, 0.0, -11.0));

	DriverSeat = CreateDefaultSubobject<USceneComponent>(TEXT("DriverSeat"));
	DriverSeat->SetupAttachment(VisualPivot);
	DriverSeat->SetRelativeLocation(FVector(-110.0, 0.0, -11.0));

	ExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ExitPoint"));
	ExitPoint->SetupAttachment(VisualPivot);
	ExitPoint->SetRelativeLocation(FVector(-30.0, 22.0, -11.0));

	// BoatGovde (hull) and BoatEngine (motor) were exported from the same scene and already slot together
	// when placed at the same origin with no extra offset. Both meshes face -X (bow) in their own local space;
	// Yaw 180 turns that to the actor's forward (+X), so GetActorForwardVector() still points at the bow.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HullAsset(TEXT("/Game/Models/Boat/BoatGovde.BoatGovde"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> EngineAsset(TEXT("/Game/Models/Boat/BoatEngine.BoatEngine"));

	HullMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HullMesh"));
	HullMesh->SetupAttachment(VisualPivot);
	HullMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HullMesh->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));

	MotorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MotorMesh"));
	MotorMesh->SetupAttachment(VisualPivot);
	MotorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MotorMesh->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));

	if (HullAsset.Succeeded())
	{
		HullMesh->SetStaticMesh(HullAsset.Object);
	}
	if (EngineAsset.Succeeded())
	{
		MotorMesh->SetStaticMesh(EngineAsset.Object);
	}

	// The harpoon barrel (Tanker mesh), standing on the deck at the stern side.
	BarrelHome = CreateDefaultSubobject<USceneComponent>(TEXT("BarrelHome"));
	BarrelHome->SetupAttachment(VisualPivot);
	BarrelHome->SetRelativeLocation(FVector(-125.0, -22.0, -11.0));

	BarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrelMesh"));
	BarrelMesh->SetupAttachment(BarrelHome);
	BarrelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BarrelMesh->SetRelativeLocation(BarrelMeshLocation);
	BarrelMesh->SetRelativeScale3D(BarrelMeshScale);

	BarrelRopePoint = CreateDefaultSubobject<USceneComponent>(TEXT("BarrelRopePoint"));
	BarrelRopePoint->SetupAttachment(BarrelMesh);
	BarrelRopePoint->SetRelativeLocation(FVector(0.0, 0.0, 50.0));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BarrelShape(TEXT("/Game/Models/Varil/Tanker.Tanker"));
	if (BarrelShape.Succeeded())
	{
		BarrelMesh->SetStaticMesh(BarrelShape.Object);
	}

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(HullBox);
	CameraArm->TargetArmLength = 900.f;
	CameraArm->SocketOffset = FVector(0.0, 0.0, 120.0);
	CameraArm->bUsePawnControlRotation = true;
	CameraArm->bEnableCameraLag = true;
	CameraArm->CameraLagSpeed = 6.f;
	CameraArm->bDoCollisionTest = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);

	// Three streaks of different thickness: a thin one on the left, a thick one in the middle, a medium one on the right.
	WakeLines = {
		FWakeLine{-64.f, 26.f, 0.85f},
		FWakeLine{0.f, 84.f, 1.f},
		FWakeLine{64.f, 44.f, 0.9f},
	};

	static ConstructorHelpers::FObjectFinder<UTexture2D> WhiteSquare(TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
	if (WhiteSquare.Succeeded())
	{
		WhiteTexture = WhiteSquare.Object;
	}
}

void ABoatPawn::BeginPlay()
{
	Super::BeginPlay();

	// The water plane sits inside the hull box, so it must never block the sweeps.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == this)
		{
			continue;
		}
		const UStaticMeshComponent* Mesh = Actor->FindComponentByClass<UStaticMeshComponent>();
		const bool bIsWater = Actor->ActorHasTag(TEXT("Water"))
			|| (Mesh && Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetName().Contains(TEXT("DenizMesh")));
		if (bIsWater)
		{
			WaterActors.AddUnique(Actor);
		}
	}
	for (AActor* Water : WaterActors)
	{
		if (Water)
		{
			HullBox->IgnoreActorWhenMoving(Water, true);
		}
	}

	FVector Location = GetActorLocation();
	Location.Z = WaterLevelZ;
	SetActorLocation(Location);
	PrevPivotTransform = CurrPivotTransform = VisualPivot->GetComponentTransform();

	// Re-apply the barrel transform: properties can be tuned without touching the constructor.
	BarrelMesh->SetRelativeLocation(BarrelMeshLocation);
	BarrelMesh->SetRelativeScale3D(BarrelMeshScale);

	BuildWakeLines();

	if (HitEffectMaterial)
	{
		HitEffectMID = UMaterialInstanceDynamic::Create(HitEffectMaterial, this);
		Camera->PostProcessSettings.AddBlendable(HitEffectMID, 1.f);
		Camera->PostProcessBlendWeight = 0.f;
	}

	// A fresh, empty wake map, and tell the water which world area it covers.
	// The map starts under the boat and follows it from then on (see RecenterWakeMap).
	if (WakeRenderTarget)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(this, WakeRenderTarget, FLinearColor::Black);
		WakeScratch = UKismetRenderingLibrary::CreateRenderTarget2D(this, WakeRenderTarget->SizeX, WakeRenderTarget->SizeY,
			WakeRenderTarget->RenderTargetFormat, FLinearColor::Black);
		WakeMapCenter = FVector2D(GetActorLocation().X, GetActorLocation().Y);
	}
	PushWakeMapToWater();

	MotorBaseRotation = MotorMesh->GetRelativeRotation();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetControlRotation(FRotator(CameraPitch, GetActorRotation().Yaw, 0.f));
	}
}

void ABoatPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxisKey(EKeys::MouseX, this, &ABoatPawn::LookYaw);
	PlayerInputComponent->BindAxisKey(EKeys::MouseY, this, &ABoatPawn::LookPitch);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_RightX, this, &ABoatPawn::LookYaw);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_RightY, this, &ABoatPawn::LookPitch);
}

void ABoatPawn::LookYaw(float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		AddControllerYawInput(Value * LookSensitivity);
		TimeSinceLook = 0.f;
	}
}

void ABoatPawn::LookPitch(float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		AddControllerPitchInput(Value * LookSensitivity * (bInvertLookPitch ? -1.f : 1.f));
		TimeSinceLook = 0.f;
	}
}

void ABoatPawn::ReadInput(float& OutThrottle, float& OutSteer) const
{
	OutThrottle = 0.f;
	OutSteer = 0.f;

	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	const auto Down = [PC](const FKey& Key) { return PC->IsInputKeyDown(Key) ? 1.f : 0.f; };

	OutThrottle = Down(EKeys::W) + Down(EKeys::Up) - Down(EKeys::S) - Down(EKeys::Down);
	OutSteer = Down(EKeys::D) + Down(EKeys::Right) - Down(EKeys::A) - Down(EKeys::Left);

	OutThrottle += PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
	OutSteer += PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);

	OutThrottle = FMath::Clamp(OutThrottle, -1.f, 1.f);
	OutSteer = FMath::Clamp(OutSteer, -1.f, 1.f);
}

float ABoatPawn::SampleWave(const FVector& WorldLocation, float Time) const
{
	return GetWaveHeightAt(FVector2D(WorldLocation.X, WorldLocation.Y));
}

float ABoatPawn::GetWaveHeightAt(const FVector2D& WorldXY) const
{
	const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	const auto Wave = [&](const FVector2D& Dir, float FreqMul, float SpeedMul)
	{
		return SineCycles(FVector2D::DotProduct(WorldXY, Dir) * WaveFrequency * FreqMul + Time * WaveSpeed * SpeedMul);
	};

	const float Swell = Wave(FVector2D(0.8, 0.6), 1.f, 0.18f)
		+ 0.6f * Wave(FVector2D(-0.5, 0.85), 1.7f, 0.27f)
		+ 0.25f * Wave(FVector2D(0.3, -0.95), 3.1f, 0.45f);
	return Swell * WaveHeight;
}

float ABoatPawn::GetForwardSpeed() const
{
	return FVector::DotProduct(Velocity, GetActorForwardVector());
}

void ABoatPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Dt = FMath::Min(DeltaSeconds, 0.05f);
	if (Dt <= UE_SMALL_NUMBER)
	{
		return;
	}

	PrevPivotTransform = VisualPivot->GetComponentTransform();

	float ThrottleInput = 0.f;
	float SteerInput = 0.f;
	ReadInput(ThrottleInput, SteerInput);

	// The motor and the tiller take time to respond: this is what makes the boat feel heavy.
	ThrottleSmoothed = FMath::FInterpTo(ThrottleSmoothed, ThrottleInput, Dt, ThrottleResponse);
	SteerSmoothed = FMath::FInterpTo(SteerSmoothed, SteerInput, Dt, SteerResponse);

	const FVector Forward = GetActorForwardVector();
	const FVector Right = GetActorRightVector();

	float ForwardSpeed = FVector::DotProduct(Velocity, Forward);
	float SideSpeed = FVector::DotProduct(Velocity, Right);
	const float ForwardSpeedBefore = ForwardSpeed;

	// Thrust along the hull; terminal speed lands on Max*Speed because thrust = MaxSpeed * Drag.
	const float TopSpeed = ThrottleSmoothed >= 0.f ? MaxForwardSpeed : MaxReverseSpeed;
	ForwardSpeed += ThrottleSmoothed * TopSpeed * ForwardDrag * Dt;

	const float TurnDrag = 1.f + TurnDragFactor * FMath::Abs(YawRate) / FMath::Max(MaxYawRate, 1.f);
	ForwardSpeed *= FMath::Exp(-ForwardDrag * TurnDrag * Dt);
	SideSpeed *= FMath::Exp(-LateralDrag * Dt);

	// World-space momentum is kept while the hull rotates, so the stern slides out until the keel catches up.
	Velocity = Forward * ForwardSpeed + Right * SideSpeed;

	// Rudder: needs water flowing past it (speed) or a prop wash (throttle), and flips when going astern.
	const float SpeedAuthority = FMath::Clamp(FMath::Abs(ForwardSpeed) / SteerFullSpeed, 0.f, 1.f);
	const float PropAuthority = FMath::Abs(ThrottleSmoothed) * PropWashSteering;
	const float Authority = FMath::Max(SpeedAuthority, PropAuthority);
	const float Direction = FMath::Abs(ForwardSpeed) > 30.f ? FMath::Sign(ForwardSpeed) : FMath::Sign(ThrottleSmoothed);

	const float YawRateTarget = SteerSmoothed * MaxYawRate * Authority * Direction;
	YawRate = FMath::FInterpTo(YawRate, YawRateTarget, Dt, YawResponse);
	const float DeltaYaw = YawRate * Dt;

	// Rotate around a pivot ahead of the centre: the bow turns, the stern swings out.
	const FVector Location = GetActorLocation();
	const FVector Pivot = Location + Forward * PivotForwardOffset;
	const FVector RotatedLocation = Pivot + FRotator(0.f, DeltaYaw, 0.f).RotateVector(Location - Pivot);

	FVector Delta = Velocity * Dt + (RotatedLocation - Location);
	Delta.Z = 0.f;

	FHitResult Hit;
	AddActorWorldOffset(Delta, true, &Hit);
	if (Hit.IsValidBlockingHit())
	{
		const float IntoSurface = FVector::DotProduct(Velocity, Hit.ImpactNormal);
		if (IntoSurface < 0.f)
		{
			Velocity -= Hit.ImpactNormal * IntoSurface;
		}
		const FVector Remaining = FVector::VectorPlaneProject(Delta * (1.f - Hit.Time), Hit.ImpactNormal);
		AddActorWorldOffset(FVector(Remaining.X, Remaining.Y, 0.f), true);
	}
	AddActorWorldRotation(FRotator(0.f, DeltaYaw, 0.f));

	// ---------------------------------------------------------------- body motion
	// Pitch follows longitudinal acceleration: bow lifts on take-off, then settles as the speed saturates.
	const float Accel = (ForwardSpeed - ForwardSpeedBefore) / Dt;
	AccelFiltered = FMath::FInterpTo(AccelFiltered, Accel, Dt, 3.f);

	const float PitchTarget = FMath::Clamp(AccelFiltered * PitchPerAcceleration, -MaxPitch, MaxPitch);
	PitchVelocity += (PitchStiffness * (PitchTarget - Pitch) - PitchDamping * PitchVelocity) * Dt;
	Pitch += PitchVelocity * Dt;

	// Roll follows centripetal acceleration (turn rate * speed).
	const float LateralAccel = FMath::DegreesToRadians(YawRate) * ForwardSpeed;
	LateralAccelFiltered = FMath::FInterpTo(LateralAccelFiltered, LateralAccel, Dt, 4.f);

	const float RollTarget = FMath::Clamp(-LateralAccelFiltered * RollPerLateralAcceleration, -MaxRoll, MaxRoll);
	RollVelocity += (RollStiffness * (RollTarget - Roll) - RollDamping * RollVelocity) * Dt;
	Roll += RollVelocity * Dt;

	// ---------------------------------------------------------------- waves
	float WaveZ = 0.f;
	float WavePitch = 0.f;
	float WaveRoll = 0.f;
	if (bRideWaves)
	{
		const FVector Center = GetActorLocation();
		const FVector Fwd = GetActorForwardVector();
		const FVector Rgt = GetActorRightVector();
		const float HL = WaveSampleHalfExtent.X;
		const float HW = WaveSampleHalfExtent.Y;

		const float Time = GetWorld()->GetTimeSeconds();
		const float HFront = SampleWave(Center + Fwd * HL, Time);
		const float HBack = SampleWave(Center - Fwd * HL, Time);
		const float HRight = SampleWave(Center + Rgt * HW, Time);
		const float HLeft = SampleWave(Center - Rgt * HW, Time);

		WaveZ = SampleWave(Center, Time);
		WavePitch = FMath::RadiansToDegrees(FMath::Atan2(HFront - HBack, 2.f * HL)) * WaveTiltScale;
		WaveRoll = FMath::RadiansToDegrees(FMath::Atan2(HLeft - HRight, 2.f * HW)) * WaveTiltScale;
	}

	// A fast hull skims across the crests instead of climbing every wave: fade the wave motion out with
	// speed and low-pass what is left.
	const float SpeedAlpha = FMath::SmoothStep(0.f, 1.f, FMath::Abs(ForwardSpeed) / WaveDampSpeed);
	const float WaveInfluence = FMath::Lerp(1.f, HighSpeedWaveInfluence, SpeedAlpha);
	WaveZSmoothed = FMath::FInterpTo(WaveZSmoothed, WaveZ * WaveInfluence, Dt, WaveResponse);
	WavePitchSmoothed = FMath::FInterpTo(WavePitchSmoothed, WavePitch * WaveInfluence, Dt, WaveResponse);
	WaveRollSmoothed = FMath::FInterpTo(WaveRollSmoothed, WaveRoll * WaveInfluence, Dt, WaveResponse);

	// The turning/acceleration lean and the wave tilt are capped together, not separately, so a hard turn on
	// top of a wave slope cannot tip the hull further than intended.
	const float TotalPitch = FMath::Clamp(Pitch + WavePitchSmoothed, -MaxCombinedTilt, MaxCombinedTilt);
	const float TotalRoll = FMath::Clamp(Roll + WaveRollSmoothed, -MaxCombinedTilt, MaxCombinedTilt);

	// Hard floor: the smoothed bob (WaveZSmoothed) lags the real sea by design, and tilting dips the hull's
	// edges below its own centre. On a sharp wave the two together can sink the hull below the water that is
	// actually there right now, so the sea shows through the open top as if the boat were flooding. This clamps
	// the pivot so that can never happen: it only engages in that edge case and never pokes the hull up out of
	// the water, so normal bobbing looks exactly as before.
	const float TiltDip = WaveSampleHalfExtent.Y * FMath::Sin(FMath::DegreesToRadians(FMath::Abs(TotalRoll)))
		+ WaveSampleHalfExtent.X * FMath::Sin(FMath::DegreesToRadians(FMath::Abs(TotalPitch)));
	const float MinPivotZ = WaveZ + TiltDip + MinHullClearance;
	const float FinalPivotZ = FMath::Max(WaveZSmoothed + HullFreeboard, MinPivotZ);

	VisualPivot->SetRelativeLocation(FVector(0.f, 0.f, FinalPivotZ));
	VisualPivot->SetRelativeRotation(FRotator(TotalPitch, 0.f, TotalRoll));

	// The outboard swings against the steering: right turn = motor turned to the left, like a tiller motor.
	MotorYaw = FMath::FInterpTo(MotorYaw, -SteerSmoothed * MotorTurnAngle, Dt, MotorTurnResponse);
	MotorMesh->SetRelativeRotation(MotorBaseRotation + FRotator(0.f, MotorYaw, 0.f));

	CurrPivotTransform = VisualPivot->GetComponentTransform();
	LastDt = Dt;

	UpdateWake(Dt);

	// Leave the helm (the key press that got us in is ignored for a moment).
	if (DriverCharacter)
	{
		const APlayerController* DriverController = Cast<APlayerController>(GetController());
		const bool bExitPressed = DriverController
			&& (DriverController->WasInputKeyJustPressed(EKeys::E) || DriverController->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Left));
		if (bExitPressed && GetWorld()->GetTimeSeconds() - DrivingStartTime > 0.35f)
		{
			EndDriving();
		}
	}

	// Hit shake on the boat camera.
	{
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

	// ---------------------------------------------------------------- camera
	TimeSinceLook += Dt;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (TimeSinceLook > RecenterDelay)
		{
			const FRotator Target(CameraPitch, GetActorRotation().Yaw, 0.f);
			PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), Target, Dt, RecenterSpeed));
		}
	}

	if (bShowDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			reinterpret_cast<uint64>(this), 0.f, FColor::Cyan,
			FString::Printf(TEXT("Speed %.0f  Side %.0f  Throttle %.2f  Yaw %.1f  Pitch %.1f  Roll %.1f"),
				ForwardSpeed, SideSpeed, ThrottleSmoothed, YawRate, Pitch, Roll));
	}
}

FVector ABoatPawn::GetInteractionLocation() const
{
	return HelmPoint->GetComponentLocation();
}

void ABoatPawn::Interact(APawn* Interactor)
{
	if (AFishPhysicsCharacter* Character = Cast<AFishPhysicsCharacter>(Interactor))
	{
		BeginDriving(Character);
	}
}

FText ABoatPawn::GetPromptText() const
{
	return IsDriven() ? ExitText : FText::GetEmpty();
}

FVector ABoatPawn::GetPlatformVelocityAtLocation(const FVector& WorldLocation) const
{
	if (LastDt <= UE_KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	// Where the deck point under WorldLocation was one tick ago.
	const FVector Local = CurrPivotTransform.InverseTransformPosition(WorldLocation);
	const FVector Before = PrevPivotTransform.TransformPosition(Local);
	return (WorldLocation - Before) / LastDt;
}

void ABoatPawn::BeginDriving(AFishPhysicsCharacter* Character)
{
	if (!Character || DriverCharacter)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC)
	{
		return;
	}

	// Remember exactly what the player sees right now, before anything is moved or possessed.
	const APlayerCameraManager* CameraManager = PC->PlayerCameraManager;
	const FVector FromLocation = CameraManager->GetCameraLocation();
	const FRotator FromRotation = CameraManager->GetCameraRotation();
	const float FromFOV = CameraManager->GetFOVAngle();

	DriverCharacter = Character;
	DrivingStartTime = GetWorld()->GetTimeSeconds();

	Character->EnterVehicle(DriverSeat);

	PC->bAutoManageActiveCameraTarget = false;
	PC->Possess(this);
	PC->SetControlRotation(FRotator(CameraPitch, GetActorRotation().Yaw, 0.f));
	GlideViewTo(PC, this, CameraBlendTime, FromLocation, FromRotation, FromFOV);
	TimeSinceLook = 100.f;
}

void ABoatPawn::EndDriving()
{
	AFishPhysicsCharacter* Character = DriverCharacter;
	if (!Character)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	const FTransform Exit = ExitPoint->GetComponentTransform();

	// What the player sees right now (the boat camera), before the character takes over.
	FVector FromLocation = FVector::ZeroVector;
	FRotator FromRotation = FRotator::ZeroRotator;
	float FromFOV = 90.f;
	if (PC && PC->PlayerCameraManager)
	{
		FromLocation = PC->PlayerCameraManager->GetCameraLocation();
		FromRotation = PC->PlayerCameraManager->GetCameraRotation();
		FromFOV = PC->PlayerCameraManager->GetFOVAngle();
	}

	Character->LeaveVehicle(Exit, GetPlatformVelocityAtLocation(Exit.GetLocation()), this);
	DriverCharacter = nullptr;

	if (PC)
	{
		PC->bAutoManageActiveCameraTarget = false;
		PC->Possess(Character);
		GlideViewTo(PC, Character, CameraBlendTime, FromLocation, FromRotation, FromFOV);
	}
}

void ABoatPawn::ApplyImpulse(const FVector& DeltaVelocity, float RollKick, float PitchKick)
{
	// Everybody on the boat feels it: the boat camera and the camera of every character.
	const float ShakeStrength = FMath::Clamp(DeltaVelocity.Size2D() / 300.f, 0.3f, 1.6f) * HitShakeScale;
	CameraShake.Add(ShakeStrength, HitShakeDuration);
	for (TActorIterator<AFishPhysicsCharacter> It(GetWorld()); It; ++It)
	{
		It->AddCameraShake(ShakeStrength * 0.9f, HitShakeDuration);
	}

	Velocity += FVector(DeltaVelocity.X, DeltaVelocity.Y, 0.f);
	RollVelocity += RollKick;
	PitchVelocity += PitchKick;
}

void ABoatPawn::GlideViewTo(APlayerController* PC, AActor* NewTarget, float BlendTime, const FVector& FromLocation, const FRotator& FromRotation, float FromFOV)
{
	if (!PC || !NewTarget)
	{
		return;
	}

	if (BlendTime <= KINDA_SMALL_NUMBER)
	{
		PC->SetViewTarget(NewTarget);
		return;
	}

	// The camera of the pawn we leave is unreliable (it loses its controller, gets attached or moved), so the blend
	// starts from a still camera that sits exactly where the player's view was a moment ago.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* Start = PC->GetWorld()->SpawnActor<ACameraActor>(FromLocation, FromRotation, Params);
	if (!Start)
	{
		PC->SetViewTargetWithBlend(NewTarget, BlendTime, VTBlend_EaseInOut, 2.f);
		return;
	}

	UCameraComponent* StartCamera = Start->GetCameraComponent();
	StartCamera->SetFieldOfView(FromFOV);
	StartCamera->bConstrainAspectRatio = false;

	PC->SetViewTarget(Start);
	PC->SetViewTargetWithBlend(NewTarget, BlendTime, VTBlend_EaseInOut, 2.f);
	Start->SetLifeSpan(BlendTime + 0.5f);
}

FVector2D ABoatPawn::WakeToPixel(const FVector& WorldLocation, const FVector2D& CanvasSize) const
{
	// Same mapping the water material uses: world XY -> 0..1 over the wake map area.
	const float U = (WorldLocation.X - WakeMapCenter.X) / WakeMapSize + 0.5f;
	const float V = (WorldLocation.Y - WakeMapCenter.Y) / WakeMapSize + 0.5f;
	return FVector2D(U * CanvasSize.X, V * CanvasSize.Y);
}

void ABoatPawn::UpdateWake(float Dt)
{
	if (!WakeRenderTarget || !WhiteTexture)
	{
		QueuedWake.Reset();
		return;
	}

	// Keep the map under the boat. Foam painted near the edge of a fixed map used to be stretched by the clamped
	// texture lookup into straight lines across the whole sea.
	const FVector2D BoatXY(GetActorLocation().X, GetActorLocation().Y);
	if ((BoatXY - WakeMapCenter).GetAbsMax() > WakeMapSize * WakeRecenterFraction)
	{
		RecenterWakeMap(BoatXY);
	}

	const float Speed = Velocity.Size2D();
	const float Strength = FMath::Clamp(Speed / WakeFullSpeed, 0.f, 1.f);
	const FVector Stern = GetActorLocation() - GetActorForwardVector() * WakeSternOffset;
	const FVector Right = GetActorRightVector();

	UCanvas* Canvas = nullptr;
	FVector2D Size = FVector2D::ZeroVector;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, WakeRenderTarget, Canvas, Size, Context);
	if (!Canvas)
	{
		return;
	}

	// Everything already on the map gets a little dimmer, so old foam melts away.
	const float FadeAlpha = 1.f - FMath::Exp(-Dt / FMath::Max(WakeFadeSeconds / 3.f, 0.05f));
	Canvas->K2_DrawTexture(WhiteTexture, FVector2D::ZeroVector, Size, FVector2D::ZeroVector, FVector2D::UnitVector,
		FLinearColor(0.f, 0.f, 0.f, FadeAlpha), BLEND_Translucent, 0.f, FVector2D(0.5f, 0.5f));

	// New foam: one segment per streak from where the stern was last frame to where it is now.
	// Every streak swells, shrinks and drifts sideways with the distance travelled, so none of them is a ruler line.
	if (bHavePrevStern)
	{
		WakeTravel += FVector::Dist2D(Stern, PrevSternLocation);
	}
	if (bHavePrevStern && Speed > WakeMinSpeed)
	{
		const float PixelsPerCm = Size.X / WakeMapSize;
		for (int32 Index = 0; Index < ActiveWakeLines.Num(); ++Index)
		{
			const FWakeLine& Line = ActiveWakeLines[Index];
			const float Seed = Index * 13.7f;

			const auto OffsetAt = [&](float Travel)
			{
				return Line.SideOffset + FMath::PerlinNoise1D(Travel * 0.0021f + Seed + 100.f) * WakeSideWobble;
			};

			const float Noise = FMath::PerlinNoise1D(WakeTravel * 0.0035f + Seed);
			const float Width = FMath::Max(Line.Width * 0.3f, Line.Width * (1.f + Noise * WakeWidthWobble));
			const float Bright = Line.Strength * (0.8f + 0.2f * FMath::PerlinNoise1D(WakeTravel * 0.004f + Seed + 50.f));

			const FVector From = PrevSternLocation + PrevRight * OffsetAt(PrevWakeTravel);
			const FVector To = Stern + Right * OffsetAt(WakeTravel);
			Canvas->K2_DrawLine(WakeToPixel(From, Size), WakeToPixel(To, Size), FMath::Max(1.f, Width * PixelsPerCm),
				FLinearColor(1.f, 1.f, 1.f, Strength * Bright));
		}
	}

	// Foam other things left this frame (the harpoon barrel).
	const float PixelsPerCm = Size.X / WakeMapSize;
	for (const FQueuedWake& Segment : QueuedWake)
	{
		Canvas->K2_DrawLine(WakeToPixel(Segment.From, Size), WakeToPixel(Segment.To, Size),
			FMath::Max(1.f, Segment.Width * PixelsPerCm), FLinearColor(1.f, 1.f, 1.f, Segment.Alpha));
	}
	QueuedWake.Reset();

	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);

	PrevWakeTravel = WakeTravel;
	PrevSternLocation = Stern;
	PrevRight = Right;
	bHavePrevStern = true;
}

void ABoatPawn::PushWakeMapToWater()
{
	if (!WaterParameters)
	{
		return;
	}
	if (UMaterialParameterCollectionInstance* Water = GetWorld()->GetParameterCollectionInstance(WaterParameters))
	{
		Water->SetVectorParameterValue(TEXT("WakeMap"), FLinearColor(WakeMapCenter.X, WakeMapCenter.Y, WakeMapSize, 0.f));
	}
}

void ABoatPawn::RecenterWakeMap(const FVector2D& NewCenter)
{
	if (!WakeRenderTarget || !WakeScratch || WakeRenderTarget->SizeX <= 0)
	{
		return;
	}

	// Move by whole pixels so the old foam lands exactly on the pixel grid and does not blur.
	const float PixelSize = WakeMapSize / WakeRenderTarget->SizeX;
	const FVector2D ShiftPixels(FMath::RoundToFloat((NewCenter.X - WakeMapCenter.X) / PixelSize),
		FMath::RoundToFloat((NewCenter.Y - WakeMapCenter.Y) / PixelSize));
	if (ShiftPixels.IsNearlyZero())
	{
		return;
	}

	// Old map -> scratch, shifted the other way; the strip that comes into view stays empty (black).
	UKismetRenderingLibrary::ClearRenderTarget2D(this, WakeScratch, FLinearColor::Black);
	UCanvas* Canvas = nullptr;
	FVector2D Size = FVector2D::ZeroVector;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, WakeScratch, Canvas, Size, Context);
	if (Canvas)
	{
		Canvas->K2_DrawTexture(WakeRenderTarget, -ShiftPixels, Size, FVector2D::ZeroVector, FVector2D::UnitVector,
			FLinearColor::White, BLEND_Opaque);
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);

	// Scratch -> the map the water reads.
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, WakeRenderTarget, Canvas, Size, Context);
	if (Canvas)
	{
		Canvas->K2_DrawTexture(WakeScratch, FVector2D::ZeroVector, Size, FVector2D::ZeroVector, FVector2D::UnitVector,
			FLinearColor::White, BLEND_Opaque);
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);

	WakeMapCenter += ShiftPixels * PixelSize;
	PushWakeMapToWater();
}

void ABoatPawn::BuildWakeLines()
{
	ActiveWakeLines.Reset();
	if (!bRandomizeWakeLines || WakeLineCount <= 0)
	{
		ActiveWakeLines = WakeLines;
		return;
	}

	// Evenly spread slots with a random nudge, mostly thin streaks and a few thicker ones.
	FRandomStream Random(WakeSeed != 0 ? WakeSeed : FMath::Rand());
	const float Slot = 2.f * WakeSpreadHalfWidth / WakeLineCount;
	for (int32 Index = 0; Index < WakeLineCount; ++Index)
	{
		FWakeLine Line;
		Line.SideOffset = -WakeSpreadHalfWidth + Slot * (Index + 0.5f) + Random.FRandRange(-0.45f, 0.45f) * Slot;
		Line.Width = FMath::Lerp(WakeMinWidth, FMath::Max(WakeMinWidth, WakeMaxWidth), FMath::Pow(Random.FRand(), 1.8f));
		Line.Strength = Random.FRandRange(0.45f, 0.95f);
		ActiveWakeLines.Add(Line);
	}
}

void ABoatPawn::ApplyHullDamage(float Damage)
{
	if (Damage <= 0.f || HullHealth <= 0.f)
	{
		return;
	}

	HullHealth = FMath::Max(0.f, HullHealth - Damage);

	// A hit rattles everybody on board: the dizzy screen for the boat camera and for every character.
	const float EffectStrength = FMath::Clamp(Damage / 20.f, 0.6f, 1.5f) * HitEffectScale;
	HitEffect.Add(EffectStrength, HitEffectDuration);
	for (TActorIterator<AFishPhysicsCharacter> It(GetWorld()); It; ++It)
	{
		It->AddHitEffect(EffectStrength);
	}

	OnHullDamaged.Broadcast(GetHullHealth01(), Damage);
	if (HullHealth <= 0.f)
	{
		OnHullDestroyed.Broadcast();
	}
}

void ABoatPawn::RepairHull(float Amount)
{
	HullHealth = FMath::Clamp(HullHealth + Amount, 0.f, MaxHullHealth);
}
