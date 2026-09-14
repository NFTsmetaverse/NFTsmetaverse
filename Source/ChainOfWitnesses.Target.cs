using UnrealBuildTool;
using System.Collections.Generic;

public class ChainOfWitnessesTarget : TargetRules
{
	public ChainOfWitnessesTarget(TargetInfo Target) : base(Target)
	{
		// Pinned enum values (BuildSettingsVersion.V5, EngineIncludeOrderVersion.Unreal5_4)
		// are retired by UBT a few releases after they ship, and Target.cs is C# --
		// a stale value fails the build before any C++ is compiled. Latest always
		// exists, and its stricter include order surfaces missing includes rather
		// than letting transitive ones paper over them.
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.Add("ChainOfWitnesses");
	}
}
