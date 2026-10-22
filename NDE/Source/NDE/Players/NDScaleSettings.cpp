// No Despiertes Eso

#include "Players/NDScaleSettings.h"

UNDScaleSettings::UNDScaleSettings() = default;

float UNDScaleSettings::GetJumpApex(float GravityZ) const
{
	const float Gravity = FMath::Abs(GravityZ);
	return Gravity > KINDA_SMALL_NUMBER ? JumpZVelocity * JumpZVelocity / (2.f * Gravity) : 0.f;
}
