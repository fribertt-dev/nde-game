// No Despiertes Eso

#include "Monsters/NDMonsterDefinition.h"

FPrimaryAssetId UNDMonsterDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("NDMonster"), MonsterId);
}
