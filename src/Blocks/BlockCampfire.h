#pragma once

#include "Mixins/Mixins.h"





class cBlockCampfireHandler final :
	public cYawRotator<
		cBlockEntityHandler, 0x03,
		E_META_CAMPFIRE_FACING_ZM, E_META_CAMPFIRE_FACING_XP, E_META_CAMPFIRE_FACING_ZP, E_META_CAMPFIRE_FACING_XM
	>
{
	using Super = cYawRotator<
		cBlockEntityHandler, 0x03,
		E_META_CAMPFIRE_FACING_ZM, E_META_CAMPFIRE_FACING_XP, E_META_CAMPFIRE_FACING_ZP, E_META_CAMPFIRE_FACING_XM
	>;

public:

	using Super::Super;

private:

	void OnNeighborChanged(cChunkInterface & a_ChunkInterface, Vector3i a_BlockPos, eBlockFace a_WhichNeighbor) const override
	{
		if ((a_WhichNeighbor != BLOCK_FACE_YM) || (a_BlockPos.y == 0))
		{
			return;
		}

		const auto Meta = a_ChunkInterface.GetBlockMeta(a_BlockPos);
		const auto Signal = (a_ChunkInterface.GetBlock(a_BlockPos.addedY(-1)) == E_BLOCK_HAY_BALE) ? E_META_CAMPFIRE_SIGNAL : 0;
		const auto NewMeta = static_cast<NIBBLETYPE>((Meta & ~E_META_CAMPFIRE_SIGNAL) | Signal);
		if (Meta != NewMeta)
		{
			a_ChunkInterface.SetBlockMeta(a_BlockPos, NewMeta);
		}
	}

	cItems ConvertToPickups(NIBBLETYPE a_Meta, const cItem * a_Tool) const override
	{
		return ToolHasSilkTouch(a_Tool) ? cItem(E_BLOCK_CAMPFIRE) : cItem(E_ITEM_COAL, 2, E_META_COAL_CHARCOAL);
	}

	ColourID GetMapBaseColourID(NIBBLETYPE a_Meta) const override
	{
		return 15;
	}
};
