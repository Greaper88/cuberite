#pragma once

#include "ItemHandler.h"
#include "Entities/Player.h"





class cItemHoneyBottleHandler final :
	public cItemHandler
{
public:

	using cItemHandler::cItemHandler;

private:

	virtual bool IsDrinkable(short a_ItemDamage) const override
	{
		return true;
	}

	virtual FoodInfo GetFoodInfo(const cItem * a_Item) const override
	{
		return FoodInfo(6, 1.2f);
	}

	virtual bool EatItem(cPlayer * a_Player, cItem * a_Item) const override
	{
		cItemHandler::EatItem(a_Player, a_Item);
		a_Player->RemoveEntityEffect(cEntityEffect::effPoison);
		if (!a_Player->IsGameModeCreative())
		{
			a_Player->ReplaceOneEquippedItemTossRest(cItem(E_ITEM_GLASS_BOTTLE));
		}
		return true;
	}

	virtual char GetMaxStackSize() const override
	{
		return 16;
	}
};
