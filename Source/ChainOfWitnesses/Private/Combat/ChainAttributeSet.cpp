#include "Combat/ChainAttributeSet.h"

#include "GameplayEffectExtension.h"

UChainAttributeSet::UChainAttributeSet()
{
	// Normalised deliberately. The encounter layer, the era tuning and the content
	// tables all speak in fractions of a man, so the attributes do too, and a
	// designer changing what a wound is worth changes it in one place.
	InitHealth(1.f);
	InitMaxHealth(1.f);
	InitResolve(1.f);
	InitMaxResolve(1.f);
}

void UChainAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetResolveAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxResolve());
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxResolveAttribute())
	{
		NewValue = FMath::Max(NewValue, UE_KINDA_SMALL_NUMBER);
	}
}

void UChainAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void UChainAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	AActor* Instigator = Data.EffectSpec.GetEffectContext().GetOriginalInstigator();
	AActor* Target = Data.Target.GetAvatarActor();

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float Dealt = GetDamage();
		SetDamage(0.f);

		if (Dealt > 0.f)
		{
			SetHealth(FMath::Clamp(GetHealth() - Dealt, 0.f, GetMaxHealth()));

			// A wound discourages a man whether or not it stops him. The encounter
			// layer applies the same rule to combatants it holds directly; this is
			// the same rule for anything driven by an effect.
			SetResolve(FMath::Clamp(GetResolve() - Dealt, 0.f, GetMaxResolve()));

			if (GetHealth() <= 0.f)
			{
				OnAttributeDepleted.Broadcast(Instigator, Target, GetHealthAttribute(), Dealt);
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetResolveLossAttribute())
	{
		const float Lost = GetResolveLoss();
		SetResolveLoss(0.f);

		if (Lost > 0.f)
		{
			SetResolve(FMath::Clamp(GetResolve() - Lost, 0.f, GetMaxResolve()));

			if (GetResolve() <= 0.f)
			{
				OnAttributeDepleted.Broadcast(Instigator, Target, GetResolveAttribute(), Lost);
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetResolveAttribute())
	{
		SetResolve(FMath::Clamp(GetResolve(), 0.f, GetMaxResolve()));
	}
}
