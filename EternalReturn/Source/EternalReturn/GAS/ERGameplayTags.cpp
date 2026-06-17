// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/ERGameplayTags.h"

namespace ERTags
{
	UE_DEFINE_GAMEPLAY_TAG(State_CC_Stun,        "State.CC.Stun");
	UE_DEFINE_GAMEPLAY_TAG(State_CC_Snare,       "State.CC.Snare");
	UE_DEFINE_GAMEPLAY_TAG(State_CC_Silence,     "State.CC.Silence");
	UE_DEFINE_GAMEPLAY_TAG(State_CC_Disarm,      "State.CC.Disarm");
	UE_DEFINE_GAMEPLAY_TAG(State_CC_Slow,        "State.CC.Slow");
	UE_DEFINE_GAMEPLAY_TAG(State_CC_Blind,       "State.CC.Blind");
	UE_DEFINE_GAMEPLAY_TAG(State_CC_VisionBlock, "State.CC.VisionBlock");

	UE_DEFINE_GAMEPLAY_TAG(State_Stealth,        "State.Stealth");
	UE_DEFINE_GAMEPLAY_TAG(State_Invulnerable,   "State.Invulnerable");
	UE_DEFINE_GAMEPLAY_TAG(State_CCImmune,       "State.CCImmune");

	UE_DEFINE_GAMEPLAY_TAG(Ability_Form_Channeled,      "Ability.Form.Channeled");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Form_NextAttackBuff, "Ability.Form.NextAttackBuff");

	UE_DEFINE_GAMEPLAY_TAG(Ability_Slot_P, "Ability.Slot.P");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Slot_Q, "Ability.Slot.Q");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Slot_W, "Ability.Slot.W");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Slot_E, "Ability.Slot.E");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Slot_R, "Ability.Slot.R");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Slot_D, "Ability.Slot.D");

	UE_DEFINE_GAMEPLAY_TAG(Damage_Type_BasicAttack, "Damage.Type.BasicAttack");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Type_Skill,       "Damage.Type.Skill");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Type_True,        "Damage.Type.True");
}
