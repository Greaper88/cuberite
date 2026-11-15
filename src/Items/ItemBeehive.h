#pragma once

#include "ItemHandler.h"
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
		if (!a_Player.GetWorld()->SupportsBeesAt(a_PlacePosition))
		{
			return false;
		}

		const auto Facing = cBlockBeehiveHandler::YawToMetaData(std::remainder(a_Player.GetYaw() + 180.0, 360.0));
		const auto NestBit = (m_ItemType == E_ITEM_BEE_NEST) ? E_META_BEEHIVE_NEST : 0;
		return a_Player.PlaceBlock(a_PlacePosition, E_BLOCK_BEEHIVE, static_cast<NIBBLETYPE>(Facing | NestBit));
	}
};
