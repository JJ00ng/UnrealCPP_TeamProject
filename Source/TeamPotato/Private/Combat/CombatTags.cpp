#include "Combat/CombatTags.h"

namespace CombatTags
{
    UE_DEFINE_GAMEPLAY_TAG(Data_Damage, "Data.Damage");
    UE_DEFINE_GAMEPLAY_TAG(Data_Healing, "Data.Healing");
    UE_DEFINE_GAMEPLAY_TAG(Data_MaxHealth, "Data.MaxHealth");
    UE_DEFINE_GAMEPLAY_TAG(State_Invincible, "State.Invincible");
    UE_DEFINE_GAMEPLAY_TAG(State_Dead, "State.Dead");
    UE_DEFINE_GAMEPLAY_TAG(Damage_Periodic, "Damage.Periodic");
    UE_DEFINE_GAMEPLAY_TAG(Actor_Player, "Actor.Type.Player");
    UE_DEFINE_GAMEPLAY_TAG(State_Poison, "State.Debuff.Poison");
    UE_DEFINE_GAMEPLAY_TAG(State_Fire, "State.Debuff.Burning");
    UE_DEFINE_GAMEPLAY_TAG(State_Electric, "State.Debuff.Shocked");
    UE_DEFINE_GAMEPLAY_TAG(State_AttackBlocked, "State.AttackBlocked");
    UE_DEFINE_GAMEPLAY_TAG(State_DamageReduction, "State.DamageReduction");
    UE_DEFINE_GAMEPLAY_TAG(GameplayCue_KnightBuff, "GameplayCue.KnightBuff");
    UE_DEFINE_GAMEPLAY_TAG(Cooldown_KnightBuffCoolDown, "Cooldown.KnightBuffCoolDown");
    UE_DEFINE_GAMEPLAY_TAG(Cooldown_TurretSommonCoolDown, "Cooldown.TurretSommonCoolDown");
}
