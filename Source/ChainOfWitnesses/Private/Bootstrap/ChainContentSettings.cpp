#include "Bootstrap/ChainContentSettings.h"

UChainContentSettings::UChainContentSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Chain of Witnesses Content");
}

const UChainContentSettings* UChainContentSettings::Get()
{
	return GetDefault<UChainContentSettings>();
}
