// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FishPromptWidget.generated.h"

class UTextBlock;

/**
 * Base class of the interaction prompt widget ("E - Tekneyi Kullan"). Make a Widget Blueprint with this as its
 * parent and design it freely. It only needs two optional named widgets:
 *   PromptText - a Text Block that receives the text
 *   PromptRoot - the widget that is faded in and out (any container, e.g. a Border around the text)
 * For custom animation, implement the OnPromptChanged event in the Widget Blueprint.
 */
UCLASS(Abstract)
class DENEME_API UFishPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Fade speed of PromptRoot (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt", meta = (ClampMin = "0.1"))
	float FadeSpeed = 12.f;

	UFUNCTION(BlueprintPure, Category = "Prompt")
	FText GetCurrentPromptText() const { return CurrentText; }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Fired when the text changes or the prompt appears / disappears. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Prompt")
	void OnPromptChanged(const FText& NewText, bool bVisible);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PromptRoot;

private:
	FText CurrentText;
	bool bShown = false;
	float Opacity = 0.f;
};
