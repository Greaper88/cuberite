#include "Globals.h"
#include "PalettedStateBlockEntities.h"

#include "../BlockEntities/BannerEntity.h"
#include "../BlockEntities/BedEntity.h"
#include "../BlockEntities/BeehiveEntity.h"
#include "../BlockEntities/CampfireEntity.h"
#include "WorldFormat.h"
#include "../BlockEntities/FlowerPotEntity.h"
#include "../BlockEntities/MobHeadEntity.h"
#include "../BlockEntities/NoteEntity.h"
#include "../BlockType.h"
#include "../Protocol/Palettes/NamedBlockPalette.h"
#include "../World.h"





namespace
{
	struct cPottedPlant
	{
		std::string_view Name;
		BLOCKTYPE Block;
		NIBBLETYPE Meta;
	};

	constexpr cPottedPlant PottedPlants[] =
	{
		{"minecraft:potted_oak_sapling", E_BLOCK_SAPLING, E_META_SAPLING_APPLE},
		{"minecraft:potted_spruce_sapling", E_BLOCK_SAPLING, E_META_SAPLING_CONIFER},
		{"minecraft:potted_birch_sapling", E_BLOCK_SAPLING, E_META_SAPLING_BIRCH},
		{"minecraft:potted_jungle_sapling", E_BLOCK_SAPLING, E_META_SAPLING_JUNGLE},
		{"minecraft:potted_acacia_sapling", E_BLOCK_SAPLING, E_META_SAPLING_ACACIA},
		{"minecraft:potted_dark_oak_sapling", E_BLOCK_SAPLING, E_META_SAPLING_DARK_OAK},
		{"minecraft:potted_fern", E_BLOCK_TALL_GRASS, E_META_TALL_GRASS_FERN},
		{"minecraft:potted_dandelion", E_BLOCK_DANDELION, 0},
		{"minecraft:potted_poppy", E_BLOCK_FLOWER, E_META_FLOWER_POPPY},
		{"minecraft:potted_blue_orchid", E_BLOCK_FLOWER, E_META_FLOWER_BLUE_ORCHID},
		{"minecraft:potted_allium", E_BLOCK_FLOWER, E_META_FLOWER_ALLIUM},
		{"minecraft:potted_azure_bluet", E_BLOCK_FLOWER, 3},
		{"minecraft:potted_red_tulip", E_BLOCK_FLOWER, E_META_FLOWER_RED_TULIP},
		{"minecraft:potted_orange_tulip", E_BLOCK_FLOWER, E_META_FLOWER_ORANGE_TULIP},
		{"minecraft:potted_white_tulip", E_BLOCK_FLOWER, E_META_FLOWER_WHITE_TULIP},
		{"minecraft:potted_pink_tulip", E_BLOCK_FLOWER, E_META_FLOWER_PINK_TULIP},
		{"minecraft:potted_oxeye_daisy", E_BLOCK_FLOWER, E_META_FLOWER_OXEYE_DAISY},
		{"minecraft:potted_red_mushroom", E_BLOCK_RED_MUSHROOM, 0},
		{"minecraft:potted_brown_mushroom", E_BLOCK_BROWN_MUSHROOM, 0},
		{"minecraft:potted_dead_bush", E_BLOCK_DEAD_BUSH, 0},
		{"minecraft:potted_cactus", E_BLOCK_CACTUS, 0},
	};

	constexpr std::array<std::string_view, 16> Colors
	{
		"white", "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray",
		"light_gray", "cyan", "purple", "blue", "brown", "green", "red", "black",
	};

	struct cHeadNames
	{
		std::string_view Standing;
		std::string_view Wall;
	};
	constexpr std::array<cHeadNames, 6> Heads
	{
		{
			{"minecraft:skeleton_skull", "minecraft:skeleton_wall_skull"},
			{"minecraft:wither_skeleton_skull", "minecraft:wither_skeleton_wall_skull"},
			{"minecraft:zombie_head", "minecraft:zombie_wall_head"},
			{"minecraft:player_head", "minecraft:player_wall_head"},
			{"minecraft:creeper_head", "minecraft:creeper_wall_head"},
			{"minecraft:dragon_head", "minecraft:dragon_wall_head"},
		}
	};

	std::optional<unsigned char> StateNumber(const cNamedBlockState & a_State, const AString & a_Name, unsigned int a_Maximum)
	{
		const auto Field = a_State.Properties.find(a_Name);
		unsigned int Value;
		if ((Field == a_State.Properties.end()) || !StringToInteger(Field->second, Value) || (Value > a_Maximum))
		{
			return std::nullopt;
		}
		return static_cast<unsigned char>(Value);
	}

	std::optional<size_t> ColorOf(std::string_view a_Name, std::string_view a_Suffix)
	{
		constexpr std::string_view Minecraft = "minecraft:";
		if (
			(a_Name.size() < Minecraft.size() + a_Suffix.size()) ||
			(a_Name.substr(0, Minecraft.size()) != Minecraft) ||
			(a_Name.substr(a_Name.size() - a_Suffix.size()) != a_Suffix)
		)
		{
			return std::nullopt;
		}
		const auto Color = a_Name.substr(Minecraft.size(), a_Name.size() - Minecraft.size() - a_Suffix.size());
		const auto Found = std::find(Colors.begin(), Colors.end(), Color);
		if (Found == Colors.end())
		{
			return std::nullopt;
		}
		return static_cast<size_t>(Found - Colors.begin());
	}

	const cNamedBlockState & BaseState(cBlockEntity & a_Entity, const cNamedBlockState * a_Original, int a_DataVersion)
	{
		if (a_Original != nullptr)
		{
			return *a_Original;
		}
		const auto World = a_Entity.GetWorld();
		const auto Meta = (World == nullptr) ? NIBBLETYPE{0} : World->GetBlockMeta(a_Entity.GetPos());
		return NamedBlockPalette::FromLegacy(a_DataVersion, a_Entity.GetBlockType(), Meta);
	}
}





OwnedBlockEntity PalettedStateBlockEntities::FromState(
	const cNamedBlockState & a_State, BLOCKTYPE a_Block, NIBBLETYPE a_Meta, Vector3i a_Pos, cWorld * a_World
)
{
	if ((a_Block == E_BLOCK_NOTE_BLOCK) && (a_State.Name == "minecraft:note_block"))
	{
		auto Entity = std::make_unique<cNoteEntity>(a_Block, a_Meta, a_Pos, a_World);
		Entity->SetNote(StateNumber(a_State, "note", 24).value_or(0));
		return Entity;
	}
	if (
		(a_Block == E_BLOCK_BEEHIVE) &&
		((a_State.Name == "minecraft:beehive") || (a_State.Name == "minecraft:bee_nest"))
	)
	{
		auto Entity = std::make_unique<cBeehiveEntity>(a_Block, a_Meta, a_Pos, a_World);
		ApplyState(*Entity, a_State);
		return Entity;
	}
	if ((a_Block == E_BLOCK_CAMPFIRE) && (a_State.Name == "minecraft:campfire"))
	{
		auto Entity = std::make_unique<cCampfireEntity>(a_Block, a_Meta, a_Pos, a_World);
		ApplyState(*Entity, a_State);
		return Entity;
	}
	if (a_Block != E_BLOCK_FLOWER_POT)
	{
		return nullptr;
	}

	auto Entity = std::make_unique<cFlowerPotEntity>(a_Block, a_Meta, a_Pos, a_World);
	if (a_State.Name == "minecraft:flower_pot")
	{
		return Entity;
	}
	const auto Plant = std::find_if(std::begin(PottedPlants), std::end(PottedPlants), [&](const cPottedPlant & a_Plant)
	{
		return a_Plant.Name == a_State.Name;
	});
	if (Plant == std::end(PottedPlants))
	{
		return nullptr;
	}
	Entity->SetItem(cItem(Plant->Block, 1, Plant->Meta));
	return Entity;
}





void PalettedStateBlockEntities::ApplyState(cBlockEntity & a_Entity, const cNamedBlockState & a_State)
{
	switch (a_Entity.GetBlockType())
	{
		case E_BLOCK_BEEHIVE:
		{
			static_cast<cBeehiveEntity &>(a_Entity).SetHoneyLevel(StateNumber(a_State, "honey_level", 5).value_or(0));
			break;
		}
		case E_BLOCK_CAMPFIRE:
		{
			const auto Waterlogged = a_State.Properties.find("waterlogged");
			static_cast<cCampfireEntity &>(a_Entity).SetWaterlogged(
				(Waterlogged != a_State.Properties.end()) && (Waterlogged->second == "true")
			);
			break;
		}
		case E_BLOCK_BED:
		{
			if (const auto Color = ColorOf(a_State.Name, "_bed"); Color.has_value())
			{
				static_cast<cBedEntity &>(a_Entity).SetColor(static_cast<short>(*Color));
			}
			break;
		}
		case E_BLOCK_STANDING_BANNER:
		case E_BLOCK_WALL_BANNER:
		{
			const auto Suffix = (a_Entity.GetBlockType() == E_BLOCK_WALL_BANNER) ? "_wall_banner" : "_banner";
			if (const auto Color = ColorOf(a_State.Name, Suffix); Color.has_value())
			{
				// Legacy banners use dye damage, in reverse order from wool and bed colors.
				static_cast<cBannerEntity &>(a_Entity).SetBaseColor(static_cast<unsigned char>(Colors.size() - 1 - *Color));
			}
			break;
		}
		case E_BLOCK_HEAD:
		{
			auto & Head = static_cast<cMobHeadEntity &>(a_Entity);
			for (size_t Index = 0; Index < Heads.size(); ++Index)
			{
				if ((Heads[Index].Standing == a_State.Name) || (Heads[Index].Wall == a_State.Name))
				{
					Head.SetType(static_cast<eMobHeadType>(Index));
					break;
				}
			}
			if (const auto Rotation = StateNumber(a_State, "rotation", 15); Rotation.has_value())
			{
				Head.SetRotation(static_cast<eMobHeadRotation>(*Rotation));
			}
			break;
		}
		default:
		{
			break;
		}
	}
}





std::optional<cNamedBlockState> PalettedStateBlockEntities::ToState(
	cBlockEntity & a_Entity, const cNamedBlockState * a_Original, int a_DataVersion
)
{
	switch (a_Entity.GetBlockType())
	{
		case E_BLOCK_BEEHIVE:
		{
			if (a_DataVersion < JavaBeeDataVersion)
			{
				return std::nullopt;
			}
			auto State = BaseState(a_Entity, a_Original, a_DataVersion);
			State.Properties.insert_or_assign("honey_level", std::to_string(static_cast<cBeehiveEntity &>(a_Entity).GetHoneyLevel()));
			return State;
		}
		case E_BLOCK_CAMPFIRE:
		{
			if (a_DataVersion < JavaCampfireDataVersion)
			{
				return std::nullopt;
			}
			return static_cast<cCampfireEntity &>(a_Entity).GetBlockState();
		}
		case E_BLOCK_NOTE_BLOCK:
		{
			auto State = BaseState(a_Entity, a_Original, a_DataVersion);
			State.Properties.insert_or_assign("note", std::to_string(static_cast<cNoteEntity &>(a_Entity).GetNote()));
			return State;
		}
		case E_BLOCK_FLOWER_POT:
		{
			const auto Item = static_cast<const cFlowerPotEntity &>(a_Entity).GetItem();
			if (Item.IsEmpty())
			{
				return cNamedBlockState{"minecraft:flower_pot", {}};
			}
			const auto Plant = std::find_if(std::begin(PottedPlants), std::end(PottedPlants), [&](const cPottedPlant & a_Plant)
			{
				return (a_Plant.Block == Item.m_ItemType) && (a_Plant.Meta == Item.m_ItemDamage);
			});
			if (Plant == std::end(PottedPlants))
			{
				throw std::runtime_error("Flower pot contents have no paletted Anvil representation");
			}
			return cNamedBlockState{AString(Plant->Name), {}};
		}
		case E_BLOCK_BED:
		case E_BLOCK_STANDING_BANNER:
		case E_BLOCK_WALL_BANNER:
		{
			const auto Block = a_Entity.GetBlockType();
			const bool IsBed = (Block == E_BLOCK_BED);
			const auto Color = IsBed ?
				static_cast<size_t>(static_cast<const cBedEntity &>(a_Entity).GetColor()) :
				(Colors.size() - 1 - static_cast<const cBannerEntity &>(a_Entity).GetBaseColor());
			if (Color >= Colors.size())
			{
				throw std::runtime_error("Block entity's dye color has no paletted Anvil representation");
			}
			const auto Suffix = IsBed ? "_bed" : ((Block == E_BLOCK_WALL_BANNER) ? "_wall_banner" : "_banner");
			auto State = BaseState(a_Entity, a_Original, a_DataVersion);
			State.Name = fmt::format(FMT_STRING("minecraft:{}{}"), Colors[Color], Suffix);
			return State;
		}
		case E_BLOCK_HEAD:
		{
			const auto & Head = static_cast<const cMobHeadEntity &>(a_Entity);
			const auto Type = static_cast<size_t>(Head.GetType());
			if (Type >= Heads.size())
			{
				throw std::runtime_error("Head type has no paletted Anvil representation");
			}
			auto State = BaseState(a_Entity, a_Original, a_DataVersion);
			if (State.Properties.find("facing") != State.Properties.end())
			{
				State.Name = Heads[Type].Wall;
			}
			else
			{
				const auto Rotation = static_cast<unsigned int>(Head.GetRotation());
				if (Rotation > 15)
				{
					throw std::runtime_error("Head rotation has no paletted Anvil representation");
				}
				State.Name = Heads[Type].Standing;
				State.Properties.insert_or_assign("rotation", std::to_string(Rotation));
			}
			return State;
		}
		default:
		{
			return std::nullopt;
		}
	}
}
