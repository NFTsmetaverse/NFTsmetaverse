#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

/**
 * A legal outer for the subsystems these tests construct by hand.
 *
 * UGameInstanceSubsystem declares ClassWithin = UGameInstance, so the transient
 * package is not a valid outer for one. UE 5.8 raises an ensure on that where
 * earlier versions let it pass with a warning -- which is why exactly one test
 * failed on it rather than all forty-five call sites: an ensure fires once, and
 * whichever test ran first wore it.
 *
 * The instance is never torn down on purpose. It outlives every fixture that
 * borrows it, holds no state any test reads, and the process is a test runner.
 */
inline UGameInstance* ChainTestOuter()
{
	static TStrongObjectPtr<UGameInstance> Outer(NewObject<UGameInstance>(GetTransientPackage()));
	return Outer.Get();
}

#endif // WITH_DEV_AUTOMATION_TESTS
