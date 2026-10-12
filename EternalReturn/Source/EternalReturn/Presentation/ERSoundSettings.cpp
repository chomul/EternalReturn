// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/ERSoundSettings.h"
#include "GAS/ERGameplayTags.h"

TArray<TPair<FGameplayTag, const FERCommonSound*>> UERSoundSettings::GetAll() const
{
	return {
		{ ERTags::Pres_Sfx_Downed, &Downed },
		{ ERTags::Pres_Sfx_Die, &Die },
		{ ERTags::Pres_Sfx_Resurrect, &Resurrect },
		{ ERTags::Pres_Sfx_TeamDown, &TeamDown },
		{ ERTags::Pres_Sfx_TeamRevival, &TeamRevival },
		{ ERTags::Pres_Sfx_Terminate, &Terminate },
		{ ERTags::Pres_Sfx_Craft, &Craft },
		{ ERTags::Pres_Sfx_BoxOpenUI, &BoxOpenUI },
	};
}

const FERCommonSound* UERSoundSettings::Find(FGameplayTag Key) const
{
	for (const TPair<FGameplayTag, const FERCommonSound*>& P : GetAll())
	{
		if (P.Key == Key)
		{
			return P.Value;
		}
	}
	return nullptr;
}
