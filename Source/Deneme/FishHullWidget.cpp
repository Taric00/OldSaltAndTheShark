// Copyright Epic Games, Inc. All Rights Reserved.

#include "FishHullWidget.h"

#include "Components/Border.h"
#include "FishInteractionInterfaces.h"
#include "Kismet/GameplayStatics.h"

void UFishHullWidget::NativeConstruct()
{
	Super::NativeConstruct();

	Segments.Reset();
	for (int32 Index = 0; Index < 16; ++Index)
	{
		if (UBorder* Segment = Cast<UBorder>(GetWidgetFromName(*FString::Printf(TEXT("Seg_%d"), Index))))
		{
			Segments.Add(Segment);
		}
	}
	FindBoat();
}

void UFishHullWidget::FindBoat()
{
	TArray<AActor*> Candidates;
	UGameplayStatics::GetAllActorsWithInterface(this, UFishHullStatus::StaticClass(), Candidates);
	Boat = Candidates.Num() > 0 ? Candidates[0] : nullptr;
	if (const IFishHullStatus* Status = Cast<IFishHullStatus>(Boat.Get()))
	{
		DisplayedHealth = LastHealth = Status->GetHullHealth01();
	}
}

void UFishHullWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// The boat may not exist yet (or was replaced): look again now and then.
	if (!Boat)
	{
		FindTimer -= InDeltaTime;
		if (FindTimer <= 0.f)
		{
			FindTimer = 0.5f;
			FindBoat();
		}
	}

	const IFishHullStatus* Status = Cast<IFishHullStatus>(Boat.Get());
	const float Health = Status ? Status->GetHullHealth01() : 1.f;

	if (Health < LastHealth - KINDA_SMALL_NUMBER)
	{
		SinceHit = 0.f;
		OnHullDamaged(Health, LastHealth - Health);
	}
	LastHealth = Health;
	SinceHit += InDeltaTime;

	// Planks go out a little after the hit, not instantly.
	DisplayedHealth = FMath::FInterpTo(DisplayedHealth, Health, InDeltaTime, DrainSpeed);

	const int32 Count = Segments.Num();
	const float Scaled = DisplayedHealth * Count;
	const float Flash = FMath::Clamp(1.f - SinceHit / FlashTime, 0.f, 1.f);
	const float Breath = DisplayedHealth < LowHealth ? 0.78f + 0.22f * FMath::Sin(GetWorld()->GetTimeSeconds() * 7.f) : 1.f;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		// Plank 0 is lost first: it goes from whole to gone while the scaled health drops from Count to Count - 1.
		const float Integrity = FMath::Clamp(Scaled - (Count - 1 - Index), 0.f, 1.f);

		FLinearColor Colour = Integrity > 0.5f
			? FMath::Lerp(WarnColor, HealthyColor, (Integrity - 0.5f) * 2.f)
			: FMath::Lerp(CriticalColor, WarnColor, Integrity * 2.f);
		Colour = FMath::Lerp(LostColor, Colour, FMath::SmoothStep(0.f, 0.12f, Integrity));
		Colour.A *= Integrity > 0.f ? Breath : 1.f;

		// The flash runs through the planks that were still standing.
		Colour = FMath::Lerp(Colour, FlashColor, Flash * Flash * (Integrity > 0.f ? 0.85f : 0.25f));
		Segments[Index]->SetBrushColor(Colour);
	}

	// Shake and pop of the whole icon.
	if (HullRoot)
	{
		const float Shake = FMath::Clamp(1.f - SinceHit / ShakeTime, 0.f, 1.f);
		const float T = GetWorld()->GetTimeSeconds();
		const FVector2D Offset(FMath::Sin(T * 61.f), FMath::Sin(T * 47.f + 1.3f));
		const float Pop = 1.f + PopScale * Shake * Shake;

		HullRoot->SetRenderTranslation(Offset * (ShakeAmount * Shake * Shake));
		HullRoot->SetRenderScale(FVector2D(Pop, Pop));
	}
}
