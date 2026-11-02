// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "FishHUD.generated.h"

class UFishCrosshairWidget;
class UFishHullWidget;
class UFishPromptWidget;

/**
 * Puts the game's Widget Blueprints on screen. Make a Blueprint child of this class (BP_FishHUD), set the widget
 * classes below and use it as the HUD class of the game mode.
 */
UCLASS()
class DENEME_API AFishHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

	/** Widget Blueprint (parent UFishPromptWidget) that shows the interaction prompt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widgets")
	TSubclassOf<UFishPromptWidget> PromptWidgetClass;

	/** Widget Blueprint (parent UFishCrosshairWidget) shown while aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widgets")
	TSubclassOf<UFishCrosshairWidget> CrosshairWidgetClass;

	/** Widget Blueprint (parent UFishHullWidget) that shows the boat's hull damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widgets")
	TSubclassOf<UFishHullWidget> HullWidgetClass;

	UFUNCTION(BlueprintPure, Category = "Widgets")
	UFishPromptWidget* GetPromptWidget() const { return PromptWidget; }

	// Used only when no prompt widget class is set: a plain text fallback.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fallback", meta = (ClampMin = "0", ClampMax = "1"))
	float PromptHeight = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fallback", meta = (ClampMin = "0.1"))
	float PromptScale = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fallback")
	FLinearColor PromptColor = FLinearColor(0.92f, 0.96f, 0.95f, 1.f);

private:
	UPROPERTY(Transient)
	TObjectPtr<UFishPromptWidget> PromptWidget;

	UPROPERTY(Transient)
	TObjectPtr<UFishCrosshairWidget> CrosshairWidget;

	UPROPERTY(Transient)
	TObjectPtr<UFishHullWidget> HullWidget;
};
