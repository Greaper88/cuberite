#include "Globals.h"
#include "Protocol/LegacyBlockFallbacks.h"
#include "BlockType.h"
#include "Item.h"
#include "WorldStorage/FastNBT.h"





namespace
{
	using Version = cProtocol::Version;
	using State = LegacyBlockFallbacks::cState;

	constexpr char LegacyItemTag[] = "CuberiteLegacyBlock";

	struct cPackBlock
	{
		BLOCKTYPE Block;
		Version FirstNative;
		NIBBLETYPE CarrierMeta;
		State Item;
	};

	constexpr cPackBlock PackBlocks[] =
	{
		#define LEGACY_PACK_BLOCK(Block, Protocol, Meta, Blockstate, Model, Texture, ItemType, ItemMeta) \
			{ Block, static_cast<Version>(Protocol), Meta, { ItemType, ItemMeta } },
		#include "Protocol/LegacyPackBlocks.inc"
		#undef LEGACY_PACK_BLOCK
	};





	/** Finds the shared pack rule for this block, if one exists. */
	const cPackBlock * FindPackBlock(BLOCKTYPE a_Type)
	{
		const auto * End = std::end(PackBlocks);
		const auto * Entry = std::find_if(std::begin(PackBlocks), End, [a_Type](const cPackBlock & a_Entry)
		{
			return a_Entry.Block == a_Type;
		});
		return (Entry == End) ? nullptr : Entry;
	}





	/** Returns the first supported version with a native ID for a block without a pack rule. */
	Version FirstNativeVersion(BLOCKTYPE a_Type)
	{
		if ((a_Type >= E_BLOCK_WHITE_GLAZED_TERRACOTTA) && (a_Type <= E_BLOCK_CONCRETE_POWDER))
		{
			return Version::v1_12;
		}
		if ((a_Type >= E_BLOCK_OBSERVER) && (a_Type <= E_BLOCK_BLACK_SHULKER_BOX))
		{
			return Version::v1_11_0;
		}
		if ((a_Type >= E_BLOCK_MAGMA) && (a_Type <= E_BLOCK_STRUCTURE_VOID))
		{
			return Version::v1_10_0;
		}
		if (((a_Type >= E_BLOCK_END_ROD) && (a_Type <= E_BLOCK_FROSTED_ICE)) || (a_Type == E_BLOCK_STRUCTURE_BLOCK))
		{
			return Version::v1_9_0;
		}
		return Version::v1_8_0;
	}





	/** Selects the substitute for a block whose ID is unavailable in the client's palette. */
	State SubstituteBlock(BLOCKTYPE a_Type, NIBBLETYPE a_Meta)
	{
		switch (a_Type)
		{
			case E_BLOCK_END_ROD:                 return { E_BLOCK_TORCH, E_META_TORCH_FLOOR };
			case E_BLOCK_CHORUS_PLANT:            return { E_BLOCK_FENCE, 0 };
			case E_BLOCK_CHORUS_FLOWER:           return { E_BLOCK_STAINED_CLAY, E_META_STAINED_CLAY_PURPLE };
			case E_BLOCK_PURPUR_PILLAR:           return { E_BLOCK_QUARTZ_BLOCK, E_META_QUARTZ_PILLAR };
			case E_BLOCK_PURPUR_STAIRS:           return { E_BLOCK_QUARTZ_STAIRS, a_Meta };
			case E_BLOCK_PURPUR_DOUBLE_SLAB:      return { E_BLOCK_DOUBLE_STONE_SLAB, E_META_DOUBLE_STONE_SLAB_QUARTZ };
			case E_BLOCK_PURPUR_SLAB:             return { E_BLOCK_STONE_SLAB, static_cast<NIBBLETYPE>(E_META_STONE_SLAB_QUARTZ | (a_Meta & 8)) };
			case E_BLOCK_BEETROOTS:               return { E_BLOCK_CROPS, static_cast<NIBBLETYPE>(std::min(7, a_Meta * 2)) };
			case E_BLOCK_GRASS_PATH:              return { E_BLOCK_DIRT, E_META_DIRT_COARSE };
			case E_BLOCK_END_GATEWAY:             return { E_BLOCK_END_PORTAL, 0 };
			case E_BLOCK_REPEATING_COMMAND_BLOCK:
			case E_BLOCK_CHAIN_COMMAND_BLOCK:
			case E_BLOCK_STRUCTURE_BLOCK:        return { E_BLOCK_COMMAND_BLOCK, 0 };
			case E_BLOCK_FROSTED_ICE:             return { E_BLOCK_ICE, 0 };
			case E_BLOCK_BONE_BLOCK:              return { E_BLOCK_QUARTZ_BLOCK, E_META_QUARTZ_PILLAR };
			case E_BLOCK_STRUCTURE_VOID:         return { E_BLOCK_AIR, 0 };
			case E_BLOCK_OBSERVER:               return { E_BLOCK_STONE, 0 };
			case E_BLOCK_CONCRETE:               return { E_BLOCK_STAINED_CLAY, a_Meta };
			case E_BLOCK_CONCRETE_POWDER:        return { E_BLOCK_WOOL, a_Meta };
		}

		if ((a_Type >= E_BLOCK_WHITE_SHULKER_BOX) && (a_Type <= E_BLOCK_BLACK_SHULKER_BOX))
		{
			return { E_BLOCK_STAINED_CLAY, static_cast<NIBBLETYPE>(a_Type - E_BLOCK_WHITE_SHULKER_BOX) };
		}
		ASSERT((a_Type >= E_BLOCK_WHITE_GLAZED_TERRACOTTA) && (a_Type <= E_BLOCK_BLACK_GLAZED_TERRACOTTA));
		return { E_BLOCK_STAINED_CLAY, static_cast<NIBBLETYPE>(a_Type - E_BLOCK_WHITE_GLAZED_TERRACOTTA) };
	}
}





LegacyBlockFallbacks::cState LegacyBlockFallbacks::ForBlock(cProtocol::Version a_Version, BLOCKTYPE a_Type, NIBBLETYPE a_Meta)
{
	a_Meta &= 0x0f;
	if (a_Version >= Version::v1_12)
	{
		return { a_Type, a_Meta };
	}

	if ((a_Type == E_BLOCK_DOUBLE_STONE_SLAB) && ((a_Meta & 8) != 0))
	{
		for (const auto & Entry : PackBlocks)
		{
			if ((a_Meta == Entry.CarrierMeta) && (a_Version < Entry.FirstNative))
			{
				// A real slab must keep its vanilla appearance when the client applies the pack.
				return { a_Type, static_cast<NIBBLETYPE>(a_Meta & 7) };
			}
		}
	}
	if (a_Type < E_BLOCK_END_ROD)
	{
		return { a_Type, a_Meta };
	}

	if (const auto * Entry = FindPackBlock(a_Type); Entry != nullptr)
	{
		return (a_Version < Entry->FirstNative) ? cState{ E_BLOCK_DOUBLE_STONE_SLAB, Entry->CarrierMeta } : cState{ a_Type, a_Meta };
	}
	return (a_Version < FirstNativeVersion(a_Type)) ? SubstituteBlock(a_Type, a_Meta) : cState{ a_Type, a_Meta };
}





Int32 LegacyBlockFallbacks::ForEffect(cProtocol::Version a_Version, Int32 a_Data)
{
	// Effects use 12 low bits for the block ID and the following 4 bits for metadata.
	const auto Type = a_Data & 0xfff;
	if ((a_Version >= Version::v1_12) || (Type > E_BLOCK_STRUCTURE_BLOCK) || (a_Data < 0) || (a_Data > 0xffff))
	{
		return a_Data;
	}
	const auto State = ForBlock(a_Version, static_cast<BLOCKTYPE>(Type), static_cast<NIBBLETYPE>(a_Data >> 12));
	return State.Type | (State.Meta << 12);
}





std::pair<short, short> LegacyBlockFallbacks::ForItem(cProtocol::Version a_Version, short a_Type, short a_Damage)
{
	const std::pair<short, short> Native{ a_Type, a_Damage };
	if ((a_Type < E_BLOCK_END_ROD) || (a_Type > E_BLOCK_STRUCTURE_BLOCK) || (a_Version >= Version::v1_12))
	{
		return Native;
	}

	const auto BlockType = static_cast<BLOCKTYPE>(a_Type);
	if (const auto * Entry = FindPackBlock(BlockType); Entry != nullptr)
	{
		return (a_Version < Entry->FirstNative) ? std::pair<short, short>{ Entry->Item.Type, Entry->Item.Meta } : Native;
	}
	if (a_Version >= FirstNativeVersion(BlockType))
	{
		return Native;
	}

	// Some world states either have no item representation or use world-only metadata.
	switch (a_Type)
	{
		case E_BLOCK_END_ROD:            return { E_BLOCK_TORCH, 0 };
		case E_BLOCK_PURPUR_DOUBLE_SLAB: return { E_BLOCK_QUARTZ_BLOCK, 0 };
		case E_BLOCK_PURPUR_SLAB:        return { E_BLOCK_STONE_SLAB, E_META_STONE_SLAB_QUARTZ };
		case E_BLOCK_BEETROOTS:          return { E_ITEM_SEEDS, 0 };
		case E_BLOCK_END_GATEWAY:        return { E_BLOCK_OBSIDIAN, 0 };
		case E_BLOCK_STRUCTURE_VOID:    return { E_BLOCK_BARRIER, 0 };
	}

	const auto State = SubstituteBlock(BlockType, static_cast<NIBBLETYPE>(a_Damage & 0x0f));
	return { State.Type, State.Meta };
}





void LegacyBlockFallbacks::WriteItemIdentity(cFastNBTWriter & a_Writer, const cItem & a_Item)
{
	a_Writer.BeginCompound(LegacyItemTag);
	a_Writer.AddShort("Type", a_Item.m_ItemType);
	a_Writer.AddShort("Damage", a_Item.m_ItemDamage);
	a_Writer.EndCompound();
}





void LegacyBlockFallbacks::RestoreItemIdentity(cProtocol::Version a_Version, cItem & a_Item, const cParsedNBT & a_Nbt)
{
	if ((a_Version >= Version::v1_12) || a_Item.IsEmpty())
	{
		return;
	}
	const int Identity = a_Nbt.FindChildByName(a_Nbt.GetRoot(), LegacyItemTag);
	if ((Identity < 0) || (a_Nbt.GetType(Identity) != TAG_Compound))
	{
		return;
	}
	const int TypeTag = a_Nbt.FindChildByName(Identity, "Type");
	const int DamageTag = a_Nbt.FindChildByName(Identity, "Damage");
	if ((TypeTag < 0) || (DamageTag < 0) || (a_Nbt.GetType(TypeTag) != TAG_Short) || (a_Nbt.GetType(DamageTag) != TAG_Short))
	{
		return;
	}
	const auto Type = a_Nbt.GetShort(TypeTag);
	const auto Damage = a_Nbt.GetShort(DamageTag);
	const auto [WireType, WireDamage] = ForItem(a_Version, Type, Damage);
	if ((Type != WireType) && (WireType == a_Item.m_ItemType) && (WireDamage == a_Item.m_ItemDamage))
	{
		a_Item.m_ItemType = Type;
		a_Item.m_ItemDamage = Damage;
	}
}
