#include "Loot/NDLootDefinition.h"

int32 UNDLootDefinition::GetRequiredCarriers() const
{
	if (RequiredCarriersOverride > 0)
	{
		return RequiredCarriersOverride;
	}
	switch (Category)
	{
	case ENDLootCategory::Heavy:
	case ENDLootCategory::Huge:
		return 2;
	default:
		return 1;
	}
}

float UNDLootDefinition::GetDamageSpeedThreshold() const
{
	if (DamageSpeedThreshold > 0.f)
	{
		return DamageSpeedThreshold;
	}
	switch (Fragility)
	{
	case ENDFragility::Critical: return 300.f;
	case ENDFragility::Delicate: return 500.f;
	default:                     return 900.f;
	}
}

float UNDLootDefinition::GetBreakSpeedThreshold() const
{
	if (BreakSpeedThreshold > 0.f)
	{
		return BreakSpeedThreshold;
	}
	switch (Fragility)
	{
	case ENDFragility::Critical: return 600.f;
	case ENDFragility::Delicate: return 1000.f;
	default:                     return 1600.f;
	}
}

float UNDLootDefinition::GetConditionFactor(ENDLootCondition Condition) const
{
	switch (Condition)
	{
	case ENDLootCondition::Damaged: return DamagedValueFactor;
	case ENDLootCondition::Broken:  return BrokenValueFactor;
	default:                        return 1.f;
	}
}
