// Copyright Epic Games, Inc. All Rights Reserved.

#include "MenuWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MenuButtonWidget.h"

void UMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (PlayButton)
	{
		PlayButton->OnMenuButtonClicked.AddDynamic(this, &UMenuWidget::HandlePlayClicked);
	}
	if (CreditsButton)
	{
		CreditsButton->OnMenuButtonClicked.AddDynamic(this, &UMenuWidget::HandleCreditsClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnMenuButtonClicked.AddDynamic(this, &UMenuWidget::HandleQuitClicked);
	}

	if (CreditsPanel)
	{
		CreditsPanel->SetVisibility(ESlateVisibility::Collapsed);
		CreditsPanel->SetRenderOpacity(0.f);
	}

	TitleElapsed = 0.f;
	if (TitleText)
	{
		TitleText->SetRenderOpacity(0.f);
		TitleText->SetRenderTranslation(FVector2D(-TitleSlideInDistance, 0.f));
	}
	if (SubtitleText)
	{
		SubtitleText->SetRenderOpacity(0.f);
		SubtitleText->SetRenderTranslation(FVector2D(-TitleSlideInDistance, 0.f));
	}
}

void UMenuWidget::FadeTitleWidget(UWidget* Widget, float Delay, float Dt)
{
	if (!Widget)
	{
		return;
	}
	const float T = TitleAppearDuration > 0.f ? (TitleElapsed - Delay) / TitleAppearDuration : 1.f;
	const float Alpha = FMath::Clamp(T, 0.f, 1.f);
	const float Eased = FMath::SmoothStep(0.f, 1.f, Alpha);

	Widget->SetRenderOpacity(Eased);
	Widget->SetRenderTranslation(FVector2D(-TitleSlideInDistance * (1.f - Eased), 0.f));
}

void UMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	TitleElapsed += InDeltaTime;
	FadeTitleWidget(TitleText, TitleAppearDelay, InDeltaTime);
	FadeTitleWidget(SubtitleText, TitleAppearDelay + 0.1f, InDeltaTime);

	if (CreditsPanel)
	{
		CreditsAlpha = FMath::FInterpTo(CreditsAlpha, bCreditsOpen ? 1.f : 0.f, InDeltaTime, CreditsFadeSpeed);
		CreditsPanel->SetRenderOpacity(CreditsAlpha);
		CreditsPanel->SetVisibility(CreditsAlpha > 0.01f ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UMenuWidget::HandlePlayClicked()
{
	UGameplayStatics::OpenLevel(this, GameLevelName);
}

void UMenuWidget::HandleCreditsClicked()
{
	bCreditsOpen = !bCreditsOpen;
}

void UMenuWidget::HandleQuitClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
}
