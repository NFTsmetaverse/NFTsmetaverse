#include "Codex/TestimonyFragment.h"

FPrimaryAssetId UTestimonyFragment::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("TestimonyFragment"), Definition.FragmentID);
}

bool UTestimonyFragment::IsTierAppropriateForLink(EWitnessTier Tier, ETransmissionLink Link)
{
	switch (Link)
	{
	case ETransmissionLink::Event:
		// That the event occurred is carried by sources with no stake in the claim.
		return Tier == EWitnessTier::Hostile || Tier == EWitnessTier::Archaeological;

	case ETransmissionLink::Eyewitness:
		return Tier == EWitnessTier::Eyewitness || Tier == EWitnessTier::Companion;

	case ETransmissionLink::OralProclamation:
		// Patristic sources belong here too: Papias is the main testimony that an
		// oral chain existed and what it carried.
		return Tier == EWitnessTier::Eyewitness
			|| Tier == EWitnessTier::Companion
			|| Tier == EWitnessTier::Patristic;

	case ETransmissionLink::WrittenSource:
		return Tier == EWitnessTier::Eyewitness || Tier == EWitnessTier::Companion;

	case ETransmissionLink::Manuscript:
		return Tier == EWitnessTier::Manuscript || Tier == EWitnessTier::Archaeological;

	case ETransmissionLink::PatristicAttestation:
		return Tier == EWitnessTier::Patristic;

	default:
		return false;
	}
}
