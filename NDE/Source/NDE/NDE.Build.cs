// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NDE : ModuleRules
{
	public NDE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Permite includes por subcarpeta, p. ej. "Core/NDTypes.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "GameplayTags", "CommonUI", "CommonInput" });

		PrivateDependencyModuleNames.AddRange(new string[] { "AIModule", "GameplayTasks", "CoreOnline", "DeveloperSettings", "NavigationSystem", "NetCore", "OnlineSubsystem", "OnlineSubsystemUtils", "Slate", "SlateCore" });
	}
}
