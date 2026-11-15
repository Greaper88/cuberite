#include "Globals.h"

#include "CampfireEntity.h"
#include "BlockInfo.h"
#include "Entities/Player.h"
#include "World.h"





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
	m_ImportedItems = Campfire.m_ImportedItems;
	if (const auto Saved = Campfire.GetPalettedStorageData(); Saved != nullptr)
	{
		SetPalettedStorageData(std::make_unique<cPalettedEntityData>(*Saved));
	}
}





cItems cCampfireEntity::ConvertToPickups() const
{
	return m_ImportedItems;
}





void cCampfireEntity::OnAddToWorld(cWorld & a_World, cChunk & a_Chunk)
{
	Super::OnAddToWorld(a_World, a_Chunk);
	UpdateBlockState();
}





bool cCampfireEntity::UsedBy(cPlayer * a_Player)
{
	const auto Item = a_Player->GetEquippedItem().m_ItemType;
	const bool IsLit = (m_BlockMeta & E_META_CAMPFIRE_LIT) != 0;
	if ((Item == E_ITEM_WATER_BUCKET) && !m_Waterlogged && (m_World->GetDimension() != dimNether))
	{
		SetWaterlogged(true);
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
	return false;
}





void cCampfireEntity::UpdateBlockState()
{
	m_World->UpdateNamedBlock(m_Pos, E_BLOCK_CAMPFIRE, GetBlockState());
}
