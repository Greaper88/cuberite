#pragma once

#include "ItemHandler.h"
#include "BlockEntities/BeehiveEntity.h"
#include "WorldStorage/BeeNBT.h"
#include "Blocks/BlockBeehive.h"
#include "Entities/Player.h"
#include "World.h"





class cItemBeehiveHandler final :
	public cItemHandler
{
public:

	using cItemHandler::cItemHandler;

private:

	virtual bool IsPlaceable() const override
	{
		return true;
	}

	virtual bool CommitPlacement(
		cPlayer & a_Player, const cItem & a_HeldItem, Vector3i a_PlacePosition, eBlockFace a_ClickedBlockFace,
		Vector3i a_CursorPosition
	) const override
	{
		auto & World = *a_Player.GetWorld();
		cWorld::cLock Lock(World);
		if (!World.SupportsBeesAt(a_PlacePosition))
		{
			return false;
		}

		const auto Facing = cBlockBeehiveHandler::YawToMetaData(std::remainder(a_Player.GetYaw() + 180.0, 360.0));
		const auto NestBit = (m_ItemType == E_ITEM_BEE_NEST) ? E_META_BEEHIVE_NEST : 0;
		const auto Meta = static_cast<NIBBLETYPE>(Facing | NestBit);
		if (a_HeldItem.m_BeeHomeData.empty())
		{
			return a_Player.PlaceBlock(a_PlacePosition, E_BLOCK_BEEHIVE, Meta);
		}

		cBeehiveEntity Stored(E_BLOCK_BEEHIVE, Meta, a_PlacePosition, nullptr);
		if (
			!BeeNBT::LoadItemData(Stored, a_HeldItem.m_BeeHomeData) ||
			!a_Player.PlaceBlock(a_PlacePosition, E_BLOCK_BEEHIVE, Meta)
		)
		{
			return false;
		}

		return World.DoWithBlockEntityAt(a_PlacePosition, [&Stored](cBlockEntity & a_Entity)
		{
			if (a_Entity.GetBlockType() != E_BLOCK_BEEHIVE)
			{
				return false;
			}
			auto & Hive = static_cast<cBeehiveEntity &>(a_Entity);
			if (Hive.IsNest() != Stored.IsNest())
			{
				return false;
			}
			Hive.SetOccupants(Stored.TakeOccupants());
			Hive.SetHoneyLevel(Stored.GetHoneyLevel());
			return true;
		});
		return true;
	}
};
