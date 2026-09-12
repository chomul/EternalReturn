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
	UE_DEFINE_GAMEPLAY_TAG(State_DamageImmune,   "State.DamageImmune");
	UE_DEFINE_GAMEPLAY_TAG(State_Unstoppable,    "State.Unstoppable");
	UE_DEFINE_GAMEPLAY_TAG(State_Untargetable,   "State.Untargetable");
	UE_DEFINE_GAMEPLAY_TAG(State_CCImmune,       "State.CCImmune");

	UE_DEFINE_GAMEPLAY_TAG(State_Block_Movement,    "State.Block.Movement");
	UE_DEFINE_GAMEPLAY_TAG(State_Block_BasicAttack, "State.Block.BasicAttack");
	UE_DEFINE_GAMEPLAY_TAG(State_Block_Skill,       "State.Block.Skill");

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

	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_CCDuration,           "SetByCaller.CCDuration");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_SlowPercent,          "SetByCaller.SlowPercent");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_SlowMultiplier,       "SetByCaller.SlowMultiplier");

	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_MaxHP,                "SetByCaller.MaxHP");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_MaxVP,                "SetByCaller.MaxVP");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_HP,                   "SetByCaller.HP");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_VP,                   "SetByCaller.VP");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_HPRegen,              "SetByCaller.HPRegen");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_VPRegen,              "SetByCaller.VPRegen");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_AttackPower,          "SetByCaller.AttackPower");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Defense,              "SetByCaller.Defense");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_AttackSpeed,          "SetByCaller.AttackSpeed");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_MoveSpeed,            "SetByCaller.MoveSpeed");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Sight,                "SetByCaller.Sight");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_AttackRange,          "SetByCaller.AttackRange");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_CritChance,           "SetByCaller.CritChance");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_CritDamageUp,         "SetByCaller.CritDamageUp");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_SkillAmp,             "SetByCaller.SkillAmp");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_BasicAtkAmp,          "SetByCaller.BasicAtkAmp");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_DefPenPercent,        "SetByCaller.DefPenPercent");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_DefPenFlat,           "SetByCaller.DefPenFlat");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_DamageUp,             "SetByCaller.DamageUp");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_FinalDamageUpPercent, "SetByCaller.FinalDamageUpPercent");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_FinalDamageUpFlat,    "SetByCaller.FinalDamageUpFlat");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_SkillHaste,           "SetByCaller.SkillHaste");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_DamageDown,           "SetByCaller.DamageDown");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_BasicAtkDamageDown,   "SetByCaller.BasicAtkDamageDown");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_SkillDamageDown,      "SetByCaller.SkillDamageDown");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_SlowResist,           "SetByCaller.SlowResist");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_CCResist,             "SetByCaller.CCResist");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Lifesteal,            "SetByCaller.Lifesteal");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_OmniLifesteal,        "SetByCaller.OmniLifesteal");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_HealAmp,              "SetByCaller.HealAmp");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_OutOfCombatRegen,     "SetByCaller.OutOfCombatRegen");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_ModeDamageUp,         "SetByCaller.ModeDamageUp");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_ModeDamageDown,       "SetByCaller.ModeDamageDown");

	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_HealAmount,           "SetByCaller.HealAmount");

	UE_DEFINE_GAMEPLAY_TAG(Data_Damage_Base,             "Data.Damage.Base");
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage_APRatio,          "Data.Damage.APRatio");
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage_BonusAPRatio,     "Data.Damage.BonusAPRatio");
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage_SkillAmpRatio,    "Data.Damage.SkillAmpRatio");
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage_MaxHPRatio,       "Data.Damage.MaxHPRatio");
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage_CurHPRatio,       "Data.Damage.CurHPRatio");
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage_LostHPRatio,      "Data.Damage.LostHPRatio");

	UE_DEFINE_GAMEPLAY_TAG(Damage_Shape_AoE,    "Damage.Shape.AoE");
	UE_DEFINE_GAMEPLAY_TAG(Actor_Type_Wildlife, "Actor.Type.Wildlife");
	UE_DEFINE_GAMEPLAY_TAG(Actor_Type_Boss,     "Actor.Type.Boss");
}
