// Copyright Epic Games, Inc. All Rights Reserved.

#include "FishCrosshairWidget.h"

#include "Components/Widget.h"
#include "FishInteractionInterfaces.h"
#include "GameFramework/Pawn.h"

void UFishCrosshairWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const IFishAimProvider* Provider = Cast<IFishAimProvider>(GetOwningPlayerPawn());
	const float NewAlpha = Provider ? Provider->GetAimAlpha() : 0.f;

	if (!FMath::IsNearlyEqual(NewAlpha, AimAlpha, 0.001f))
	{
		AimAlpha = NewAlpha;
		OnAimChanged(AimAlpha);
	}

	// Only the opacity changes: a collapsed widget would stop ticking and never come back.
	SetRenderOpacity(AimAlpha);

	if (HitMarkerPanel)
	{
		const float HitAlpha = Provider ? Provider->GetHitMarkerAlpha() : 0.f;
		HitMarkerPanel->SetVisibility(HitAlpha > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		HitMarkerPanel->SetRenderOpacity(HitAlpha);
	}
}

void UFishCrosshairWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(0.f);
}
