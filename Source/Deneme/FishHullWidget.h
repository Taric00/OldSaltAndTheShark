// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FishHullWidget.generated.h"

class UBorder;

/**
 * Base class of the hull status widget: no numbers, no text. The boat is drawn as a handful of planks. The more
 * damage the hull takes, the more planks fade from white to amber to red and finally go dark, one after another.
 * A hit makes the planks flash and the whole icon shake and pop.
 *
 * Make a Widget Blueprint with this parent and design it freely. It looks for these named widgets:
 *   Seg_0, Seg_1, Seg_2 ...  Border widgets, one per plank (up to 16). Seg_0 is the first to be lost.
 *   HullRoot (optional)      the container that shakes and pops when the hull is hit.
 * Layout, shape, sizes and extra decoration (waves, glow, frames) are entirely up to the Widget Blueprint.
 */
UCLASS(Abstract)
class DENEME_API UFishHullWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** How quickly the shown health follows the real one (1/s). Lower = the planks go out slowly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull", meta = (ClampMin = "0.1"))
	float DrainSpeed = 3.5f;

	/** Seconds the planks flash after a hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull", meta = (ClampMin = "0.05"))
	float FlashTime = 0.45f;

	/** Seconds the icon shakes after a hit, and how far (pixels). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull", meta = (ClampMin = "0.05"))
	float ShakeTime = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull", meta = (ClampMin = "0"))
	float ShakeAmount = 7.f;

	/** The icon swells by this much on a hit and settles back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull", meta = (ClampMin = "0"))
	float PopScale = 0.14f;

	/** Below this health the remaining planks breathe like a warning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull", meta = (ClampMin = "0", ClampMax = "1"))
	float LowHealth = 0.3f;

	/** Colour of a whole plank, a half-broken one, a nearly gone one, and a lost one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull")
	FLinearColor HealthyColor = FLinearColor(0.90f, 0.96f, 0.94f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull")
	FLinearColor WarnColor = FLinearColor(1.f, 0.70f, 0.26f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull")
	FLinearColor CriticalColor = FLinearColor(0.95f, 0.20f, 0.16f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull")
	FLinearColor LostColor = FLinearColor(0.10f, 0.14f, 0.16f, 0.30f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull")
	FLinearColor FlashColor = FLinearColor(1.f, 1.f, 1.f, 1.f);

	/** The hull health the icon currently shows, 0..1. */
	UFUNCTION(BlueprintPure, Category = "Hull")
	float GetDisplayedHealth() const { return DisplayedHealth; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Fires once for every hit with the new health (0..1) and how much it dropped. Use it for extra animation. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hull")
	void OnHullDamaged(float NewHealth, float Drop);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HullRoot;

private:
	void FindBoat();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> Segments;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Boat;

	float DisplayedHealth = 1.f;
	float LastHealth = 1.f;
	float SinceHit = 100.f;
	float FindTimer = 0.f;
};
