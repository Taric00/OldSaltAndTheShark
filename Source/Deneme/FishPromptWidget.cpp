// Copyright Epic Games, Inc. All Rights Reserved.

#include "FishPromptWidget.h"

#include "Components/TextBlock.h"
#include "FishInteractionInterfaces.h"
#include "GameFramework/Pawn.h"

void UFishPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// The prompt always belongs to the pawn the player controls right now (character or boat).
	const IFishPromptProvider* Provider = Cast<IFishPromptProvider>(GetOwningPlayerPawn());
	const FText NewText = Provider ? Provider->GetPromptText() : FText::GetEmpty();
	const bool bWantsShown = !NewText.IsEmpty();

	if (bWantsShown && !NewText.EqualTo(CurrentText))
	{
		CurrentText = NewText;
		if (PromptText)
		{
			PromptText->SetText(CurrentText);
		}
		OnPromptChanged(CurrentText, true);
	}
	else if (bWantsShown != bShown && !bWantsShown)
	{
		OnPromptChanged(CurrentText, false);
	}
	bShown = bWantsShown;

	Opacity = FMath::FInterpTo(Opacity, bShown ? 1.f : 0.f, InDeltaTime, FadeSpeed);
	if (PromptRoot)
	{
		PromptRoot->SetRenderOpacity(Opacity);
		PromptRoot->SetVisibility(Opacity > 0.01f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
