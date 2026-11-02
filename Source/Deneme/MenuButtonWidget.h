// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MenuButtonWidget.generated.h"

class UButton;
class UTextBlock;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMenuButtonClicked);

/**
 * One main-menu button: label, hover glow/scale and a staggered slide-fade entrance, all driven from here so
 * the Widget Blueprint only needs to lay the pieces out. Make a Widget Blueprint with this as its parent; it
 * optionally binds:
 *   Button      - the clickable UButton
 *   Label       - a Text Block that receives ButtonLabel
 *   Accent      - any widget (e.g. a thin Border/Image) used as an underline: it scales in from zero on hover
 */
UCLASS(Abstract)
class DENEME_API UMenuButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Fires when the button is clicked. */
	UPROPERTY(BlueprintAssignable, Category = "Menu Button")
	FMenuButtonClicked OnMenuButtonClicked;

	/** Text shown on the button. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Button")
	FText ButtonLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Button")
	FLinearColor NormalColor = FLinearColor(0.92f, 0.93f, 0.9f, 1.f);

	/** Accent colour on hover (amber, REPO-style). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Button")
	FLinearColor HoverColor = FLinearColor(1.f, 0.72f, 0.18f, 1.f);

	/** How much bigger the button gets on hover (1 = no change). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Button", meta = (ClampMin = "1"))
	float HoverScale = 1.08f;

	/** How fast the hover grow/glow and the underline sweep follow (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Button", meta = (ClampMin = "0.5"))
	float HoverSpeed = 9.f;

	/** How far left of its resting spot the button starts (cm) before sliding in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Button|Entrance")
	float SlideInDistance = 70.f;

	/** Seconds after the widget is constructed before this button starts appearing: stagger the buttons by
	 * giving each one a bigger delay (0, 0.15, 0.3, ...) for a one-after-another reveal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Button|Entrance", meta = (ClampMin = "0"))
	float AppearDelay = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Button|Entrance", meta = (ClampMin = "0.05"))
	float AppearDuration = 0.5f;

	UFUNCTION(BlueprintPure, Category = "Menu Button")
	float GetHoverAlpha() const { return HoverAlpha; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Fires every frame the hover amount (0..1, eased) changes, for extra Blueprint-side animation. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu Button")
	void OnHoverAlphaChanged(float NewAlpha);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Label;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Accent;

private:
	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

	bool bHovered = false;
	float HoverAlpha = 0.f;
	float AppearElapsed = 0.f;
};
