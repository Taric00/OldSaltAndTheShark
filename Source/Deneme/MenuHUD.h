// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MenuHUD.generated.h"

class UMenuWidget;

/** Puts the main menu Widget Blueprint on screen and switches the player to mouse/UI input. */
UCLASS()
class DENEME_API AMenuHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	/** Widget Blueprint (parent UMenuWidget) for the main menu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widgets")
	TSubclassOf<UMenuWidget> MenuWidgetClass;

	UFUNCTION(BlueprintPure, Category = "Widgets")
	UMenuWidget* GetMenuWidget() const { return MenuWidget; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UMenuWidget> MenuWidget;
};
