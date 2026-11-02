// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FishCrosshairWidget.generated.h"

/**
 * Base class of the crosshair widget. Make a Widget Blueprint with this parent and draw any crosshair you like:
 * the whole widget fades in with the aim and is hidden when the player is not aiming.
 * Use OnAimChanged for extra animation (spreading, pulsing ...).
 */
UCLASS(Abstract)
class DENEME_API UFishCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 0..1, how far the aim is raised. Also readable from the Widget Blueprint. */
	UFUNCTION(BlueprintPure, Category = "Crosshair")
	float GetAimAlpha() const { return AimAlpha; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Fires every frame the aim value changes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Crosshair")
	void OnAimChanged(float NewAimAlpha);

	/** The "+" rotated 45 deg (an "X"), shown briefly on a successful hit. Optional: fine if the Widget Blueprint
	 * has no widget named this. */
	UPROPERTY(BlueprintReadOnly, Category = "Crosshair", meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> HitMarkerPanel;

private:
	float AimAlpha = 0.f;
};
