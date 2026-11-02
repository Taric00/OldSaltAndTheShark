// Copyright Epic Games, Inc. All Rights Reserved.

#include "MenuHUD.h"

#include "Blueprint/UserWidget.h"
#include "MenuWidget.h"

void AMenuHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetOwningPlayerController();
	if (PC)
	{
		PC->bShowMouseCursor = true;
		PC->SetInputMode(FInputModeUIOnly());
	}

	if (PC && MenuWidgetClass)
	{
		MenuWidget = CreateWidget<UMenuWidget>(PC, MenuWidgetClass);
		if (MenuWidget)
		{
			MenuWidget->AddToViewport(0);
		}
	}
}
