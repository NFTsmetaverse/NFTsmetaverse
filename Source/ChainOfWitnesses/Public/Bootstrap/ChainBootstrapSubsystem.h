#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ChainBootstrapSubsystem.generated.h"

class UDataTable;

DECLARE_LOG_CATEGORY_EXTERN(LogChainBootstrap, Log, All);

/**
 * Loads the content tables into the subsystems that need them, once, at startup.
 *
 * **Why this is not optional.** Every subsystem here is an empty registry until
 * someone calls its Register*Table entry point. Before this existed, nothing did
 * outside the automation tests: the project would open, compile, run, and have no
 * factions, no fragments, no roads and no conversations, with nothing in the log
 * to say why. That is the single worst failure mode in the design, because it
 * looks exactly like working software.
 *
 * **Order matters in one place only.** The registries are independent, but the
 * timeline has to hold the campaign's dates before anything asks what year it is,
 * and validation has to run after everything is registered or it reports
 * cross-references that simply have not loaded yet. Hence: timeline, everything
 * else, then validate.
 *
 * **Idempotent.** Every Register* call refuses duplicate IDs and returns a count,
 * so running this twice registers nothing the second time and says so.
 */
UCLASS()
class CHAINOFWITNESSES_API UChainBootstrapSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * Loads and registers everything named in Project Settings. Called for you at
	 * startup unless bRegisterContentOnStartup is off. Returns the number of rows
	 * registered across all systems.
	 */
	UFUNCTION(BlueprintCallable, Category = "Chain of Witnesses")
	int32 RegisterAllContent();

	/**
	 * Runs every subsystem's content validator and logs what it finds. Returns the
	 * number of problems, so a cook or a smoke test can fail on it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Chain of Witnesses")
	int32 ValidateAllContent(TArray<FString>& OutProblems) const;

	UFUNCTION(BlueprintPure, Category = "Chain of Witnesses")
	bool IsContentRegistered() const { return bContentRegistered; }

private:
	/** Synchronous load. This is startup; the tables are small and nothing can run without them. */
	static UDataTable* Load(const TSoftObjectPtr<UDataTable>& Table, const TCHAR* Label);

	/** Puts the player in the starting era and place once the content is in. */
	void ApplyStartingState();

	UPROPERTY()
	bool bContentRegistered = false;
};
