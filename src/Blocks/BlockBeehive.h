#pragma once

#include "Mixins/Mixins.h"





class cBlockBeehiveHandler final :
	public cYawRotator<
		cBlockEntityHandler, 0x03,
		E_META_BEEHIVE_FACING_ZM, E_META_BEEHIVE_FACING_XP, E_META_BEEHIVE_FACING_ZP, E_META_BEEHIVE_FACING_XM
	>
{
	using Super = cYawRotator<
		cBlockEntityHandler, 0x03,
		E_META_BEEHIVE_FACING_ZM, E_META_BEEHIVE_FACING_XP, E_META_BEEHIVE_FACING_ZP, E_META_BEEHIVE_FACING_XM
	>;

public:

	using Super::Super;

private:

	virtual cItems ConvertToPickups(NIBBLETYPE a_Meta, const cItem * a_Tool) const override
	{
		if ((a_Meta & E_META_BEEHIVE_NEST) == 0)
		{
			return cItem(E_BLOCK_BEEHIVE);
		}
		if (ToolHasSilkTouch(a_Tool))
		{
			return cItem(E_ITEM_BEE_NEST);
		}
		return {};
	}

	virtual ColourID GetMapBaseColourID(NIBBLETYPE a_Meta) const override
	{
		return ((a_Meta & E_META_BEEHIVE_NEST) != 0) ? 18 : 2;
	}
};
