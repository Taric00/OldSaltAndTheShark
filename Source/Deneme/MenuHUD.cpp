// Copyright Epic Games, Inc. All Rights Reserved.

#include "MenuHUD.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameViewportClient.h"
#include "MenuWidget.h"

void AMenuHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetOwningPlayerController();

	if (PC && MenuWidgetClass)
	{
		MenuWidget = CreateWidget<UMenuWidget>(PC, MenuWidgetClass);
		if (MenuWidget)
		{
			MenuWidget->AddToViewport(0);
		}
	}

	if (PC)
	{
		PC->bShowMouseCursor = true;

		// The project's default viewport capture mode grabs and hides the mouse on the first click (and eats
		// that click instead of handing it to the UI) - fine for the FPS-style gameplay levels, wrong for a
		// menu with buttons to press. Switch it off for as long as this HUD (and its menu) is up.
		if (UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			ViewportClient->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
			ViewportClient->SetMouseLockMode(EMouseLockMode::DoNotLock);
		}

		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		if (MenuWidget)
		{
			InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
		}
		PC->SetInputMode(InputMode);
	}
}
