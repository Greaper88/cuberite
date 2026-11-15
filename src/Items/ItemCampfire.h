#pragma once

#include "ItemHandler.h"
#include "BlockEntities/CampfireEntity.h"
#include "BlockInfo.h"
#include "Blocks/BlockCampfire.h"
#include "Entities/Player.h"
#include "World.h"





class cItemCampfireHandler final :
	public cItemHandler
{
public:

	using cItemHandler::cItemHandler;

private:

	bool IsPlaceable() const override
	{
		return true;
	}

	bool CommitPlacement(
		cPlayer & a_Player, const cItem & a_HeldItem, Vector3i a_PlacePosition, eBlockFace a_ClickedBlockFace,
		Vector3i a_CursorPosition
	) const override
	{
		auto & World = *a_Player.GetWorld();
		if (!World.SupportsCampfiresAt(a_PlacePosition))
		{
			return false;
		}
		const auto Facing = cBlockCampfireHandler::YawToMetaData(a_Player.GetYaw());
		const bool Waterlogged = IsBlockWater(World.GetBlock(a_PlacePosition));
		const bool OnHay = (a_PlacePosition.y > 0) && (World.GetBlock(a_PlacePosition.addedY(-1)) == E_BLOCK_HAY_BALE);
		const auto Meta = static_cast<NIBBLETYPE>(Facing | (Waterlogged ? 0 : E_META_CAMPFIRE_LIT) | (OnHay ? E_META_CAMPFIRE_SIGNAL : 0));
		if (!a_Player.PlaceBlock(a_PlacePosition, E_BLOCK_CAMPFIRE, Meta))
		{
			return false;
		}

		if (Waterlogged)
		{
			World.DoWithBlockEntityAt(a_PlacePosition, [](cBlockEntity & a_Entity)
			{
				if (a_Entity.GetBlockType() == E_BLOCK_CAMPFIRE)
				{
					static_cast<cCampfireEntity &>(a_Entity).SetWaterlogged(true);
				}
				return true;
			});
		}
		return true;
	}
};
