// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MenuWidget.generated.h"

class UMenuButtonWidget;
class UTextBlock;
class UWidget;

/**
 * The main menu: title, Play / Credits / Quit buttons and a credits panel the Credits button toggles.
 * Make a Widget Blueprint with this as its parent. Everything below is optional - bind the pieces you add:
 *   PlayButton, CreditsButton, QuitButton - WBP_MenuButton instances (or any UMenuButtonWidget child)
 *   CreditsPanel  - shown while credits are open, hidden otherwise (any container)
 *   TitleText, SubtitleText - faded/slid in the same way as the buttons
 */
UCLASS(Abstract)
class DENEME_API UMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Level to open when Play is clicked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	FName GameLevelName = TEXT("Game");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Title Entrance", meta = (ClampMin = "0"))
	float TitleAppearDelay = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Title Entrance", meta = (ClampMin = "0.05"))
	float TitleAppearDuration = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Title Entrance")
	float TitleSlideInDistance = 50.f;

	/** How fast the credits panel fades in/out (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu", meta = (ClampMin = "0.5"))
	float CreditsFadeSpeed = 8.f;

	UFUNCTION(BlueprintPure, Category = "Menu")
	bool IsCreditsOpen() const { return bCreditsOpen; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMenuButtonWidget> PlayButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMenuButtonWidget> CreditsButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMenuButtonWidget> QuitButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CreditsPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SubtitleText;

private:
	UFUNCTION()
	void HandlePlayClicked();

	UFUNCTION()
	void HandleCreditsClicked();

	UFUNCTION()
	void HandleQuitClicked();

	void FadeTitleWidget(UWidget* Widget, float Delay, float Dt);

	bool bCreditsOpen = false;
	float CreditsAlpha = 0.f;
	float TitleElapsed = 0.f;
};
