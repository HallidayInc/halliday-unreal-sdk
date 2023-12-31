// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class HallidaySDK : ModuleRules
{
	public HallidaySDK(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
                Path.Combine(ModuleDirectory, "..", "ThirdParty", "secp256k1", "include"),
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...   
                "HTTP",
                "HTTPServer",
                "Web3AuthSDK",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...
                "Json",
                "JsonUtilities",
                "Web3AuthSDK",
                "UMG",
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
   
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // SECP256K1 library requires this to be defined if using a static library on Windows
            PublicDefinitions.Add("SECP256K1_STATIC");
            
            // Add any static public static libraries
            PublicSystemLibraries.AddRange(
                new string[]
                {
                    "BCrypt.lib"
                }
            );
            
            PublicAdditionalLibraries.Add(
                Path.Combine(ModuleDirectory, "..", "ThirdParty", "secp256k1", "lib", "libsecp256k1.lib")
            );
        }
        else
        {
            PublicAdditionalLibraries.Add(
                Path.Combine(ModuleDirectory, "..", "ThirdParty", "secp256k1", "lib", "libsecp256k1.a")
            );
        }
	}
}
