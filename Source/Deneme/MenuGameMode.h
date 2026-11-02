// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MenuGameMode.generated.h"

/**
 * Game mode for the main menu level. Spawns no pawn of its own - the level's own AMenuCameraPawn (placed by
 * hand, AutoPossessPlayer) is what the player sees through - and puts up AMenuHUD for the menu widget.
 */
UCLASS()
class DENEME_API AMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMenuGameMode();
};
