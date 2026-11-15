
// NBTChunkSerializer.cpp


#include "Globals.h"
#include "NBTChunkSerializer.h"
#include "BeeNBT.h"
#include "WorldFormat.h"
#include "../BlockEntities/BeehiveEntity.h"
#include "../Mobs/Bee.h"
#include "EnchantmentSerializer.h"
#include "NamespaceSerializer.h"
#include "PalettedItemNBT.h"
#include "../Protocol/Palettes/NamedBlockPalette.h"
#include "../JsonUtils.h"
#include "../ChunkDataCallback.h"
#include "../ItemGrid.h"
#include "../StringCompression.h"
#include "../UUID.h"
#include "FastNBT.h"

#include "../BlockEntities/BannerEntity.h"
#include "../BlockEntities/BeaconEntity.h"
#include "../BlockEntities/BedEntity.h"
#include "../BlockEntities/BrewingstandEntity.h"
#include "../BlockEntities/ChestEntity.h"
#include "../BlockEntities/CommandBlockEntity.h"
#include "../BlockEntities/DispenserEntity.h"
#include "../BlockEntities/DropperEntity.h"
#include "../BlockEntities/EnchantingTableEntity.h"
#include "../BlockEntities/EnderChestEntity.h"
#include "../BlockEntities/EndPortalEntity.h"
#include "../BlockEntities/FurnaceEntity.h"
#include "../BlockEntities/HopperEntity.h"
#include "../BlockEntities/JukeboxEntity.h"
#include "../BlockEntities/MobSpawnerEntity.h"
#include "../BlockEntities/NoteEntity.h"
#include "../BlockEntities/SignEntity.h"
#include "../BlockEntities/MobHeadEntity.h"
#include "../BlockEntities/FlowerPotEntity.h"

#include "../Entities/Entity.h"
#include "../Entities/EnderCrystal.h"
#include "../Entities/FallingBlock.h"
#include "../Entities/Boat.h"
#include "../Entities/Minecart.h"
#include "../Entities/Pickup.h"
#include "../Entities/ArrowEntity.h"
#include "../Entities/SplashPotionEntity.h"
#include "../Entities/TNTEntity.h"
#include "../Entities/ExpOrb.h"
#include "../Entities/HangingEntity.h"
#include "../Entities/ItemFrame.h"
#include "../Entities/LeashKnot.h"
#include "../Entities/Painting.h"

#include "../Mobs/IncludeAllMonsters.h"





namespace
{
	std::string_view NativeMobID(eMonsterType a_Type, int a_DataVersion)
	{
		switch (a_Type)
		{
			case mtZombiePigman:
			{
				return "zombie_pigman";
			}
			case mtBee:
			{
				return (a_DataVersion >= JavaBeeDataVersion) ? "bee" : std::string_view{};
			}
			case mtCat:
			case mtFox:
			case mtPanda:
			case mtPillager:
			case mtRavager:
			case mtTraderLlama:
			case mtWanderingTrader:
			{
				return (a_DataVersion >= 1952) ? NamespaceSerializer::From(a_Type) : std::string_view{};
			}
			case mtHoglin:
			case mtPiglin:
			case mtPiglinBrute:
			case mtStrider:
			case mtZoglin:
			case mtInvalidType:
			{
				return {};
			}
			default:
			{
				return NamespaceSerializer::From(a_Type);
			}
		}
	}

	bool UsesNativeCatRecord(const cOcelot & a_Ocelot, int a_DataVersion)
	{
		return (a_DataVersion >= 1952) && (a_Ocelot.IsTame() || (a_Ocelot.GetOcelotType() != cOcelot::ctWildOcelot));
	}

	std::string_view NativeMobID(const cMonster & a_Monster, int a_DataVersion)
	{
		const auto Type = a_Monster.GetMobType();
		if ((Type == mtOcelot) && UsesNativeCatRecord(static_cast<const cOcelot &>(a_Monster), a_DataVersion))
		{
			return "cat";
		}
		if (Type == mtHorse)
		{
			// Cuberite represents the whole legacy horse family with one engine class.
			static constexpr std::array<std::string_view, 5> HorseIDs = {"horse", "donkey", "mule", "zombie_horse", "skeleton_horse"};
			const auto Variant = static_cast<size_t>(static_cast<const cHorse &>(a_Monster).GetHorseType());
			return (Variant < HorseIDs.size()) ? HorseIDs[Variant] : HorseIDs.front();
		}
		return NativeMobID(Type, a_DataVersion);
	}

	std::string_view NativeEntityID(std::string_view a_LegacyID)
	{
		static constexpr std::pair<std::string_view, std::string_view> IDs[] =
		{
			{"Arrow", "arrow"},
			{"Boat", "boat"},
			{"Egg", "egg"},
			{"EnderCrystal", "end_crystal"},
			{"FallingSand", "falling_block"},
			{"Fireball", "fireball"},
			{"Firework", "firework_rocket"},
			{"Item", "item"},
			{"ItemFrame", "item_frame"},
			{"LeashKnot", "leash_knot"},
			{"MinecartChest", "chest_minecart"},
			{"MinecartFurnace", "furnace_minecart"},
			{"MinecartHopper", "hopper_minecart"},
			{"MinecartRideable", "minecart"},
			{"MinecartTNT", "tnt_minecart"},
			{"Painting", "painting"},
			{"PrimedTnt", "tnt"},
			{"SmallFireball", "small_fireball"},
			{"Snowball", "snowball"},
			{"SplashPotion", "potion"},
			{"ThrownEnderpearl", "ender_pearl"},
			{"ThrownExpBottle", "experience_bottle"},
			{"WitherSkull", "wither_skull"},
			{"XPOrb", "experience_orb"},
		};
		const auto Found = std::find_if(std::begin(IDs), std::end(IDs), [a_LegacyID](const auto & a_Entry)
		{
			return a_Entry.first == a_LegacyID;
		});
		return (Found == std::end(IDs)) ? a_LegacyID : Found->second;
	}

	std::string_view NativeBlockEntityID(std::string_view a_LegacyID, BLOCKTYPE a_Block)
	{
		static constexpr std::pair<std::string_view, std::string_view> IDs[] =
		{
			{"Banner", "banner"},
			{"Beacon", "beacon"},
			{"Bed", "bed"},
			{"Beehive", "beehive"},
			{"Brewingstand", "brewing_stand"},
			{"Chest", "chest"},
			{"Control", "command_block"},
			{"Dropper", "dropper"},
			{"EnchantingTable", "enchanting_table"},
			{"EnderChest", "ender_chest"},
			{"EndPortal", "end_portal"},
			{"Furnace", "furnace"},
			{"Hopper", "hopper"},
			{"MobSpawner", "mob_spawner"},
			{"RecordPlayer", "jukebox"},
			{"Sign", "sign"},
			{"Skull", "skull"},
			{"Trap", "dispenser"},
		};
		if (a_Block == E_BLOCK_TRAPPED_CHEST)
		{
			return "trapped_chest";
		}
		const auto Found = std::find_if(std::begin(IDs), std::end(IDs), [a_LegacyID](const auto & a_Entry)
		{
			return a_Entry.first == a_LegacyID;
		});
		if (Found == std::end(IDs))
		{
			throw std::runtime_error(fmt::format(FMT_STRING("Missing native block entity ID for {}"), a_LegacyID));
		}
		return Found->second;
	}

	ContiguousByteBuffer UnwrapEntityRecord(const cFastNBTWriter & a_Wrapped)
	{
		const cParsedNBT Parsed(a_Wrapped.GetResult());
		ASSERT(Parsed.IsValid());
		const auto Compound = Parsed.GetFirstChild(0);
		if (Compound < 0)
		{
			return {};
		}
		ASSERT(Parsed.GetType(Compound) == TAG_Compound);
		ASSERT(Parsed.GetNextSibling(Compound) < 0);
		ContiguousByteBuffer Result{std::byte{TAG_Compound}, std::byte{}, std::byte{}};
		Result.append(Parsed.GetTagPayload(Compound));
		return Result;
	}
}





/** Collects and stores the chunk data via the cChunkDataCallback interface */
class SerializerCollector final :
	public cChunkDataCopyCollector
{
public:

	// The data collected from the chunk:
	UInt8 Biomes[cChunkDef::Width * cChunkDef::Width];
	int Heights[cChunkDef::Width * cChunkDef::Width];

	/** True if a tag has been opened in the callbacks and not yet closed. */
	bool mIsTagOpen;

	/** True if any Entity has already been received and processed. */
	bool mHasHadEntity;

	/** True if any BlockEntity has already been received and processed. */
	bool mHasHadBlockEntity;

	/** True if the chunk lighting is valid. */
	bool mIsLightValid;

	/** The NBT writer used to store the data. */
	cFastNBTWriter & mWriter;

	const int mDataVersion;





	explicit SerializerCollector(cFastNBTWriter & aWriter, int a_DataVersion = 0):
		mIsTagOpen(false),
		mHasHadEntity(false),
		mHasHadBlockEntity(false),
		mIsLightValid(false),
		mWriter(aWriter),
		mDataVersion(a_DataVersion)
	{
	}





	virtual void LightIsValid(bool a_IsLightValid) override
	{
		mIsLightValid = a_IsLightValid;
	}





	virtual void HeightMap(const cChunkDef::HeightMap & a_HeightMap) override
	{
		for (int RelZ = 0; RelZ < cChunkDef::Width; RelZ++)
		{
			for (int RelX = 0; RelX < cChunkDef::Width; RelX++)
			{
				Heights[RelX + RelZ * cChunkDef::Width] = cChunkDef::GetHeight(a_HeightMap, RelX, RelZ);
			}
		}
	}





	virtual void BiomeMap(const cChunkDef::BiomeMap & a_BiomeMap) override
	{
		for (size_t i = 0; i < ARRAYCOUNT(Biomes); i++)
		{
			if (a_BiomeMap[i] < 255)
			{
				// Normal MC biome, copy as-is:
				Biomes[i] = static_cast<Byte>(a_BiomeMap[i]);
			}
			else
			{
				// TODO: MCS-specific biome, need to map to some basic MC biome:
				ASSERT(!"Unimplemented MCS-specific biome");
				return;
			}
		}  // for i - mBiomeMap[]
	}





	virtual void Entity(cEntity * a_Entity) override
	{
		// Add entity into NBT:
		if (mIsTagOpen)
		{
			if (!mHasHadEntity)
			{
				mWriter.EndList();
				mWriter.BeginList("Entities", TAG_Compound);
			}
		}
		else
		{
			mWriter.BeginList("Entities", TAG_Compound);
		}
		mIsTagOpen = true;
		mHasHadEntity = true;

		AddEntityRecord(a_Entity);
	}





	void AddEntityRecord(cEntity * a_Entity)
	{
		if (a_Entity->IsMob())
		{
			const auto Type = static_cast<const cMonster &>(*a_Entity).GetMobType();
			if (
				(IsNative() && NativeMobID(Type, mDataVersion).empty()) ||
				((Type == mtBee) && (mDataVersion < JavaBeeDataVersion))
			)
			{
				return;
			}
		}
		switch (a_Entity->GetEntityType())
		{
			case cEntity::etBoat:         AddBoatEntity        (static_cast<cBoat *>            (a_Entity)); break;
			case cEntity::etEnderCrystal: AddEnderCrystalEntity(static_cast<cEnderCrystal *>    (a_Entity)); break;
			case cEntity::etFallingBlock: AddFallingBlockEntity(static_cast<cFallingBlock *>    (a_Entity)); break;
			case cEntity::etMinecart:     AddMinecartEntity    (static_cast<cMinecart *>        (a_Entity)); break;
			case cEntity::etMonster:      AddMonsterEntity     (static_cast<cMonster *>         (a_Entity)); break;
			case cEntity::etPickup:       AddPickupEntity      (static_cast<cPickup *>          (a_Entity)); break;
			case cEntity::etProjectile:   AddProjectileEntity  (static_cast<cProjectileEntity *>(a_Entity)); break;
			case cEntity::etTNT:          AddTNTEntity         (static_cast<cTNTEntity *>       (a_Entity)); break;
			case cEntity::etExpOrb:       AddExpOrbEntity      (static_cast<cExpOrb *>          (a_Entity)); break;
			case cEntity::etItemFrame:    AddItemFrameEntity   (static_cast<cItemFrame *>       (a_Entity)); break;
			case cEntity::etLeashKnot:    AddLeashKnotEntity   (static_cast<cLeashKnot *>       (a_Entity)); break;
			case cEntity::etPainting:     AddPaintingEntity    (static_cast<cPainting *>        (a_Entity)); break;
			case cEntity::etPlayer: return;  // Players aren't saved into the world
			case cEntity::etFloater: return;  // Floaters aren't saved either
			default:
			{
				ASSERT(!"Unhandled entity type is being saved");
				break;
			}
		}
	}





	virtual void BlockEntity(cBlockEntity * a_Entity) override
	{
		if (mIsTagOpen)
		{
			if (!mHasHadBlockEntity)
			{
				mWriter.EndList();
				mWriter.BeginList("TileEntities", TAG_Compound);
			}
		}
		else
		{
			mWriter.BeginList("TileEntities", TAG_Compound);
		}
		mIsTagOpen = true;

		AddBlockEntityRecord(a_Entity);
		mHasHadBlockEntity = true;
	}





	void AddBlockEntityRecord(cBlockEntity * a_Entity)
	{
		const auto Type = a_Entity->GetBlockType();
		if (IsNative() && ((Type == E_BLOCK_NOTE_BLOCK) || (Type == E_BLOCK_FLOWER_POT)))
		{
			return;  // These became palette states in Java 1.13.
		}

		switch (Type)
		{
			// Banners:
			case E_BLOCK_STANDING_BANNER:
			case E_BLOCK_WALL_BANNER:       AddBannerEntity         (static_cast<cBannerEntity *>         (a_Entity)); break;

			// Others:
			case E_BLOCK_BEEHIVE:
			{
				if (mDataVersion >= JavaBeeDataVersion)
				{
					mWriter.BeginCompound("");
					AddBasicTileEntity(a_Entity, "Beehive");
					BeeNBT::WriteHive(mWriter, static_cast<const cBeehiveEntity &>(*a_Entity));
					mWriter.EndCompound();
				}
				break;
			}
			case E_BLOCK_BEACON:            AddBeaconEntity         (static_cast<cBeaconEntity *>         (a_Entity)); break;
			case E_BLOCK_BED:               AddBedEntity            (static_cast<cBedEntity *>            (a_Entity)); break;
			case E_BLOCK_BREWING_STAND:     AddBrewingstandEntity   (static_cast<cBrewingstandEntity *>   (a_Entity)); break;
			case E_BLOCK_CHEST:             AddChestEntity          (static_cast<cChestEntity *>          (a_Entity), a_Entity->GetBlockType()); break;
			case E_BLOCK_COMMAND_BLOCK:     AddCommandBlockEntity   (static_cast<cCommandBlockEntity *>   (a_Entity)); break;
			case E_BLOCK_DISPENSER:         AddDispenserEntity      (static_cast<cDispenserEntity *>      (a_Entity)); break;
			case E_BLOCK_DROPPER:           AddDropperEntity        (static_cast<cDropperEntity *>        (a_Entity)); break;
			case E_BLOCK_ENCHANTMENT_TABLE: AddEnchantingTableEntity(static_cast<cEnchantingTableEntity *>(a_Entity)); break;
			case E_BLOCK_ENDER_CHEST:       AddEnderchestEntity     (static_cast<cEnderChestEntity *>     (a_Entity)); break;
			case E_BLOCK_END_PORTAL:        AddEndPortalEntity      (static_cast<cEndPortalEntity *>      (a_Entity)); break;
			case E_BLOCK_FLOWER_POT:        AddFlowerPotEntity      (static_cast<cFlowerPotEntity *>      (a_Entity)); break;
			case E_BLOCK_FURNACE:           AddFurnaceEntity        (static_cast<cFurnaceEntity *>        (a_Entity)); break;
			case E_BLOCK_HEAD:              AddMobHeadEntity        (static_cast<cMobHeadEntity *>        (a_Entity)); break;
			case E_BLOCK_HOPPER:            AddHopperEntity         (static_cast<cHopperEntity *>         (a_Entity)); break;
			case E_BLOCK_JUKEBOX:           AddJukeboxEntity        (static_cast<cJukeboxEntity *>        (a_Entity)); break;
			case E_BLOCK_LIT_FURNACE:       AddFurnaceEntity        (static_cast<cFurnaceEntity *>        (a_Entity)); break;
			case E_BLOCK_MOB_SPAWNER:       AddMobSpawnerEntity     (static_cast<cMobSpawnerEntity *>     (a_Entity)); break;
			case E_BLOCK_NOTE_BLOCK:        AddNoteEntity           (static_cast<cNoteEntity *>           (a_Entity)); break;
			case E_BLOCK_SIGN_POST:         AddSignEntity           (static_cast<cSignEntity *>           (a_Entity)); break;
			case E_BLOCK_TRAPPED_CHEST:     AddChestEntity          (static_cast<cChestEntity *>          (a_Entity), a_Entity->GetBlockType()); break;
			case E_BLOCK_WALLSIGN:          AddSignEntity           (static_cast<cSignEntity *>           (a_Entity)); break;
			default:
			{
				ASSERT(!"Unhandled block entity saved into Anvil");
			}
		}
	}





	void Finish(void)
	{
		if (mIsTagOpen)
		{
			mWriter.EndList();
		}

		// Check if "Entity" and "TileEntities" lists exists. MCEdit requires this.
		if (!mHasHadEntity)
		{
			mWriter.BeginList("Entities", TAG_Compound);
			mWriter.EndList();
		}
		if (!mHasHadBlockEntity)
		{
			mWriter.BeginList("TileEntities", TAG_Compound);
			mWriter.EndList();
		}
	}





	bool IsNative() const
	{
		return mDataVersion != 0;
	}





	void AddText(const AString & a_Tag, const AString & a_Text)
	{
		mWriter.AddString(a_Tag, IsNative() ? JsonUtils::SerializeSingleValueJsonObject("text", a_Text) : a_Text);
	}





	void AddNativeBlockState(const AString & a_Tag, BLOCKTYPE a_Type, NIBBLETYPE a_Meta)
	{
		const auto & State = NamedBlockPalette::FromLegacy(mDataVersion, a_Type, a_Meta);
		mWriter.BeginCompound(a_Tag);
		mWriter.AddString("Name", State.Name);
		if (!State.Properties.empty())
		{
			mWriter.BeginCompound("Properties");
			for (const auto & [Name, Value] : State.Properties)
			{
				mWriter.AddString(Name, Value);
			}
			mWriter.EndCompound();
		}
		mWriter.EndCompound();
	}





	/** Writes an item into the writer.
	If aSlot >= 0, adds the Slot tag.
	The compound is named as requested (empty name by default). */
	void AddItem(const cItem & a_Item, int a_Slot, const AString & a_CompoundName = AString())
	{
		if (IsNative())
		{
			PalettedItemNBT::Write(mWriter, a_Item, a_Slot, a_CompoundName, mDataVersion);
			return;
		}

		mWriter.BeginCompound(a_CompoundName);
		mWriter.AddShort("id",         static_cast<Int16>(a_Item.m_ItemType));
		mWriter.AddShort("Damage",     static_cast<Int16>((a_Item.m_ItemDamage)));
		mWriter.AddByte ("Count",      static_cast<Byte>(a_Item.m_ItemCount));
		if (a_Slot >= 0)
		{
			mWriter.AddByte ("Slot", static_cast<unsigned char>(a_Slot));
		}

		// Write the tag compound (for enchantment, firework, custom name and repair cost):
		if (
			(!a_Item.m_Enchantments.IsEmpty()) ||
			((a_Item.m_ItemType == E_ITEM_FIREWORK_ROCKET) || (a_Item.m_ItemType == E_ITEM_FIREWORK_STAR)) ||
			(a_Item.m_RepairCost > 0) ||
			(a_Item.m_CustomName != "") ||
			(!a_Item.m_LoreTable.empty())
		)
		{
			mWriter.BeginCompound("tag");
				if (a_Item.m_RepairCost > 0)
				{
					mWriter.AddInt("RepairCost", a_Item.m_RepairCost);
				}

				if ((a_Item.m_CustomName != "") || (!a_Item.m_LoreTable.empty()))
				{
					mWriter.BeginCompound("display");
					if (a_Item.m_CustomName != "")
					{
						mWriter.AddString("Name", a_Item.m_CustomName);
					}
					if (!a_Item.m_LoreTable.empty())
					{
						mWriter.BeginList("Lore", TAG_String);

						for (const auto & Line : a_Item.m_LoreTable)
						{
							mWriter.AddString("", Line);
						}

						mWriter.EndList();
					}
					mWriter.EndCompound();
				}

				if ((a_Item.m_ItemType == E_ITEM_FIREWORK_ROCKET) || (a_Item.m_ItemType == E_ITEM_FIREWORK_STAR))
				{
					cFireworkItem::WriteToNBTCompound(a_Item.m_FireworkItem, mWriter, static_cast<ENUM_ITEM_TYPE>(a_Item.m_ItemType));
				}

				if (!a_Item.m_Enchantments.IsEmpty())
				{
					const char * TagName = (a_Item.m_ItemType == E_ITEM_BOOK) ? "StoredEnchantments" : "ench";
					EnchantmentSerializer::WriteToNBTCompound(a_Item.m_Enchantments, mWriter, TagName);
				}
			mWriter.EndCompound();
		}

		mWriter.EndCompound();
	}





	/** Writes an item grid into the writer.
	Begins the stored slot numbers with a_BeginSlotNum.
	Note that it doesn't begin nor end the list tag, so that multiple grids may be concatenated together using this function. */
	void AddItemGrid(const cItemGrid & a_Grid, int a_BeginSlotNum = 0)
	{
		int NumSlots = a_Grid.GetNumSlots();
		for (int i = 0; i < NumSlots; i++)
		{
			const cItem & Item = a_Grid.GetSlot(i);
			if (Item.IsEmpty())
			{
				continue;
			}
			AddItem(Item, i + a_BeginSlotNum);
		}  // for i - slots[]
	}





	void AddBasicTileEntity(cBlockEntity * a_Entity, const char * a_EntityTypeID)
	{
		mWriter.AddInt   ("x",  a_Entity->GetPosX());
		mWriter.AddInt   ("y",  a_Entity->GetPosY());
		mWriter.AddInt   ("z",  a_Entity->GetPosZ());
		if (IsNative())
		{
			mWriter.AddString("id", fmt::format(FMT_STRING("minecraft:{}"), NativeBlockEntityID(a_EntityTypeID, a_Entity->GetBlockType())));
		}
		else
		{
			mWriter.AddString("id", a_EntityTypeID);
		}
	}





	void AddBannerEntity(cBannerEntity * a_Entity)
	{
		mWriter.BeginCompound("");
			AddBasicTileEntity(a_Entity,"Banner");
			if (!IsNative())
			{
				mWriter.AddInt("Base", static_cast<int>(a_Entity->GetBaseColor()));
			}
			if (!a_Entity->GetCustomName().empty())
			{
				AddText("CustomName", a_Entity->GetCustomName());
			}
		mWriter.EndCompound();
	}





	void AddBeaconEntity(cBeaconEntity * a_Entity)
	{
		mWriter.BeginCompound("");
			AddBasicTileEntity(a_Entity, "Beacon");
			mWriter.AddInt("Levels", a_Entity->GetBeaconLevel());
			mWriter.AddInt("Primary", static_cast<int>(a_Entity->GetPrimaryEffect()));
			mWriter.AddInt("Secondary", static_cast<int>(a_Entity->GetSecondaryEffect()));
			mWriter.BeginList("Items", TAG_Compound);
				AddItemGrid(a_Entity->GetContents());
			mWriter.EndList();
		mWriter.EndCompound();
	}





	void AddBedEntity(cBedEntity * a_Entity)
	{
		mWriter.BeginCompound("");
		AddBasicTileEntity(a_Entity, "Bed");
		mWriter.AddInt("color", a_Entity->GetColor());
		mWriter.EndCompound();
	}





	void AddBrewingstandEntity(cBrewingstandEntity * a_Brewingstand)
	{
		mWriter.BeginCompound("");
			AddBasicTileEntity(a_Brewingstand, "Brewingstand");
			mWriter.BeginList("Items", TAG_Compound);
				AddItemGrid(a_Brewingstand->GetContents());
			mWriter.EndList();
			mWriter.AddShort("BrewTime", a_Brewingstand->GetTimeBrewed());
			if (IsNative())
			{
				mWriter.AddByte("Fuel", static_cast<Byte>(a_Brewingstand->GetRemainingFuel()));
			}
			else
			{
				mWriter.AddShort("Fuel", a_Brewingstand->GetRemainingFuel());
			}
		mWriter.EndCompound();
	}





	void AddChestEntity(cChestEntity * a_Entity, BLOCKTYPE a_ChestType)
	{
		mWriter.BeginCompound("");
			AddBasicTileEntity(a_Entity, "Chest");
			mWriter.BeginList("Items", TAG_Compound);
				AddItemGrid(a_Entity->GetContents());
			mWriter.EndList();
		mWriter.EndCompound();
	}





	void AddDispenserEntity(cDispenserEntity * a_Entity)
	{
		mWriter.BeginCompound("");
			AddBasicTileEntity(a_Entity, "Trap");
			mWriter.BeginList("Items", TAG_Compound);
				AddItemGrid(a_Entity->GetContents());
			mWriter.EndList();
		mWriter.EndCompound();
	}





	void AddDropperEntity(cDropperEntity * a_Entity)
	{
		mWriter.BeginCompound("");
			AddBasicTileEntity(a_Entity, "Dropper");
			mWriter.BeginList("Items", TAG_Compound);
				AddItemGrid(a_Entity->GetContents());
			mWriter.EndList();
		mWriter.EndCompound();
	}





	void AddEnchantingTableEntity(cEnchantingTableEntity * a_Entity)
	{
		mWriter.BeginCompound("");
			AddBasicTileEntity(a_Entity, "EnchantingTable");
			if (!a_Entity->GetCustomName().empty())
			{
				AddText("CustomName", a_Entity->GetCustomName());
			}
		mWriter.EndCompound();
	}




	void AddEnderchestEntity(cEnderChestEntity * a_Entity)
	{
		mWriter.BeginCompound("");
			AddBasicTileEntity(a_Entity, "EnderChest");
		mWriter.EndCompound();
	}




	void AddEndPortalEntity(cEndPortalEntity * a_Entity)
	{
		mWriter.BeginCompound("");
			AddBasicTileEntity(a_Entity, "EndPortal");
		mWriter.EndCompound();
	}





	void AddFurnaceEntity(cFurnaceEntity * a_Furnace)
	{
		mWriter.BeginCompound("");
			AddBasicTileEntity(a_Furnace, "Furnace");
			mWriter.BeginList("Items", TAG_Compound);
				AddItemGrid(a_Furnace->GetContents());
			mWriter.EndList();
			mWriter.AddShort("BurnTime", static_cast<Int16>(a_Furnace->GetFuelBurnTimeLeft()));
			mWriter.AddShort("CookTime", static_cast<Int16>(a_Furnace->GetTimeCooked()));
			if (IsNative())
			{
				mWriter.AddShort("CookTimeTotal", static_cast<Int16>(a_Furnace->GetTimeCooked() + a_Furnace->GetCookTimeLeft()));
			}
		mWriter.EndCompound();
	}





	void AddHopperEntity(cHopperEntity * a_Entity)
	{
		mWriter.BeginCompound("");
			AddBasicTileEntity(a_Entity, "Hopper");
			mWriter.BeginList("Items", TAG_Compound);
				AddItemGrid(a_Entity->GetContents());
			mWriter.EndList();
		mWriter.EndCompound();
	}





	void AddJukeboxEntity(cJukeboxEntity * a_Jukebox)
	{
		mWriter.BeginCompound("");
			AddBasicTileEntity(a_Jukebox, "RecordPlayer");
			if (IsNative())
			{
				if (a_Jukebox->GetRecord() > 0)
				{
					AddItem(cItem(static_cast<short>(a_Jukebox->GetRecord())), -1, "RecordItem");
				}
			}
			else
			{
				mWriter.AddInt("Record", a_Jukebox->GetRecord());
			}
		mWriter.EndCompound();
	}





	void AddMobSpawnerEntity(cMobSpawnerEntity * a_MobSpawner)
	{
		mWriter.BeginCompound("");
			AddBasicTileEntity(a_MobSpawner, "MobSpawner");
			if (IsNative())
			{
				const auto ID = NativeMobID(a_MobSpawner->GetEntity(), mDataVersion);
				if (ID.empty())
				{
					throw std::runtime_error("Spawner entity has no native ID in this world version");
				}
				mWriter.BeginCompound("SpawnData");
				mWriter.AddString("id", fmt::format(FMT_STRING("minecraft:{}"), ID));
				mWriter.EndCompound();
			}
			else
			{
				mWriter.AddString("EntityId", NamespaceSerializer::From(a_MobSpawner->GetEntity()));
			}
			mWriter.AddShort("SpawnCount", a_MobSpawner->GetSpawnCount());
			mWriter.AddShort("SpawnRange", a_MobSpawner->GetSpawnRange());
			mWriter.AddShort("Delay", a_MobSpawner->GetSpawnDelay());
			mWriter.AddShort("MinSpawnDelay", a_MobSpawner->GetMinSpawnDelay());
			mWriter.AddShort("MaxSpawnDelay", a_MobSpawner->GetMaxSpawnDelay());
			mWriter.AddShort("MaxNearbyEntities", a_MobSpawner->GetMaxNearbyEntities());
			mWriter.AddShort("RequiredPlayerRange", a_MobSpawner->GetRequiredPlayerRange());
		mWriter.EndCompound();
	}





	void AddNoteEntity(cNoteEntity * a_Note)
	{
		mWriter.BeginCompound("");
			AddBasicTileEntity(a_Note, "Music");
			mWriter.AddByte("note", static_cast<Byte>(a_Note->GetNote()));
		mWriter.EndCompound();
	}





	void AddCommandBlockEntity(cCommandBlockEntity * a_CmdBlock)
	{
		mWriter.BeginCompound("");
			AddBasicTileEntity(a_CmdBlock, "Control");
			mWriter.AddString("Command",      a_CmdBlock->GetCommand());
			mWriter.AddInt   ("SuccessCount", a_CmdBlock->GetResult());
			mWriter.AddString("LastOutput",   a_CmdBlock->GetLastOutput());
			mWriter.AddByte  ("TrackOutput",  1);  // TODO 2014-01-18 xdot: Figure out what TrackOutput is and save it.
		mWriter.EndCompound();
	}





	void AddSignEntity(cSignEntity * a_Sign)
	{
		mWriter.BeginCompound("");
			AddBasicTileEntity(a_Sign, "Sign");
			AddText("Text1", a_Sign->GetLine(0));
			AddText("Text2", a_Sign->GetLine(1));
			AddText("Text3", a_Sign->GetLine(2));
			AddText("Text4", a_Sign->GetLine(3));
		mWriter.EndCompound();
	}





	void AddMobHeadEntity(cMobHeadEntity * a_MobHead)
	{
		mWriter.BeginCompound("");
			AddBasicTileEntity(a_MobHead, "Skull");
			if (!IsNative())
			{
				mWriter.AddByte("SkullType", a_MobHead->GetType() & 0xFF);
				mWriter.AddByte("Rot", a_MobHead->GetRotation() & 0xFF);
			}
			if (IsNative() && a_MobHead->GetOwnerUUID().IsNil() && a_MobHead->GetOwnerName().empty())
			{
				mWriter.EndCompound();
				return;
			}

			// The new Block Entity format for a Mob Head. See: https://minecraft.wiki/w/Head#Block_entity
			mWriter.BeginCompound("Owner");
				const auto & Owner = a_MobHead->GetOwnerUUID();
				if (!IsNative() || !Owner.IsNil())
				{
					mWriter.AddString("Id", IsNative() ? Owner.ToLongString() : Owner.ToShortString());
				}
				const auto Name = a_MobHead->GetOwnerName();
				if (!IsNative() || !Name.empty())
				{
					mWriter.AddString("Name", Name);
				}
				const auto Texture = a_MobHead->GetOwnerTexture();
				if (!IsNative() || !Texture.empty())
				{
					mWriter.BeginCompound("Properties");
					mWriter.BeginList("textures", TAG_Compound);
					mWriter.BeginCompound("");
					const auto Signature = a_MobHead->GetOwnerTextureSignature();
					if (!IsNative() || !Signature.empty())
					{
						mWriter.AddString("Signature", Signature);
					}
					mWriter.AddString("Value", Texture);
					mWriter.EndCompound();
					mWriter.EndList();
					mWriter.EndCompound();
				}
			mWriter.EndCompound();
		mWriter.EndCompound();
	}





	void AddFlowerPotEntity(cFlowerPotEntity * a_FlowerPot)
	{
		mWriter.BeginCompound("");
			AddBasicTileEntity(a_FlowerPot, "FlowerPot");
			mWriter.AddInt   ("Item", static_cast<Int32>(a_FlowerPot->GetItem().m_ItemType));
			mWriter.AddInt   ("Data", static_cast<Int32>(a_FlowerPot->GetItem().m_ItemDamage));
		mWriter.EndCompound();
	}





	void AddBasicEntity(cEntity * a_Entity, const std::string_view a_ClassName)
	{
		if (IsNative())
		{
			const auto ID = a_Entity->IsMob() ? NativeMobID(static_cast<const cMonster &>(*a_Entity), mDataVersion) : NativeEntityID(a_ClassName);
			mWriter.AddString("id", fmt::format(FMT_STRING("minecraft:{}"), ID));
		}
		else
		{
			mWriter.AddString("id", a_ClassName);
		}
		mWriter.BeginList("Pos", TAG_Double);
			mWriter.AddDouble("", a_Entity->GetPosX());
			mWriter.AddDouble("", a_Entity->GetPosY());
			mWriter.AddDouble("", a_Entity->GetPosZ());
		mWriter.EndList();
		mWriter.BeginList("Motion", TAG_Double);
			mWriter.AddDouble("", a_Entity->GetSpeedX());
			mWriter.AddDouble("", a_Entity->GetSpeedY());
			mWriter.AddDouble("", a_Entity->GetSpeedZ());
		mWriter.EndList();
		if (IsNative())
		{
			mWriter.BeginList("Rotation", TAG_Float);
			mWriter.AddFloat("", static_cast<float>(a_Entity->GetYaw()));
			mWriter.AddFloat("", static_cast<float>(a_Entity->GetPitch()));
			mWriter.EndList();
		}
		else
		{
			mWriter.BeginList("Rotation", TAG_Double);
			mWriter.AddDouble("", a_Entity->GetYaw());
			mWriter.AddDouble("", a_Entity->GetPitch());
			mWriter.EndList();
		}
		mWriter.AddFloat("Health", a_Entity->GetHealth());
		if (IsNative())
		{
			mWriter.AddShort("Air", static_cast<Int16>(a_Entity->GetAirLevel()));
			mWriter.AddByte("OnGround", a_Entity->IsOnGround());
		}
	}





	void AddBoatEntity(cBoat * a_Boat)
	{
		mWriter.BeginCompound("");
			AddBasicEntity(a_Boat, "Boat");
			mWriter.AddString("Type", cBoat::MaterialToString(a_Boat->GetMaterial()));
		mWriter.EndCompound();
	}





	void AddEnderCrystalEntity(cEnderCrystal * a_EnderCrystal)
	{
		mWriter.BeginCompound("");
			AddBasicEntity(a_EnderCrystal, "EnderCrystal");
			mWriter.AddByte("ShowBottom", a_EnderCrystal->ShowsBottom() ? 1 : 0);
			if (a_EnderCrystal->DisplaysBeam())
			{
				mWriter.BeginCompound("BeamTarget");
				const auto & BeamTarget = a_EnderCrystal->GetBeamTarget();
				mWriter.AddInt("X", BeamTarget.x);
				mWriter.AddInt("Y", BeamTarget.y);
				mWriter.AddInt("Z", BeamTarget.z);
				mWriter.EndCompound();
			}
		mWriter.EndCompound();
	}





	void AddFallingBlockEntity(cFallingBlock * a_FallingBlock)
	{
		mWriter.BeginCompound("");
			AddBasicEntity(a_FallingBlock, "FallingSand");
			if (IsNative())
			{
				AddNativeBlockState("BlockState", a_FallingBlock->GetBlockType(), a_FallingBlock->GetBlockMeta());
				mWriter.AddInt("Time", 1);
			}
			else
			{
				mWriter.AddInt("TileID", a_FallingBlock->GetBlockType());
				mWriter.AddByte("Data", a_FallingBlock->GetBlockMeta());
				mWriter.AddByte("Time", 1);  // Unused in Cuberite, Vanilla said to need nonzero
			}
			mWriter.AddByte("DropItem", 1);
			mWriter.AddByte("HurtEntities", a_FallingBlock->GetBlockType() == E_BLOCK_ANVIL);
		mWriter.EndCompound();
	}





	void AddMinecartEntity(cMinecart * a_Minecart)
	{
		mWriter.BeginCompound("");

			switch (a_Minecart->GetPayload())
			{
				case cMinecart::mpChest:
				{
					AddBasicEntity(a_Minecart, "MinecartChest");
					// Add chest contents into the Items tag:
					AddMinecartChestContents(static_cast<cMinecartWithChest *>(a_Minecart));
					break;
				}
				case cMinecart::mpFurnace:
				{
					AddBasicEntity(a_Minecart, "MinecartFurnace");
					// TODO: Add "Push" and "Fuel" tags
					break;
				}
				case cMinecart::mpHopper:
				{
					AddBasicEntity(a_Minecart, "MinecartHopper");
					// TODO: Add hopper contents?
					break;
				}
				case cMinecart::mpTNT:
				{
					AddBasicEntity(a_Minecart, "MinecartTNT");
					break;
				}
				case cMinecart::mpNone:
				{
					AddBasicEntity(a_Minecart, "MinecartRideable");
					break;
				}
			}  // switch (Payload)

		mWriter.EndCompound();
	}





	void AddNativeMonsterEquipment(const cMonster & a_Monster)
	{
		mWriter.BeginList("HandItems", TAG_Compound);
		AddItem(a_Monster.GetEquippedWeapon(), -1);
		AddItem(a_Monster.GetOffHandEquipedItem(), -1);
		mWriter.EndList();

		mWriter.BeginList("ArmorItems", TAG_Compound);
		AddItem(a_Monster.GetEquippedBoots(), -1);
		AddItem(a_Monster.GetEquippedLeggings(), -1);
		AddItem(a_Monster.GetEquippedChestplate(), -1);
		AddItem(a_Monster.GetEquippedHelmet(), -1);
		mWriter.EndList();
	}





	void AddVillagerProfession(int a_Profession)
	{
		if (!IsNative() || (mDataVersion < 1952))
		{
			mWriter.AddInt("Profession", a_Profession);
			return;
		}

		static constexpr std::string_view Professions[] =
		{
			"minecraft:farmer", "minecraft:librarian", "minecraft:cleric",
			"minecraft:armorer", "minecraft:butcher", "minecraft:nitwit",
		};
		mWriter.BeginCompound("VillagerData");
		mWriter.AddString("type", "minecraft:plains");
		const auto Index = static_cast<size_t>(a_Profession);
		mWriter.AddString("profession", (Index < ARRAYCOUNT(Professions)) ? Professions[Index] : "minecraft:none");
		mWriter.AddInt("level", 1);
		mWriter.EndCompound();
	}





	void AddNativeHorse(const cHorse & a_Horse)
	{
		mWriter.AddByte("Tame", a_Horse.IsTame());
		mWriter.AddByte("EatingHaystack", a_Horse.IsEating());
		mWriter.AddInt("Age", a_Horse.GetAge());
		const auto Type = a_Horse.GetHorseType();
		if (Type == 0)
		{
			mWriter.AddInt("Variant", a_Horse.GetHorseColor() | (a_Horse.GetHorseStyle() << 8));
			if (!a_Horse.GetHorseArmorItem().IsEmpty())
			{
				AddItem(a_Horse.GetHorseArmorItem(), -1, "ArmorItem");
			}
		}
		else if ((Type == 1) || (Type == 2))
		{
			mWriter.AddByte("ChestedHorse", a_Horse.IsChested());
		}
		if (a_Horse.IsSaddled())
		{
			AddItem(a_Horse.GetHorseSaddle(), -1, "SaddleItem");
		}
	}





	void AddMonsterEntity(cMonster * a_Monster)
	{
		mWriter.BeginCompound("");
			AddBasicEntity(a_Monster, NamespaceSerializer::From(a_Monster->GetMobType()));
			if (IsNative())
			{
				mWriter.BeginList("HandDropChances", TAG_Float);
				mWriter.AddFloat("", a_Monster->GetDropChanceWeapon());
				mWriter.AddFloat("", 0.085f);
				mWriter.EndList();

				mWriter.BeginList("ArmorDropChances", TAG_Float);
				mWriter.AddFloat("", a_Monster->GetDropChanceBoots());
				mWriter.AddFloat("", a_Monster->GetDropChanceLeggings());
				mWriter.AddFloat("", a_Monster->GetDropChanceChestplate());
				mWriter.AddFloat("", a_Monster->GetDropChanceHelmet());
				mWriter.EndList();

				AddNativeMonsterEquipment(*a_Monster);
				if (!a_Monster->GetCustomName().empty())
				{
					AddText("CustomName", a_Monster->GetCustomName());
					mWriter.AddByte("PersistenceRequired", 1);
				}
			}
			else
			{
				mWriter.BeginList("DropChances", TAG_Float);
				mWriter.AddFloat("", a_Monster->GetDropChanceWeapon());
				mWriter.AddFloat("", a_Monster->GetDropChanceHelmet());
				mWriter.AddFloat("", a_Monster->GetDropChanceChestplate());
				mWriter.AddFloat("", a_Monster->GetDropChanceLeggings());
				mWriter.AddFloat("", a_Monster->GetDropChanceBoots());
				mWriter.EndList();
				mWriter.AddString("CustomName", a_Monster->GetCustomName());
			}
			mWriter.AddByte("CanPickUpLoot", (a_Monster->CanPickUpLoot())? 1 : 0);
			mWriter.AddByte("CustomNameVisible", static_cast<Byte>(a_Monster->IsCustomNameAlwaysVisible()));

			// Mob was leashed
			if (a_Monster->IsLeashed() || (a_Monster->GetLeashToPos() != nullptr))
			{
				mWriter.AddByte("Leashed", 1);

				const Vector3d * LeashedToPos = nullptr;

				if (a_Monster->GetLeashToPos() != nullptr)
				{
					LeashedToPos = a_Monster->GetLeashToPos();
				}
				else if (a_Monster->GetLeashedTo()->IsLeashKnot())
				{
					LeashedToPos = & a_Monster->GetLeashedTo()->GetPosition();
				}

				if (LeashedToPos != nullptr)
				{
					mWriter.BeginCompound("Leash");
					if (IsNative())
					{
						mWriter.AddInt("X", FloorC(LeashedToPos->x));
						mWriter.AddInt("Y", FloorC(LeashedToPos->y));
						mWriter.AddInt("Z", FloorC(LeashedToPos->z));
					}
					else
					{
						mWriter.AddDouble("X", LeashedToPos->x);
						mWriter.AddDouble("Y", LeashedToPos->y);
						mWriter.AddDouble("Z", LeashedToPos->z);
					}
					mWriter.EndCompound();
				}
			}

			switch (a_Monster->GetMobType())
			{
				case mtBee:
				{
					BeeNBT::WriteFields(mWriter, static_cast<const cBee &>(*a_Monster));
					break;
				}
				case mtBat:
				{
					mWriter.AddByte("BatFlags", static_cast<const cBat *>(a_Monster)->IsHanging());
					break;
				}
				case mtCreeper:
				{
					const cCreeper *Creeper = static_cast<const cCreeper *>(a_Monster);
					mWriter.AddByte("powered", Creeper->IsCharged());
					mWriter.AddByte("ignited", Creeper->IsBlowing());
					break;
				}
				case mtEnderman:
				{
					const cEnderman *Enderman = static_cast<const cEnderman *>(a_Monster);
					if (IsNative())
					{
						if (Enderman->GetCarriedBlock() != E_BLOCK_AIR)
						{
							AddNativeBlockState("carriedBlockState", Enderman->GetCarriedBlock(), Enderman->GetCarriedMeta());
						}
					}
					else
					{
						mWriter.AddShort("carried",     static_cast<Int16>(Enderman->GetCarriedBlock()));
						mWriter.AddShort("carriedData", static_cast<Int16>(Enderman->GetCarriedMeta()));
					}
					break;
				}
				case mtHorse:
				{
					const cHorse *Horse = static_cast<const cHorse *>(a_Monster);
					if (IsNative())
					{
						AddNativeHorse(*Horse);
						break;
					}
					mWriter.AddByte("ChestedHorse",   Horse->IsChested()? 1 : 0);
					mWriter.AddByte("EatingHaystack", Horse->IsEating()? 1 : 0);
					mWriter.AddByte("Tame",           Horse->IsTame()? 1: 0);
					mWriter.AddInt ("Type",           Horse->GetHorseType());
					mWriter.AddInt ("Color",          Horse->GetHorseColor());
					mWriter.AddInt ("Style",          Horse->GetHorseStyle());
					mWriter.AddInt ("ArmorType",      Horse->GetHorseArmour());
					mWriter.AddByte("Saddle",         Horse->IsSaddled()? 1 : 0);
					mWriter.AddInt ("Age",            Horse->GetAge());
					break;
				}
				case mtMagmaCube:
				{
					mWriter.AddInt("Size", static_cast<const cMagmaCube *>(a_Monster)->GetSize() - (IsNative() ? 1 : 0));
					break;
				}
				case mtOcelot:
				{
					const auto *Ocelot = static_cast<const cOcelot *>(a_Monster);
					const bool NativeCat = IsNative() && UsesNativeCatRecord(*Ocelot, mDataVersion);
					if (!IsNative() || (mDataVersion < 1952) || NativeCat)
					{
						if (!IsNative() && !Ocelot->GetOwnerName().empty())
						{
							mWriter.AddString("Owner", Ocelot->GetOwnerName());
						}
						if (!Ocelot->GetOwnerUUID().IsNil())
						{
							const auto & Owner = Ocelot->GetOwnerUUID();
							mWriter.AddString("OwnerUUID", IsNative() ? Owner.ToLongString() : Owner.ToShortString());
						}
						mWriter.AddByte("Sitting", Ocelot->IsSitting() ? 1 : 0);
						const auto Coat = Ocelot->GetOcelotType();
						// Modern cats reserve 0 for tabby. Black and Siamese use their old IDs.
						mWriter.AddInt("CatType", (NativeCat && (Coat == cOcelot::ctTabby)) ? 0 : Coat);
					}
					mWriter.AddInt("Age", Ocelot->GetAge());
					break;
				}
				case mtPig:
				{
					const auto & Pig = static_cast<const cPig &>(*a_Monster);
					mWriter.AddInt("Age", Pig.GetAge());
					if (IsNative())
					{
						mWriter.AddByte("Saddle", Pig.IsSaddled());
					}
					break;
				}
				case mtRabbit:
				{
					const cRabbit * Rabbit = static_cast<const cRabbit *>(a_Monster);
					mWriter.AddInt("RabbitType", static_cast<Int32>(Rabbit->GetRabbitType()));
					mWriter.AddInt("MoreCarrotTicks", Rabbit->GetMoreCarrotTicks());
					mWriter.AddInt("Age", Rabbit->GetAge());
					break;
				}
				case mtSheep:
				{
					const cSheep *Sheep = static_cast<const cSheep *>(a_Monster);
					mWriter.AddByte("Sheared", Sheep->IsSheared()? 1 : 0);
					mWriter.AddByte("Color",   static_cast<Byte>(Sheep->GetFurColor()));
					mWriter.AddInt ("Age",     Sheep->GetAge());
					break;
				}
				case mtSlime:
				{
					mWriter.AddInt("Size", static_cast<const cSlime *>(a_Monster)->GetSize() - (IsNative() ? 1 : 0));
					break;
				}
				case mtVillager:
				{
					const cVillager *Villager = static_cast<const cVillager *>(a_Monster);
					AddVillagerProfession(Villager->GetVilType());
					mWriter.AddInt("Age", Villager->GetAge());
					mWriter.BeginList("Inventory", TAG_Compound);
					if (IsNative())
					{
						for (int Slot = 0; Slot < Villager->GetInventory().GetNumSlots(); ++Slot)
						{
							const auto & Item = Villager->GetInventory().GetSlot(Slot);
							if (!Item.IsEmpty())
							{
								AddItem(Item, -1);
							}
						}
					}
					else
					{
						AddItemGrid(Villager->GetInventory());
					}
					mWriter.EndList();
					break;
				}
				case mtWither:
				{
					mWriter.AddInt("Invul", static_cast<Int32>(static_cast<const cWither *>(a_Monster)->GetWitherInvulnerableTicks()));
					break;
				}
				case mtWolf:
				{
					const cWolf *Wolf = static_cast<const cWolf *>(a_Monster);
					if (!IsNative() && !Wolf->GetOwnerName().empty())
					{
						mWriter.AddString("Owner", Wolf->GetOwnerName());
					}
					if (!Wolf->GetOwnerUUID().IsNil())
					{
						const auto & Owner = Wolf->GetOwnerUUID();
						mWriter.AddString("OwnerUUID", IsNative() ? Owner.ToLongString() : Owner.ToShortString());
					}
					mWriter.AddByte("Sitting",     Wolf->IsSitting() ? 1 : 0);
					mWriter.AddByte("Angry",       Wolf->IsAngry() ? 1 : 0);
					mWriter.AddByte("CollarColor", static_cast<Byte>(Wolf->GetCollarColor()));
					mWriter.AddInt ("Age",         Wolf->GetAge());
					break;
				}
				case mtZombie:
				case mtZombiePigman:
				{
					if (IsNative())
					{
						mWriter.AddByte("IsBaby", a_Monster->IsBaby());
					}
					else
					{
						mWriter.AddInt("Age", a_Monster->GetAge());
					}
					break;
				}
				case mtZombieVillager:
				{
					const cZombieVillager *ZombieVillager = reinterpret_cast<const cZombieVillager *>(a_Monster);
					AddVillagerProfession(ZombieVillager->GetProfession());
					mWriter.AddInt("ConversionTime", ZombieVillager->ConversionTime());
					if (IsNative())
					{
						mWriter.AddByte("IsBaby", ZombieVillager->IsBaby());
					}
					else
					{
						mWriter.AddInt("Age", ZombieVillager->GetAge());
					}
					break;
				}
				case mtChicken:
				case mtCow:
				case mtMooshroom:
				{
					if (IsNative())
					{
						mWriter.AddInt("Age", a_Monster->GetAge());
					}
					break;
				}
				case mtBlaze:
				case mtCaveSpider:
				case mtEnderDragon:
				case mtGhast:
				case mtGiant:
				case mtGuardian:
				case mtIronGolem:
				case mtSilverfish:
				case mtSkeleton:
				case mtSnowGolem:
				case mtSpider:
				case mtSquid:
				case mtWitch:
				case mtWitherSkeleton:
				{
					// Other mobs have no special tags.
					break;
				}
				case mtCat:
				case mtCod:
				case mtDolphin:
				case mtDonkey:
				case mtDrowned:
				case mtElderGuardian:
				case mtEndermite:
				case mtEvoker:
				case mtFox:
				case mtHoglin:
				case mtHusk:
				case mtIllusioner:
				case mtLlama:
				case mtMule:
				case mtPanda:
				case mtParrot:
				case mtPhantom:
				case mtPiglin:
				case mtPiglinBrute:
				case mtPillager:
				case mtPolarBear:
				case mtPufferfish:
				case mtRavager:
				case mtSalmon:
				case mtShulker:
				case mtSkeletonHorse:
				case mtStray:
				case mtStrider:
				case mtTraderLlama:
				case mtTropicalFish:
				case mtTurtle:
				case mtVex:
				case mtVindicator:
				case mtWanderingTrader:
				case mtZoglin:
				case mtZombieHorse:
				{
					// All the entities not added
					LOGD("Saving unimplemented entity type: %d", NamespaceSerializer::From(a_Monster->GetMobType()));
					break;
				}
				case mtInvalidType:
				{
					ASSERT(!"NBTChunkSerializer::SerializerCollector::AddMonsterEntity: Recieved mob of invalid type");
					break;
				}
			}
		mWriter.EndCompound();
	}





	void AddPickupEntity(cPickup * a_Pickup)
	{
		mWriter.BeginCompound("");
			AddBasicEntity(a_Pickup, "Item");
			AddItem(a_Pickup->GetItem(), -1, "Item");
			mWriter.AddShort("Age",    static_cast<Int16>(a_Pickup->GetAge()));
		mWriter.EndCompound();
	}





	void AddProjectileEntity(cProjectileEntity * a_Projectile)
	{
		mWriter.BeginCompound("");
			AddBasicEntity(a_Projectile, a_Projectile->GetMCAClassName());
			mWriter.AddByte("inGround", a_Projectile->IsInGround() ? 1 : 0);

			switch (a_Projectile->GetProjectileKind())
			{
				case cProjectileEntity::pkArrow:
				{
					cArrowEntity * Arrow = static_cast<cArrowEntity *>(a_Projectile);

					if (!IsNative())
					{
						mWriter.AddShort("xTile", static_cast<Int16>(Arrow->GetBlockHit().x));
						mWriter.AddShort("yTile", static_cast<Int16>(Arrow->GetBlockHit().y));
						mWriter.AddShort("zTile", static_cast<Int16>(Arrow->GetBlockHit().z));
					}
					mWriter.AddByte("pickup", static_cast<unsigned char>(Arrow->GetPickupState()));
					mWriter.AddDouble("damage", Arrow->GetDamageCoeff());
					break;
				}
				case cProjectileEntity::pkSplashPotion:
				{
					cSplashPotionEntity * Potion = static_cast<cSplashPotionEntity *>(a_Projectile);

					if (IsNative())
					{
						AddItem(Potion->GetItem(), -1, "Potion");
						break;
					}

					mWriter.AddInt("EffectType",                static_cast<Int16>(Potion->GetEntityEffectType()));
					mWriter.AddInt("EffectDuration",            static_cast<Int16>(Potion->GetEntityEffect().GetDuration()));
					mWriter.AddShort("EffectIntensity",         Potion->GetEntityEffect().GetIntensity());
					mWriter.AddDouble("EffectDistanceModifier", Potion->GetEntityEffect().GetDistanceModifier());
					mWriter.AddInt("PotionName",                Potion->GetPotionColor());
					break;
				}
				case cProjectileEntity::pkGhastFireball:
				{
					mWriter.AddInt("ExplosionPower", 1);
					break;
				}
				case cProjectileEntity::pkFireCharge:
				case cProjectileEntity::pkWitherSkull:
				case cProjectileEntity::pkEnderPearl:
				case cProjectileEntity::pkSnowball:
				case cProjectileEntity::pkEgg:
				case cProjectileEntity::pkExpBottle:
				case cProjectileEntity::pkFirework:
				{
					break;
				}
				default:
				{
					ASSERT(!"Unsaved projectile entity!");
				}
			}  // switch (ProjectileKind)

			if (!a_Projectile->GetCreatorName().empty())
			{
				mWriter.AddString("ownerName", a_Projectile->GetCreatorName());
			}
		mWriter.EndCompound();
	}





	void AddHangingEntity(cHangingEntity * a_Hanging)
	{
		mWriter.AddInt("TileX", FloorC(a_Hanging->GetPosX()));
		mWriter.AddInt("TileY", FloorC(a_Hanging->GetPosY()));
		mWriter.AddInt("TileZ", FloorC(a_Hanging->GetPosZ()));
		mWriter.AddByte("Facing", a_Hanging->GetProtocolFacing());
	}





	void AddTNTEntity(cTNTEntity * a_TNT)
	{
		mWriter.BeginCompound("");
			AddBasicEntity(a_TNT, "PrimedTnt");
			if (IsNative())
			{
				const auto Fuse = std::min(a_TNT->GetFuseTicks(), static_cast<unsigned>(std::numeric_limits<Int16>::max()));
				mWriter.AddShort("Fuse", static_cast<Int16>(Fuse));
			}
			else
			{
				mWriter.AddByte("Fuse", static_cast<unsigned char>(a_TNT->GetFuseTicks()));
			}
		mWriter.EndCompound();
	}





	void AddExpOrbEntity(cExpOrb * a_ExpOrb)
	{
		mWriter.BeginCompound("");
			AddBasicEntity(a_ExpOrb, "XPOrb");
			mWriter.AddShort("Age", static_cast<Int16>(a_ExpOrb->GetAge()));
			mWriter.AddShort("Value", static_cast<Int16>(a_ExpOrb->GetReward()));
		mWriter.EndCompound();
	}





	void AddItemFrameEntity(cItemFrame * a_ItemFrame)
	{
		mWriter.BeginCompound("");
			AddBasicEntity(a_ItemFrame, "ItemFrame");
			AddHangingEntity(a_ItemFrame);
			AddItem(a_ItemFrame->GetItem(), -1, "Item");
			mWriter.AddByte("ItemRotation", static_cast<Byte>(a_ItemFrame->GetItemRotation()));
			mWriter.AddFloat("ItemDropChance", 1.0F);
		mWriter.EndCompound();
	}





	void AddLeashKnotEntity(cLeashKnot * a_LeashKnot)
	{
		mWriter.BeginCompound("");
			AddBasicEntity(a_LeashKnot, "LeashKnot");
			AddHangingEntity(a_LeashKnot);
		mWriter.EndCompound();
	}





	void AddPaintingEntity(cPainting * a_Painting)
	{
		mWriter.BeginCompound("");
			AddBasicEntity(a_Painting, "Painting");
			AddHangingEntity(a_Painting);
			auto Motive = a_Painting->GetName();
			if (IsNative() && (Motive.find(':') == AString::npos))
			{
				AString ID = "minecraft:";
				for (const char Character : Motive)
				{
					if ((ID.size() > 10) && std::isupper(static_cast<unsigned char>(Character)))
					{
						ID += '_';
					}
					ID += static_cast<char>(std::tolower(static_cast<unsigned char>(Character)));
				}
				Motive = std::move(ID);
			}
			mWriter.AddString("Motive", Motive);
		mWriter.EndCompound();
	}





	void AddMinecartChestContents(cMinecartWithChest * a_Minecart)
	{
		mWriter.BeginList("Items", TAG_Compound);
			for (int i = 0; i < cMinecartWithChest::ContentsHeight * cMinecartWithChest::ContentsWidth; i++)
			{
				const cItem & Item = a_Minecart->GetSlot(i);
				if (Item.IsEmpty())
				{
					continue;
				}
				AddItem(Item, i);
			}
		mWriter.EndList();
	}
};  // SerializerCollector





////////////////////////////////////////////////////////////////////////////////
// NBTChunkSerializer:

void NBTChunkSerializer::Serialize(const cWorld & aWorld, cChunkCoords aCoords, cFastNBTWriter & aWriter)
{
	SerializerCollector serializer(aWriter);
	aWriter.BeginCompound("Level");
	aWriter.AddInt("xPos", aCoords.m_ChunkX);
	aWriter.AddInt("zPos", aCoords.m_ChunkZ);
	[[maybe_unused]] const bool Result = aWorld.GetChunkData(aCoords, serializer);  // Chunk must be present in order to save
	ASSERT(Result);
	serializer.Finish();  // Close NBT tags

	// Save biomes:
	aWriter.AddByteArray("Biomes", reinterpret_cast<const char *>(serializer.Biomes), ARRAYCOUNT(serializer.Biomes));

	// Save heightmap (Vanilla require this):
	aWriter.AddIntArray("HeightMap", reinterpret_cast<const int *>(serializer.Heights), ARRAYCOUNT(serializer.Heights));

	// Save blockdata:
	aWriter.BeginList("Sections", TAG_Compound);
	ChunkDef_ForEachSection(serializer.m_BlockData, serializer.m_LightData,
	{
		aWriter.BeginCompound("");

		if (Blocks != nullptr)
		{
			aWriter.AddByteArray("Blocks", reinterpret_cast<const char *>(Blocks->data()), Blocks->size());
		}
		else
		{
			aWriter.AddByteArray("Blocks", ChunkBlockData::SectionBlockCount, ChunkBlockData::DefaultValue);
		}

		if (Metas != nullptr)
		{
			aWriter.AddByteArray("Data", reinterpret_cast<const char *>(Metas->data()), Metas->size());
		}
		else
		{
			aWriter.AddByteArray("Data", ChunkBlockData::SectionMetaCount, ChunkBlockData::DefaultMetaValue);
		}

		if (BlockLights != nullptr)
		{
			aWriter.AddByteArray("BlockLight", reinterpret_cast<const char *>(BlockLights->data()), BlockLights->size());
		}
		else
		{
			aWriter.AddByteArray("BlockLight", ChunkLightData::SectionLightCount, ChunkLightData::DefaultBlockLightValue);
		}

		if (SkyLights != nullptr)
		{
			aWriter.AddByteArray("SkyLight", reinterpret_cast<const char *>(SkyLights->data()), SkyLights->size());
		}
		else
		{
			aWriter.AddByteArray("SkyLight", ChunkLightData::SectionLightCount, ChunkLightData::DefaultSkyLightValue);
		}

		aWriter.AddByte("Y", static_cast<unsigned char>(Y));
		aWriter.EndCompound();
	});
	aWriter.EndList();  // "Sections"

	// Store the information that the lighting is valid.
	// For compatibility reason, the default is "invalid" (missing) - this means older data is re-lighted upon loading.
	if (serializer.mIsLightValid)
	{
		aWriter.AddByte("MCSIsLightValid", 1);
	}

	// Save the world age to the chunk data. Required by vanilla and mcedit.
	aWriter.AddLong("LastUpdate", aWorld.GetWorldAge().count());

	// Store the flag that the chunk has all the ores, trees, dungeons etc. Cuberite chunks are always complete.
	aWriter.AddByte("TerrainPopulated", 1);

	aWriter.EndCompound();  // "Level"
}





ContiguousByteBuffer NBTChunkSerializer::SerializeEntity(cEntity & a_Entity, int a_DataVersion)
{
	cFastNBTWriter Writer;
	SerializerCollector Serializer(Writer, a_DataVersion);
	Serializer.AddEntityRecord(&a_Entity);
	Writer.Finish();
	return UnwrapEntityRecord(Writer);
}





ContiguousByteBuffer NBTChunkSerializer::SerializeBlockEntity(cBlockEntity & a_Entity, int a_DataVersion)
{
	cFastNBTWriter Writer;
	SerializerCollector Serializer(Writer, a_DataVersion);
	Serializer.AddBlockEntityRecord(&a_Entity);
	Writer.Finish();
	return UnwrapEntityRecord(Writer);
}
