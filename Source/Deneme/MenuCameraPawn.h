// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MenuCameraPawn.generated.h"

class UCameraComponent;

/**
 * A plain camera, no movement, no input. Place it in the level and point it wherever you like in the editor
 * viewport: it auto-possesses Player 0, so the game starts from, and looks exactly where, you left it.
 */
UCLASS()
class DENEME_API AMenuCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AMenuCameraPawn();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;
};
