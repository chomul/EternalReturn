// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERStatCapSettings.h"

UERStatCapSettings::UERStatCapSettings()
{
	CategoryName = TEXT("Game");
	SectionName  = TEXT("ER Stat Caps");
}

const UERStatCapSettings& UERStatCapSettings::Get()
{
	const UERStatCapSettings* Settings = GetDefault<UERStatCapSettings>();
	check(Settings);
	return *Settings;
}
