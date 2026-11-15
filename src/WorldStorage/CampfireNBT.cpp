#include "Globals.h"
#include "CampfireNBT.h"
#include "FastNBT.h"
#include "PalettedItemNBT.h"
#include "../BlockEntities/CampfireEntity.h"





namespace
{
	constexpr size_t SlotCount = std::tuple_size_v<cCampfireEntity::cCookingSlots>;
	using CookingTimes = std::array<Int32, SlotCount>;

	bool ReadTimes(const cParsedNBT & a_NBT, int a_Tag, const char * a_Name, CookingTimes & a_Times)
	{
		const auto Tag = a_NBT.FindChildByName(a_Tag, a_Name);
		if (Tag < 0)
		{
			// Campfires saved before cooking support can omit the timers.
			return true;
		}
		if (a_NBT.GetType(Tag) != TAG_IntArray)
		{
			return false;
		}
		const auto Count = a_NBT.GetDataLength(Tag) / sizeof(Int32);
		if (Count > a_Times.size())
		{
			return false;
		}
		for (size_t Slot = 0; Slot < Count; ++Slot)
		{
			const auto Time = NetworkBufToHost<Int32>(a_NBT.GetData(Tag) + Slot * sizeof(Int32));
			if (Time < 0)
			{
				return false;
			}
			a_Times[Slot] = Time;
		}
		return true;
	}
}





bool CampfireNBT::Read(cCampfireEntity & a_Campfire, const cParsedNBT & a_NBT, int a_Tag, int a_DataVersion)
{
	CookingTimes Progress{}, Total{};
	if (
		!ReadTimes(a_NBT, a_Tag, "CookingTimes", Progress) ||
		!ReadTimes(a_NBT, a_Tag, "CookingTotalTimes", Total)
	)
	{
		return false;
	}

	cCampfireEntity::cCookingSlots Slots{};
	for (size_t Slot = 0; Slot < Slots.size(); ++Slot)
	{
		Slots[Slot].Progress = Progress[Slot];
		Slots[Slot].Total = Total[Slot];
	}
	const auto Items = a_NBT.FindChildByName(a_Tag, "Items");
	if (Items >= 0)
	{
		if (
			(a_NBT.GetType(Items) != TAG_List) ||
			((a_NBT.GetChildrenType(Items) != TAG_Compound) && (a_NBT.GetChildrenType(Items) != TAG_End))
		)
		{
			return false;
		}
		std::array<bool, SlotCount> Seen{};
		for (auto Item = a_NBT.GetFirstChild(Items); Item >= 0; Item = a_NBT.GetNextSibling(Item))
		{
			const auto SlotTag = a_NBT.FindChildByName(Item, "Slot");
			if ((SlotTag < 0) || (a_NBT.GetType(SlotTag) != TAG_Byte))
			{
				return false;
			}
			const auto Slot = a_NBT.GetByte(SlotTag);
			if ((Slot >= Slots.size()) || Seen[Slot] || !PalettedItemNBT::Read(Slots[Slot].Item, a_NBT, Item, a_DataVersion))
			{
				return false;
			}
			Seen[Slot] = true;
		}
	}
	a_Campfire.SetCookingSlots(std::move(Slots));
	return true;
}





void CampfireNBT::WriteItems(cFastNBTWriter & a_Writer, const cCampfireEntity & a_Campfire, int a_DataVersion)
{
	const auto & Slots = a_Campfire.GetCookingSlots();
	a_Writer.BeginList("Items", TAG_Compound);
	for (size_t Slot = 0; Slot < Slots.size(); ++Slot)
	{
		if (!Slots[Slot].Item.IsEmpty())
		{
			PalettedItemNBT::Write(a_Writer, Slots[Slot].Item, static_cast<int>(Slot), "", a_DataVersion);
		}
	}
	a_Writer.EndList();
}





void CampfireNBT::Write(cFastNBTWriter & a_Writer, const cCampfireEntity & a_Campfire, int a_DataVersion)
{
	WriteItems(a_Writer, a_Campfire, a_DataVersion);
	const auto & Slots = a_Campfire.GetCookingSlots();
	CookingTimes Progress{}, Total{};
	for (size_t Slot = 0; Slot < Slots.size(); ++Slot)
	{
		Progress[Slot] = Slots[Slot].Progress;
		Total[Slot] = Slots[Slot].Total;
	}
	a_Writer.AddIntArray("CookingTimes", Progress.data(), Progress.size());
	a_Writer.AddIntArray("CookingTotalTimes", Total.data(), Total.size());
}
