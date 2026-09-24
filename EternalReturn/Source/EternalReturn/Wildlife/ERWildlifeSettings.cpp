// Copyright Epic Games, Inc. All Rights Reserved.

#include "Wildlife/ERWildlifeSettings.h"

#include "EternalReturn.h"
#include "Wildlife/ERWildlifeData.h"

const UERWildlifeData* UERWildlifeSettings::FindData(FName Name)
{
	const FString Path = FString::Printf(TEXT("%s/DA_Wild_%s.DA_Wild_%s"), *Get().DataPath, *Name.ToString(), *Name.ToString());
	const UERWildlifeData* Data = LoadObject<UERWildlifeData>(nullptr, *Path);
	if (!Data)
	{
		UE_LOG(LogEternalReturn, Error, TEXT("[야생동물] 종 정의 애셋이 없다: %s"), *Path);
	}
	return Data;
}
