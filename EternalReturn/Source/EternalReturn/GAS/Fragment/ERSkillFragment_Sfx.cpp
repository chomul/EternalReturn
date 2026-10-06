// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Fragment/ERSkillFragment_Sfx.h"

#include "Presentation/ERPresentationComponent.h"

void UERSkillFragment_Sfx::OnCastStart(FERSkillContext& Ctx) const
{
	if (Ctx.bAuthority && Ctx.Avatar && CastStartSfx.IsValid())
	{
		UERPresentationComponent::SendSfxCue(Ctx.Avatar, CastStartSfx, Ctx.Avatar->GetActorLocation());
	}
}

void UERSkillFragment_Sfx::OnExecute(FERSkillContext& Ctx) const
{
	if (Ctx.bAuthority && Ctx.Avatar && ExecuteSfx.IsValid() && !Ctx.bActivatedByRecast)
	{
		UERPresentationComponent::SendSfxCue(Ctx.Avatar, ExecuteSfx, Ctx.Avatar->GetActorLocation());
	}
}
