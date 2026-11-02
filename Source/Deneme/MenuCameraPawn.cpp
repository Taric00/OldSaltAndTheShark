// Copyright Epic Games, Inc. All Rights Reserved.

#include "MenuCameraPawn.h"

#include "Camera/CameraComponent.h"

AMenuCameraPawn::AMenuCameraPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	RootComponent = Camera;

	AutoPossessPlayer = EAutoReceiveInput::Player0;
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
}
