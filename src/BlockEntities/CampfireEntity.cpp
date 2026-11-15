#include "Globals.h"

#include "CampfireEntity.h"
#include "BlockInfo.h"
#include "Chunk.h"
#include "ClientHandle.h"
#include "Entities/Player.h"
#include "World.h"





namespace
{
	struct cCampfireRecipe
	{
		short Ingredient;
		short IngredientDamage;
		short Result;
		short ResultDamage;
	};

	// Java's campfire recipes are a food-only list independent of the furnace recipes.
	constexpr int CookingTime = 600;
	constexpr cCampfireRecipe CampfireRecipes[] =
	{
		{ E_ITEM_RAW_BEEF, 0, E_ITEM_STEAK, 0 },
		{ E_ITEM_RAW_PORKCHOP, 0, E_ITEM_COOKED_PORKCHOP, 0 },
		{ E_ITEM_RAW_CHICKEN, 0, E_ITEM_COOKED_CHICKEN, 0 },
		{ E_ITEM_RAW_RABBIT, 0, E_ITEM_COOKED_RABBIT, 0 },
		{ E_ITEM_RAW_MUTTON, 0, E_ITEM_COOKED_MUTTON, 0 },
		{ E_ITEM_RAW_FISH, E_META_RAW_FISH_FISH, E_ITEM_COOKED_FISH, E_META_COOKED_FISH_FISH },
		{ E_ITEM_RAW_FISH, E_META_RAW_FISH_SALMON, E_ITEM_COOKED_FISH, E_META_COOKED_FISH_SALMON },
		{ E_ITEM_POTATO, 0, E_ITEM_BAKED_POTATO, 0 },
		{ E_ITEM_KELP, 0, E_ITEM_DRIED_KELP, 0 },
	};

	const cCampfireRecipe * FindCampfireRecipe(const cItem & a_Item)
	{
		const auto Recipe = std::find_if(std::begin(CampfireRecipes), std::end(CampfireRecipes), [&a_Item](const auto & a_Recipe)
		{
			return (a_Item.m_ItemType == a_Recipe.Ingredient) && (a_Item.m_ItemDamage == a_Recipe.IngredientDamage);
		});
		return (Recipe == std::end(CampfireRecipes)) ? nullptr : Recipe;
	}
}





cCampfireEntity::cCampfireEntity(BLOCKTYPE a_BlockType, NIBBLETYPE a_BlockMeta, Vector3i a_Pos, cWorld * a_World) :
	Super(a_BlockType, a_BlockMeta, a_Pos, a_World)
{
	ASSERT(a_BlockType == E_BLOCK_CAMPFIRE);
}





void cCampfireEntity::SetWaterlogged(bool a_Waterlogged)
{
	if (m_Waterlogged == a_Waterlogged)
	{
		return;
	}
	m_Waterlogged = a_Waterlogged;
	if (m_World != nullptr)
	{
		if (a_Waterlogged && ((m_BlockMeta & E_META_CAMPFIRE_LIT) != 0))
		{
			m_World->SetBlockMeta(m_Pos, static_cast<NIBBLETYPE>(m_BlockMeta & ~E_META_CAMPFIRE_LIT));
		}
		else
		{
			UpdateBlockState();
		}
	}
}





void cCampfireEntity::SetBlockMeta(NIBBLETYPE a_Meta)
{
	m_BlockMeta = a_Meta;
	UpdateBlockState();
}





cNamedBlockState cCampfireEntity::GetBlockState() const
{
	static constexpr const char * Facings[] = { "north", "south", "west", "east" };
	return
	{
		"minecraft:campfire",
		{
			{"facing", Facings[m_BlockMeta & 3]},
			{"lit", ((m_BlockMeta & E_META_CAMPFIRE_LIT) != 0) ? "true" : "false"},
			{"signal_fire", ((m_BlockMeta & E_META_CAMPFIRE_SIGNAL) != 0) ? "true" : "false"},
			{"waterlogged", m_Waterlogged ? "true" : "false"},
		}
	};
}





bool cCampfireEntity::IsSmokey(const cWorld & a_World, Vector3i a_Position)
{
	const auto Lit = [](BLOCKTYPE a_Block, NIBBLETYPE a_Meta)
	{
		return (a_Block == E_BLOCK_CAMPFIRE) && ((a_Meta & E_META_CAMPFIRE_LIT) != 0);
	};
	for (int Below = 1; (Below <= 5) && (a_Position.y >= Below); ++Below)
	{
		const auto Pos = a_Position.addedY(-Below);
		BLOCKTYPE Block;
		NIBBLETYPE Meta;
		if (!a_World.GetBlockTypeMeta(Pos, Block, Meta))
		{
			return false;
		}
		if (Lit(Block, Meta))
		{
			return true;
		}

		const bool IsOpenTrapdoor =
			((Block == E_BLOCK_TRAPDOOR) || (Block == E_BLOCK_IRON_TRAPDOOR)) &&
			((Meta & 4) != 0);
		if (cBlockInfo::IsSolid(Block) && !IsOpenTrapdoor)
		{
			// Java also recognizes smoke from the campfire directly beneath the first obstruction.
			return (
				(Pos.y > 0) &&
				a_World.GetBlockTypeMeta(Pos.addedY(-1), Block, Meta) &&
				Lit(Block, Meta)
			);
		}
	}
	return false;
}





void cCampfireEntity::CopyFrom(const cBlockEntity & a_Src)
{
	Super::CopyFrom(a_Src);
	const auto & Campfire = static_cast<const cCampfireEntity &>(a_Src);
	m_Waterlogged = Campfire.m_Waterlogged;
	m_CookingSlots = Campfire.m_CookingSlots;
	if (const auto Saved = Campfire.GetPalettedStorageData(); Saved != nullptr)
	{
		SetPalettedStorageData(std::make_unique<cPalettedEntityData>(*Saved));
	}
}





cItems cCampfireEntity::ConvertToPickups() const
{
	cItems Items;
	for (const auto & Slot : m_CookingSlots)
	{
		if (!Slot.Item.IsEmpty())
		{
			Items.Add(Slot.Item);
		}
	}
	return Items;
}





void cCampfireEntity::OnAddToWorld(cWorld & a_World, cChunk & a_Chunk)
{
	Super::OnAddToWorld(a_World, a_Chunk);
	UpdateBlockState();
}





void cCampfireEntity::SendTo(cClientHandle & a_Client)
{
	a_Client.SendUpdateBlockEntity(*this);
}





bool cCampfireEntity::Tick(std::chrono::milliseconds a_Dt, cChunk & a_Chunk)
{
	const bool IsLit = !m_Waterlogged && ((m_BlockMeta & E_META_CAMPFIRE_LIT) != 0);
	bool ChangedProgress = false;
	bool ChangedItems = false;
	for (auto & Slot : m_CookingSlots)
	{
		if (Slot.Item.IsEmpty())
		{
			continue;
		}
		if (!IsLit)
		{
			if (Slot.Progress > 0)
			{
				Slot.Progress = std::max(0, Slot.Progress - 2);
				ChangedProgress = true;
			}
			continue;
		}

		if (Slot.Total <= 0)
		{
			// Earlier saves may contain an item without a timer.
			Slot.Total = CookingTime;
			ChangedProgress = true;
		}
		if (Slot.Progress < Slot.Total)
		{
			++Slot.Progress;
			ChangedProgress = true;
		}
		if (Slot.Progress < Slot.Total)
		{
			continue;
		}

		const auto Recipe = FindCampfireRecipe(Slot.Item);
		const auto Result = (Recipe == nullptr) ? Slot.Item : cItem(Recipe->Result, Slot.Item.m_ItemCount, Recipe->ResultDamage);
		ChangedItems |= EjectSlot(Slot, Result);
	}
	if (ChangedItems)
	{
		a_Chunk.QueueBlockEntityUpdate(*this);
	}
	return ChangedProgress || ChangedItems;
}





bool cCampfireEntity::UsedBy(cPlayer * a_Player)
{
	const auto & Held = a_Player->GetEquippedItem();
	const auto Item = Held.m_ItemType;
	const bool IsLit = (m_BlockMeta & E_META_CAMPFIRE_LIT) != 0;
	if ((Item == E_ITEM_WATER_BUCKET) && !m_Waterlogged && (m_World->GetDimension() != dimNether))
	{
		SetWaterlogged(true);
		bool ChangedItems = false;
		for (auto & Slot : m_CookingSlots)
		{
			if (!Slot.Item.IsEmpty())
			{
				ChangedItems |= EjectSlot(Slot, Slot.Item);
			}
		}
		if (ChangedItems)
		{
			OnContentsChanged();
		}
		if (!a_Player->IsGameModeCreative())
		{
			a_Player->ReplaceOneEquippedItemTossRest(cItem(E_ITEM_BUCKET));
		}
		return true;
	}
	if ((Item == E_ITEM_BUCKET) && m_Waterlogged)
	{
		SetWaterlogged(false);
		if (!a_Player->IsGameModeCreative())
		{
			a_Player->ReplaceOneEquippedItemTossRest(cItem(E_ITEM_WATER_BUCKET));
		}
		return true;
	}

	if (IsLit && ItemCategory::IsShovel(Item))
	{
		m_World->SetBlockMeta(m_Pos, static_cast<NIBBLETYPE>(m_BlockMeta & ~E_META_CAMPFIRE_LIT));
		return true;
	}

	if (!IsLit && !m_Waterlogged && ((Item == E_ITEM_FLINT_AND_STEEL) || (Item == E_ITEM_FIRE_CHARGE)))
	{
		m_World->SetBlockMeta(m_Pos, static_cast<NIBBLETYPE>(m_BlockMeta | E_META_CAMPFIRE_LIT));
		if (!a_Player->IsGameModeCreative())
		{
			if (Item == E_ITEM_FLINT_AND_STEEL)
			{
				a_Player->UseEquippedItem();
			}
			else
			{
				a_Player->GetInventory().RemoveOneEquippedItem();
			}
		}
		return true;
	}

	if (!IsLit || m_Waterlogged || Held.IsEmpty() || (FindCampfireRecipe(Held) == nullptr))
	{
		return false;
	}
	const auto EmptySlot = std::find_if(m_CookingSlots.begin(), m_CookingSlots.end(), [](const auto & a_Slot)
	{
		return a_Slot.Item.IsEmpty();
	});
	if (EmptySlot == m_CookingSlots.end())
	{
		return false;
	}
	auto Ingredient = Held.CopyOne();
	if (!a_Player->IsGameModeCreative() && !a_Player->GetInventory().RemoveOneEquippedItem())
	{
		return false;
	}
	*EmptySlot = { std::move(Ingredient), 0, CookingTime };
	OnContentsChanged();
	return true;
}





bool cCampfireEntity::EjectSlot(cCookingSlot & a_Slot, const cItem & a_Stack)
{
	const auto DropPos = Vector3d(m_Pos) + Vector3d(0.5, 0.65, 0.5);
	if (m_World->SpawnItemPickup(DropPos, a_Stack, {0.0f, 1.0f, 0.0f}) == cEntity::INVALID_ID)
	{
		return false;
	}
	a_Slot = {};
	return true;
}





void cCampfireEntity::OnContentsChanged()
{
	m_World->DoWithChunkAt(m_Pos, [this](cChunk & a_Chunk)
	{
		a_Chunk.MarkDirty();
		a_Chunk.QueueBlockEntityUpdate(*this);
		return true;
	});
}





void cCampfireEntity::UpdateBlockState()
{
	m_World->UpdateNamedBlock(m_Pos, E_BLOCK_CAMPFIRE, GetBlockState());
}
