#include "Codex/CodexTypes.h"

void FTransmissionChain::InitialiseSlots()
{
	Slots.Reset();
	Slots.Reserve(NumTransmissionLinks);

	for (int32 LinkIndex = 0; LinkIndex < NumTransmissionLinks; ++LinkIndex)
	{
		FTransmissionLinkSlot& Slot = Slots.AddDefaulted_GetRef();
		Slot.Link = static_cast<ETransmissionLink>(LinkIndex);
	}
}

FTransmissionLinkSlot* FTransmissionChain::FindSlot(ETransmissionLink Link)
{
	return Slots.FindByPredicate([Link](const FTransmissionLinkSlot& Slot)
	{
		return Slot.Link == Link;
	});
}

const FTransmissionLinkSlot* FTransmissionChain::FindSlot(ETransmissionLink Link) const
{
	return Slots.FindByPredicate([Link](const FTransmissionLinkSlot& Slot)
	{
		return Slot.Link == Link;
	});
}

bool FTransmissionChain::ContainsFragment(FName FragmentID) const
{
	for (const FTransmissionLinkSlot& Slot : Slots)
	{
		if (Slot.FragmentIDs.Contains(FragmentID))
		{
			return true;
		}
	}
	return false;
}
