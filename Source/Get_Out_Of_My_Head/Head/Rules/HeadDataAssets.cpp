#include "HeadDataAssets.h"

const FHeadStatusTier* UHeadStatusData::FindTier(int32 Stacks) const
{
	const FHeadStatusTier* Best = nullptr;

	for (const FHeadStatusTier& Tier : Tiers)
	{
		if (Stacks >= Tier.MinStacks && (!Best || Tier.MinStacks > Best->MinStacks))
		{
			Best = &Tier;
		}
	}

	return Best;
}

const FHeadAttachSlot* UHeadObjectData::FindAttachSlot(const UHeadObjectData* Item) const
{
	return AttachSlots.FindByPredicate([Item](const FHeadAttachSlot& Slot) { return Slot.Item == Item; });
}
