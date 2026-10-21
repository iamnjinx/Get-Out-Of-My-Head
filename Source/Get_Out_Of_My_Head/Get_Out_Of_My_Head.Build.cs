// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Get_Out_Of_My_Head : ModuleRules
{
	public Get_Out_Of_My_Head(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Get_Out_Of_My_Head",
			"Get_Out_Of_My_Head/Variant_Platforming",
			"Get_Out_Of_My_Head/Variant_Platforming/Animation",
			"Get_Out_Of_My_Head/Variant_Combat",
			"Get_Out_Of_My_Head/Variant_Combat/AI",
			"Get_Out_Of_My_Head/Variant_Combat/Animation",
			"Get_Out_Of_My_Head/Variant_Combat/Gameplay",
			"Get_Out_Of_My_Head/Variant_Combat/Interfaces",
			"Get_Out_Of_My_Head/Variant_Combat/UI",
			"Get_Out_Of_My_Head/Variant_SideScrolling",
			"Get_Out_Of_My_Head/Variant_SideScrolling/AI",
			"Get_Out_Of_My_Head/Variant_SideScrolling/Gameplay",
			"Get_Out_Of_My_Head/Variant_SideScrolling/Interfaces",
			"Get_Out_Of_My_Head/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
