// Copyright Epic Games, Inc. All Rights Reserved.

#include "MenuGameMode.h"

#include "MenuHUD.h"

AMenuGameMode::AMenuGameMode()
{
	DefaultPawnClass = nullptr;
	HUDClass = AMenuHUD::StaticClass();
}
