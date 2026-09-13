#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "ChainAttributeSet.generated.h"

// The standard GAS accessor block: getter, setter, initialiser and a direct
// property accessor for each attribute.
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

DECLARE_MULTICAST_DELEGATE_FourParams(FOnChainAttributeDepleted, AActor* /*Instigator*/, AActor* /*Target*/,
	FGameplayAttribute /*Attribute*/, float /*Magnitude*/);

/**
 * The per-pawn numbers behind an encounter.
 *
 * **Why this exists separately from UCombatEncounterSubsystem.** The subsystem owns
 * the encounter: who is in it, how it can end, and what it costs afterwards. This
 * owns one man's condition, and it is a UAttributeSet because that is what lets
 * abilities, effects and damage sources written later modify it without any of them
 * knowing about each other. Section 9 asks for GAS; this is the part of GAS that
 * can honestly be written without the editor.
 *
 * **Health and Resolve are deliberately parallel.** A man in this game can be
 * stopped two ways and they are not interchangeable: bleeding him out is loud,
 * permanent, and produces a deed that travels; breaking his nerve is quiet and
 * reversible. Most encounters here should end the second way, so the second way has
 * a real attribute rather than being a flag on the first.
 *
 * **Damage and ResolveLoss are meta-attributes**: transient inputs that effects
 * write and PostGameplayEffectExecute immediately converts into Health and Resolve.
 * This is the standard GAS pattern and it is what keeps clamping, death detection
 * and shock propagation in one place instead of in every effect.
 */
UCLASS()
class CHAINOFWITNESSES_API UChainAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UChainAttributeSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Health")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UChainAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Health")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UChainAttributeSet, MaxHealth)

	// How much fight is left in him. Runs down faster than Health and recovers,
	// which is the whole difference between the two.
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Resolve")
	FGameplayAttributeData Resolve;
	ATTRIBUTE_ACCESSORS(UChainAttributeSet, Resolve)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Resolve")
	FGameplayAttributeData MaxResolve;
	ATTRIBUTE_ACCESSORS(UChainAttributeSet, MaxResolve)

	// Meta-attribute. Written by damage effects, never held.
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UChainAttributeSet, Damage)

	// Meta-attribute for everything that costs nerve without costing blood: being
	// shouted down, seeing a companion break, being outnumbered.
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData ResolveLoss;
	ATTRIBUTE_ACCESSORS(UChainAttributeSet, ResolveLoss)

	/** Fires when Health or Resolve reaches zero. The pawn layer decides what that looks like. */
	FOnChainAttributeDepleted OnAttributeDepleted;

private:
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
};
