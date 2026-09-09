// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERDamageStatics.h"

const FERDamageStatics& ERDamageStatics()
{
	static FERDamageStatics Statics;
	return Statics;
}
