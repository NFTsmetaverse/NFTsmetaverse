#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/DataTable.h"
#include "Campaign/CampaignEventRow.h"
#include "CampaignTimelineSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCampaignYearAdvanced, int32, NewYearAD);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCampaignEventActivated, FName, EventID);

/**
 * Era-gating subsystem for the Section 6 historical spine (Task 10.1).
 *
 * Owns the campaign clock and derives world state -- active events, established/
 * destroyed hub cities, unlocked factions, and the current act -- from a single
 * FCampaignEventRow DataTable. Downstream systems (Codex, Task 2; dialogue
 * disclosure, Task 4; faction reputation, Task 5; campaign map, Task 6) should read
 * world state from here rather than re-deriving it, so Mode A/Mode B (Section 3)
 * stay a data swap instead of parallel logic.
 *
 * An event activates once CurrentYearAD reaches its YearEarliestAD and its
 * prerequisites are active; it stays active afterward. These are historical anchor
 * points that happened and then remain true (the Jerusalem church, once founded,
 * doesn't un-found itself), not a closing time window -- YearLatestAD documents the
 * outer edge of the defensible dating range for UI/debate purposes rather than
 * gating deactivation.
 */
UCLASS()
class CHAINOFWITNESSES_API UCampaignTimelineSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Points the subsystem at the Section 6 timeline DataTable (row struct
	// FCampaignEventRow, see Content/Data/DT_CampaignEvents.json). Call once at
	// startup, e.g. from the GameMode or the mode-select asset that configures
	// Mode A/Mode B. Re-evaluates world state at the current year immediately.
	UFUNCTION(BlueprintCallable, Category = "Campaign Timeline")
	void SetTimelineTable(UDataTable* InTimelineTable);

	// Advances the in-fiction campaign clock and re-evaluates which events are
	// active, broadcasting OnEventActivated for anything newly unlocked and
	// OnYearAdvanced once at the end.
	UFUNCTION(BlueprintCallable, Category = "Campaign Timeline")
	void AdvanceToYear(int32 NewYearAD);

	UFUNCTION(BlueprintPure, Category = "Campaign Timeline")
	int32 GetCurrentYearAD() const { return CurrentYearAD; }

	// Highest act whose act-defining year has been reached. Acts overlap in the
	// source table by design (e.g. Act II begins in AD 46, before Act I's last row
	// at AD 44), so this reports the furthest act reached, not an exclusive range.
	UFUNCTION(BlueprintPure, Category = "Campaign Timeline")
	ECampaignAct GetCurrentAct() const;

	UFUNCTION(BlueprintPure, Category = "Campaign Timeline")
	bool IsEventActive(FName EventID) const;

	UFUNCTION(BlueprintPure, Category = "Campaign Timeline")
	bool IsHubCityEstablished(FName HubCityID) const;

	UFUNCTION(BlueprintPure, Category = "Campaign Timeline")
	bool IsFactionUnlocked(FName FactionID) const;

	UFUNCTION(BlueprintCallable, Category = "Campaign Timeline")
	void GetActiveEventIDs(TArray<FName>& OutEventIDs) const;

	// Fails (returns false) if EventID isn't in the table.
	UFUNCTION(BlueprintCallable, Category = "Campaign Timeline")
	bool GetEventRow(FName EventID, FCampaignEventRow& OutRow) const;

	UPROPERTY(BlueprintAssignable, Category = "Campaign Timeline")
	FOnCampaignYearAdvanced OnYearAdvanced;

	UPROPERTY(BlueprintAssignable, Category = "Campaign Timeline")
	FOnCampaignEventActivated OnEventActivated;

private:
	bool EvaluatePrerequisites(const FCampaignEventRow& Row) const;
	void ApplyWorldStateEffects(const FCampaignEventRow& Row);

	UPROPERTY()
	TObjectPtr<UDataTable> TimelineTable = nullptr;

	UPROPERTY()
	int32 CurrentYearAD = 30;

	UPROPERTY()
	TSet<FName> ActiveEventIDs;

	UPROPERTY()
	TSet<FName> EstablishedHubCityIDs;

	UPROPERTY()
	TSet<FName> DestroyedHubCityIDs;

	UPROPERTY()
	TSet<FName> UnlockedFactionIDs;
};
