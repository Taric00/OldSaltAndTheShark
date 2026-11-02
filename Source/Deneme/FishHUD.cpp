// Copyright Epic Games, Inc. All Rights Reserved.

#include "FishHUD.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "FishInteractionInterfaces.h"
#include "FishCrosshairWidget.h"
#include "FishHullWidget.h"
#include "FishPromptWidget.h"

void AFishHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetOwningPlayerController();
	if (PC && PromptWidgetClass)
	{
		PromptWidget = CreateWidget<UFishPromptWidget>(PC, PromptWidgetClass);
		if (PromptWidget)
		{
			PromptWidget->AddToViewport(10);
		}
	}
	if (PC && HullWidgetClass)
	{
		HullWidget = CreateWidget<UFishHullWidget>(PC, HullWidgetClass);
		if (HullWidget)
		{
			HullWidget->AddToViewport(4);
		}
	}
	if (PC && CrosshairWidgetClass)
	{
		CrosshairWidget = CreateWidget<UFishCrosshairWidget>(PC, CrosshairWidgetClass);
		if (CrosshairWidget)
		{
			CrosshairWidget->AddToViewport(5);
		}
	}
}

void AFishHUD::DrawHUD()
{
	Super::DrawHUD();

	// A Widget Blueprint draws the prompt; this is only the fallback.
	if (PromptWidget)
	{
		return;
	}

	const IFishPromptProvider* Provider = Cast<IFishPromptProvider>(GetOwningPawn());
	if (!Provider || !Canvas || !GEngine)
	{
		return;
	}

	const FString Text = Provider->GetPromptText().ToString();
	if (Text.IsEmpty())
	{
		return;
	}

	UFont* Font = GEngine->GetLargeFont();
	float Width = 0.f;
	float Height = 0.f;
	GetTextSize(Text, Width, Height, Font, PromptScale);

	const float X = (Canvas->SizeX - Width) * 0.5f;
	const float Y = Canvas->SizeY * PromptHeight;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), X - 18.f, Y - 8.f, Width + 36.f, Height + 16.f);
	DrawText(Text, PromptColor, X, Y, Font, PromptScale);
}
