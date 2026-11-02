// Copyright Epic Games, Inc. All Rights Reserved.

#include "MenuButtonWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

void UMenuButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Label)
	{
		Label->SetText(ButtonLabel);
		Label->SetColorAndOpacity(FSlateColor(NormalColor));
	}
	if (Button)
	{
		Button->OnClicked.AddDynamic(this, &UMenuButtonWidget::HandleClicked);
		Button->OnHovered.AddDynamic(this, &UMenuButtonWidget::HandleHovered);
		Button->OnUnhovered.AddDynamic(this, &UMenuButtonWidget::HandleUnhovered);
	}
	if (Accent)
	{
		Accent->SetRenderTransformPivot(FVector2D(0.f, 0.5f));
	}

	// Starts fully hidden, off to the left: the first tick's entrance math brings it in.
	AppearElapsed = 0.f;
	SetRenderOpacity(0.f);
	SetRenderTranslation(FVector2D(-SlideInDistance, 0.f));
}

void UMenuButtonWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	AppearElapsed += InDeltaTime;
	const float AppearT = AppearDuration > 0.f ? (AppearElapsed - AppearDelay) / AppearDuration : 1.f;
	const float AppearAlpha = FMath::Clamp(AppearT, 0.f, 1.f);
	const float AppearEased = FMath::SmoothStep(0.f, 1.f, AppearAlpha);

	const float NewHoverAlpha = FMath::FInterpTo(HoverAlpha, bHovered ? 1.f : 0.f, InDeltaTime, HoverSpeed);
	if (!FMath::IsNearlyEqual(NewHoverAlpha, HoverAlpha, 0.001f))
	{
		HoverAlpha = NewHoverAlpha;
		OnHoverAlphaChanged(HoverAlpha);
	}

	SetRenderOpacity(AppearEased);

	FWidgetTransform Transform;
	Transform.Translation = FVector2D(-SlideInDistance * (1.f - AppearEased), 0.f);
	const float Scale = FMath::Lerp(1.f, HoverScale, HoverAlpha);
	Transform.Scale = FVector2D(Scale, Scale);
	SetRenderTransform(Transform);

	if (Label)
	{
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::LerpUsingHSV(NormalColor, HoverColor, HoverAlpha)));
	}
	if (Accent)
	{
		Accent->SetRenderOpacity(HoverAlpha);
		FWidgetTransform AccentTransform;
		AccentTransform.Scale = FVector2D(HoverAlpha, 1.f);
		Accent->SetRenderTransform(AccentTransform);
	}
}

void UMenuButtonWidget::HandleClicked()
{
	OnMenuButtonClicked.Broadcast();
}

void UMenuButtonWidget::HandleHovered()
{
	bHovered = true;
}

void UMenuButtonWidget::HandleUnhovered()
{
	bHovered = false;
}
