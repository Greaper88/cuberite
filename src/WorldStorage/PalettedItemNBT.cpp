#include "Globals.h"
#include "PalettedItemNBT.h"
#include "FastNBT.h"
#include "BeeNBT.h"

#include "Item.h"
#include "JsonUtils.h"
#include "OSSupport/File.h"
#include "Protocol/LegacyBlockFallbacks.h"
#include "Protocol/Palettes/Palette_1_13.h"
#include "Protocol/Palettes/Palette_1_13_1.h"
#include "Protocol/Palettes/Palette_1_14.h"
#include "Protocol/Palettes/Palette_1_15.h"
#include "Protocol/Palettes/Upgrade.h"
#include "json/json.h"

#include <sstream>





namespace
{
	struct cItemPaletteVersion
	{
		size_t Column;
		cProtocol::Version ProtocolVersion;
		UInt32 (*GetProtocolID)(Item);
		Item (*GetItem)(UInt32);
	};

	cItemPaletteVersion GetItemPaletteVersion(int a_DataVersion)
	{
		switch (a_DataVersion)
		{
			case 1519:  // 1.13
			{
				return {0, cProtocol::Version::v1_13, Palette_1_13::From, Palette_1_13::ToItem};
			}
			case 1628:  // 1.13.1
			case 1631:  // 1.13.2
			{
				return {1, cProtocol::Version::v1_13_1, Palette_1_13_1::From, Palette_1_13_1::ToItem};
			}
			case 1952:  // 1.14
			case 1957:
			case 1963:
			case 1968:
			case 1976:  // 1.14.4
			{
				return {2, cProtocol::Version::v1_14, Palette_1_14::From, Palette_1_14::ToItem};
			}
			case 2225:  // 1.15
			case 2227:
			case 2230:  // 1.15.2
			{
				return {3, cProtocol::Version::v1_15_2, Palette_1_15::From, Palette_1_15::ToItem};
			}
			default:
			{
				throw std::runtime_error(fmt::format(FMT_STRING("No item registry for Java DataVersion {}"), a_DataVersion));
			}
		}
	}

	struct cPalettedItemRegistry
	{
		std::map<AString, UInt32, std::less<>> ByName;
		std::map<UInt32, AString> ByID;
	};

	const cPalettedItemRegistry & ItemRegistry(size_t a_Column)
	{
		static const auto Registries = []
		{
			static constexpr auto File = "Protocol/PalettedItems.tsv";
			std::array<cPalettedItemRegistry, 4> Result;
			std::istringstream Input(cFile::ReadWholeFile(File));
			AString Line;
			while (std::getline(Input, Line))
			{
				if (Line.empty() || (Line.front() == '#'))
				{
					continue;
				}
				const auto Fields = StringSplit(Line, "\t\r");
				if (Fields.size() != Result.size() + 1)
				{
					throw std::runtime_error(fmt::format(FMT_STRING("Invalid row in {}"), File));
				}
				const auto Name = "minecraft:" + Fields.front();
				for (size_t Column = 0; Column < Result.size(); ++Column)
				{
					const auto & Field = Fields[Column + 1];
					if (Field == "-")
					{
						continue;
					}
					UInt32 Number;
					if (
						!StringToInteger(Field, Number) ||
						!Result[Column].ByName.emplace(Name, Number).second ||
						!Result[Column].ByID.emplace(Number, Name).second
					)
					{
						throw std::runtime_error(fmt::format(FMT_STRING("Invalid or repeated item ID for {} in {}"), Name, File));
					}
				}
			}
			for (const auto & Registry : Result)
			{
				const auto Air = Registry.ByName.find("minecraft:air");
				if ((Air == Registry.ByName.end()) || (Air->second != 0))
				{
					throw std::runtime_error(fmt::format(FMT_STRING("Missing or invalid item registry {}"), File));
				}
			}
			return Result;
		}();
		return Registries[a_Column];
	}

	/** Checks that every field can be round-tripped. Rejecting unknown fields prevents inventory edits discarding them. */
	bool HasOnlyFields(const cParsedNBT & a_NBT, int a_Tag, std::initializer_list<std::string_view> a_Allowed)
	{
		if ((a_Tag < 0) || (a_NBT.GetType(a_Tag) != TAG_Compound))
		{
			return false;
		}
		std::set<AString> Seen;
		for (auto Child = a_NBT.GetFirstChild(a_Tag); Child >= 0; Child = a_NBT.GetNextSibling(Child))
		{
			const auto Name = a_NBT.GetName(Child);
			if ((std::find(a_Allowed.begin(), a_Allowed.end(), Name) == a_Allowed.end()) || !Seen.insert(Name).second)
			{
				return false;
			}
		}
		return true;
	}

	bool ReadPlainText(const AString & a_JSON, AString & a_Value)
	{
		Json::Value Component;
		if (!JsonUtils::ParseString(a_JSON, Component))
		{
			return false;
		}
		if (Component.isString())
		{
			a_Value = Component.asString();
			return true;
		}
		if (Component.isObject() && (Component.size() == 1) && Component["text"].isString())
		{
			a_Value = Component["text"].asString();
			return true;
		}
		return false;
	}

	bool ReadDisplay(cItem & a_Item, const cParsedNBT & a_NBT, int a_Tag, int a_DataVersion)
	{
		if (!HasOnlyFields(a_NBT, a_Tag, {"Name", "Lore", "color"}))
		{
			return false;
		}
		const auto Name = a_NBT.FindChildByName(a_Tag, "Name");
		if (
			(Name >= 0) &&
			((a_NBT.GetType(Name) != TAG_String) || !ReadPlainText(a_NBT.GetString(Name), a_Item.m_CustomName) || a_Item.m_CustomName.empty())
		)
		{
			return false;
		}
		const auto Color = a_NBT.FindChildByName(a_Tag, "color");
		if (Color >= 0)
		{
			if ((a_NBT.GetType(Color) != TAG_Int) || (a_NBT.GetInt(Color) < 0) || (a_NBT.GetInt(Color) > 0xffffff))
			{
				return false;
			}
			a_Item.m_ItemColor.m_Color = static_cast<unsigned int>(a_NBT.GetInt(Color));
		}
		const auto Lore = a_NBT.FindChildByName(a_Tag, "Lore");
		if (Lore < 0)
		{
			return true;
		}
		if ((a_NBT.GetType(Lore) != TAG_List) || ((a_NBT.GetChildrenType(Lore) != TAG_String) && (a_NBT.GetChildrenType(Lore) != TAG_End)))
		{
			return false;
		}
		for (auto Line = a_NBT.GetFirstChild(Lore); Line >= 0; Line = a_NBT.GetNextSibling(Line))
		{
			AString Text = a_NBT.GetString(Line);
			// Item lore became a JSON text component in Java 1.14.
			if ((a_DataVersion >= 1952) && !ReadPlainText(Text, Text))
			{
				return false;
			}
			a_Item.m_LoreTable.push_back(std::move(Text));
		}
		return true;
	}

	constexpr std::pair<int, std::string_view> PalettedEnchantments[] =
	{
		{0, "minecraft:protection"},
		{1, "minecraft:fire_protection"},
		{2, "minecraft:feather_falling"},
		{3, "minecraft:blast_protection"},
		{4, "minecraft:projectile_protection"},
		{5, "minecraft:respiration"},
		{6, "minecraft:aqua_affinity"},
		{7, "minecraft:thorns"},
		{8, "minecraft:depth_strider"},
		{9, "minecraft:frost_walker"},
		{10, "minecraft:binding_curse"},
		{16, "minecraft:sharpness"},
		{17, "minecraft:smite"},
		{18, "minecraft:bane_of_arthropods"},
		{19, "minecraft:knockback"},
		{20, "minecraft:fire_aspect"},
		{21, "minecraft:looting"},
		{22, "minecraft:sweeping"},
		{32, "minecraft:efficiency"},
		{33, "minecraft:silk_touch"},
		{34, "minecraft:unbreaking"},
		{35, "minecraft:fortune"},
		{48, "minecraft:power"},
		{49, "minecraft:punch"},
		{50, "minecraft:flame"},
		{51, "minecraft:infinity"},
		{61, "minecraft:luck_of_the_sea"},
		{62, "minecraft:lure"},
		{70, "minecraft:mending"},
		{71, "minecraft:vanishing_curse"},
	};

	bool ReadPalettedEnchantments(cItem & a_Item, const cParsedNBT & a_NBT, int a_Tag)
	{
		if (
			(a_NBT.GetType(a_Tag) != TAG_List) ||
			((a_NBT.GetChildrenType(a_Tag) != TAG_Compound) && (a_NBT.GetChildrenType(a_Tag) != TAG_End))
		)
		{
			return false;
		}
		for (auto Child = a_NBT.GetFirstChild(a_Tag); Child >= 0; Child = a_NBT.GetNextSibling(Child))
		{
			if (!HasOnlyFields(a_NBT, Child, {"id", "lvl"}))
			{
				return false;
			}

			const auto ID = a_NBT.FindChildByName(Child, "id");
			const auto Level = a_NBT.FindChildByName(Child, "lvl");
			if ((ID < 0) || (Level < 0) || (a_NBT.GetType(ID) != TAG_String) || (a_NBT.GetType(Level) != TAG_Short) || (a_NBT.GetShort(Level) <= 0))
			{
				return false;
			}
			const auto Name = a_NBT.GetStringView(ID);
			const auto It = std::find_if(std::begin(PalettedEnchantments), std::end(PalettedEnchantments), [Name](const auto & a_Entry)
			{
				return a_Entry.second == Name;
			});
			if ((It == std::end(PalettedEnchantments)) || (a_Item.m_Enchantments.GetLevel(It->first) != 0))
			{
				return false;
			}
			a_Item.m_Enchantments.SetLevel(It->first, static_cast<unsigned int>(a_NBT.GetShort(Level)));
		}
		return true;
	}

	bool IsPotion(short a_Type)
	{
		return (a_Type == E_ITEM_POTION) || (a_Type == E_ITEM_SPLASH_POTION) || (a_Type == E_ITEM_LINGERING_POTION);
	}

	// Legacy potion damage uses the low nibble for the effect and bits 5 and 6 for strength and duration.
	constexpr std::pair<short, std::string_view> PalettedPotions[] =
	{
		{0, "minecraft:water"},
		{16, "minecraft:awkward"},
		{32, "minecraft:thick"},
		{64, "minecraft:mundane"},
		{1, "minecraft:regeneration"},
		{33, "minecraft:strong_regeneration"},
		{65, "minecraft:long_regeneration"},
		{2, "minecraft:swiftness"},
		{34, "minecraft:strong_swiftness"},
		{66, "minecraft:long_swiftness"},
		{3, "minecraft:fire_resistance"},
		{67, "minecraft:long_fire_resistance"},
		{4, "minecraft:poison"},
		{36, "minecraft:strong_poison"},
		{68, "minecraft:long_poison"},
		{5, "minecraft:healing"},
		{37, "minecraft:strong_healing"},
		{6, "minecraft:night_vision"},
		{70, "minecraft:long_night_vision"},
		{8, "minecraft:weakness"},
		{72, "minecraft:long_weakness"},
		{9, "minecraft:strength"},
		{41, "minecraft:strong_strength"},
		{73, "minecraft:long_strength"},
		{10, "minecraft:slowness"},
		{74, "minecraft:long_slowness"},
		{11, "minecraft:leaping"},
		{43, "minecraft:strong_leaping"},
		{75, "minecraft:long_leaping"},
		{12, "minecraft:harming"},
		{44, "minecraft:strong_harming"},
		{13, "minecraft:water_breathing"},
		{77, "minecraft:long_water_breathing"},
		{14, "minecraft:invisibility"},
		{78, "minecraft:long_invisibility"},
	};

	bool ReadPotion(cItem & a_Item, const cParsedNBT & a_NBT, int a_Tag)
	{
		if ((a_Tag < 0) || (a_NBT.GetType(a_Tag) != TAG_String))
		{
			return false;
		}
		const auto Name = a_NBT.GetStringView(a_Tag);
		const auto It = std::find_if(std::begin(PalettedPotions), std::end(PalettedPotions), [Name](const auto & a_Entry)
		{
			return a_Entry.second == Name;
		});
		if (It == std::end(PalettedPotions))
		{
			return false;
		}
		const auto Flags = (a_Item.m_ItemType == E_ITEM_POTION) ? ((It->first == 0) ? 0 : 0x2000) : 0x4000;
		a_Item.m_ItemDamage = static_cast<short>(It->first | Flags);
		return true;
	}

	bool ReadExplosion(cFireworkItem & a_Item, const cParsedNBT & a_NBT, int a_Tag)
	{
		if (!HasOnlyFields(a_NBT, a_Tag, {"Type", "Colors", "FadeColors", "Flicker", "Trail"}))
		{
			return false;
		}
		for (const auto & [Name, Out] : {std::pair{"Colors", &a_Item.m_Colours}, std::pair{"FadeColors", &a_Item.m_FadeColours}})
		{
			const auto Tag = a_NBT.FindChildByName(a_Tag, Name);
			if (Tag < 0)
			{
				continue;
			}
			if (a_NBT.GetType(Tag) != TAG_IntArray)
			{
				return false;
			}
			const auto Data = a_NBT.GetData(Tag);
			for (size_t Offset = 0; Offset < a_NBT.GetDataLength(Tag); Offset += sizeof(Int32))
			{
				Out->push_back(NetworkBufToHost<Int32>(Data + Offset));
			}
		}
		const auto Type = a_NBT.FindChildByName(a_Tag, "Type");
		const auto Flicker = a_NBT.FindChildByName(a_Tag, "Flicker");
		const auto Trail = a_NBT.FindChildByName(a_Tag, "Trail");
		for (const auto Tag : {Type, Flicker, Trail})
		{
			if ((Tag >= 0) && (a_NBT.GetType(Tag) != TAG_Byte))
			{
				return false;
			}
		}
		a_Item.m_Type = (Type < 0) ? 0 : a_NBT.GetByte(Type);
		a_Item.m_HasFlicker = (Flicker >= 0) && (a_NBT.GetByte(Flicker) != 0);
		a_Item.m_HasTrail = (Trail >= 0) && (a_NBT.GetByte(Trail) != 0);
		return true;
	}

	bool ReadRocket(cFireworkItem & a_Item, const cParsedNBT & a_NBT, int a_Tag)
	{
		if (!HasOnlyFields(a_NBT, a_Tag, {"Flight", "Explosions"}))
		{
			return false;
		}
		const auto Flight = a_NBT.FindChildByName(a_Tag, "Flight");
		if (Flight >= 0)
		{
			if (a_NBT.GetType(Flight) != TAG_Byte)
			{
				return false;
			}
			a_Item.m_FlightTimeInTicks = static_cast<short>(a_NBT.GetByte(Flight) * 20);
		}
		const auto Explosions = a_NBT.FindChildByName(a_Tag, "Explosions");
		if (Explosions < 0)
		{
			return true;
		}
		if (a_NBT.GetType(Explosions) != TAG_List)
		{
			return false;
		}
		const auto Explosion = a_NBT.GetFirstChild(Explosions);
		// The engine supports one explosion per rocket.
		return (Explosion < 0) || ((a_NBT.GetNextSibling(Explosion) < 0) && ReadExplosion(a_Item, a_NBT, Explosion));
	}

	bool HasDurability(const cItem & a_Item)
	{
		return a_Item.IsDamageable() || (a_Item.m_ItemType == E_ITEM_SHIELD) || (a_Item.m_ItemType == E_ITEM_CARROT_ON_STICK);
	}

	bool ReadMetadata(cItem & a_Item, const cParsedNBT & a_NBT, int a_Tag, int a_DataVersion)
	{
		if (a_Tag < 0)
		{
			return !IsPotion(a_Item.m_ItemType);
		}
		const auto EnchantmentField = (a_Item.m_ItemType == E_ITEM_ENCHANTED_BOOK) ? "StoredEnchantments" : "Enchantments";
		const std::initializer_list<std::string_view> Fields
		{
			"RepairCost", "display", EnchantmentField, "Damage", "map", "Potion", "Fireworks", "Explosion",
			"BlockEntityTag", "BlockStateTag", LegacyBlockFallbacks::ItemIdentityTag,
		};
		if (!HasOnlyFields(a_NBT, a_Tag, Fields))
		{
			return false;
		}
		if (a_Item.IsBeeHome())
		{
			if (!BeeNBT::ReadItemData(a_Item.m_BeeHomeData, a_NBT, a_Tag))
			{
				return false;
			}
		}
		else if ((a_NBT.FindChildByName(a_Tag, "BlockEntityTag") >= 0) || (a_NBT.FindChildByName(a_Tag, "BlockStateTag") >= 0))
		{
			return false;
		}
		const auto RepairCost = a_NBT.FindChildByName(a_Tag, "RepairCost");
		if (RepairCost >= 0)
		{
			if (a_NBT.GetType(RepairCost) != TAG_Int)
			{
				return false;
			}
			a_Item.m_RepairCost = a_NBT.GetInt(RepairCost);
		}
		const auto Damage = a_NBT.FindChildByName(a_Tag, "Damage");
		if (Damage >= 0)
		{
			if ((a_NBT.GetType(Damage) != TAG_Int) || (a_NBT.GetInt(Damage) < 0) || (a_NBT.GetInt(Damage) > std::numeric_limits<short>::max()))
			{
				return false;
			}
			if (HasDurability(a_Item))
			{
				a_Item.m_ItemDamage = static_cast<short>(a_NBT.GetInt(Damage));
			}
			else if (a_NBT.GetInt(Damage) != 0)
			{
				return false;
			}
		}
		const auto Map = a_NBT.FindChildByName(a_Tag, "map");
		if (Map >= 0)
		{
			if (
				(a_Item.m_ItemType != E_ITEM_MAP) || (a_NBT.GetType(Map) != TAG_Int) ||
				(a_NBT.GetInt(Map) < 0) || (a_NBT.GetInt(Map) > std::numeric_limits<short>::max())
			)
			{
				return false;
			}
			a_Item.m_ItemDamage = static_cast<short>(a_NBT.GetInt(Map));
		}
		const auto Potion = a_NBT.FindChildByName(a_Tag, "Potion");
		if (IsPotion(a_Item.m_ItemType) ? !ReadPotion(a_Item, a_NBT, Potion) : (Potion >= 0))
		{
			return false;
		}
		const auto Display = a_NBT.FindChildByName(a_Tag, "display");
		const auto Enchantments = a_NBT.FindChildByName(a_Tag, EnchantmentField);
		const auto Rocket = a_NBT.FindChildByName(a_Tag, "Fireworks");
		const auto Star = a_NBT.FindChildByName(a_Tag, "Explosion");
		return
			((Display < 0) || ReadDisplay(a_Item, a_NBT, Display, a_DataVersion)) &&
			((Enchantments < 0) || ReadPalettedEnchantments(a_Item, a_NBT, Enchantments)) &&
			((Rocket < 0) || ((a_Item.m_ItemType == E_ITEM_FIREWORK_ROCKET) && ReadRocket(a_Item.m_FireworkItem, a_NBT, Rocket))) &&
			((Star < 0) || ((a_Item.m_ItemType == E_ITEM_FIREWORK_STAR) && ReadExplosion(a_Item.m_FireworkItem, a_NBT, Star)));
	}

	void WritePalettedEnchantments(cFastNBTWriter & a_Writer, const cItem & a_Item)
	{
		a_Writer.BeginList((a_Item.m_ItemType == E_ITEM_ENCHANTED_BOOK) ? "StoredEnchantments" : "Enchantments", TAG_Compound);
		for (const auto & [ID, Level] : a_Item.m_Enchantments)
		{
			const auto It = std::find_if(std::begin(PalettedEnchantments), std::end(PalettedEnchantments), [ID](const auto & a_Entry)
			{
				return a_Entry.first == ID;
			});
			if ((It == std::end(PalettedEnchantments)) || (Level > static_cast<unsigned int>(std::numeric_limits<short>::max())))
			{
				throw std::runtime_error(fmt::format(FMT_STRING("Cannot write enchantment {} of level {} to a Java item"), ID, Level));
			}
			a_Writer.BeginCompound("");
			a_Writer.AddString("id", It->second);
			a_Writer.AddShort("lvl", static_cast<Int16>(Level));
			a_Writer.EndCompound();
		}
		a_Writer.EndList();
	}

	void WriteDisplay(cFastNBTWriter & a_Writer, const cItem & a_Item, int a_DataVersion)
	{
		a_Writer.BeginCompound("display");
		if (!a_Item.m_CustomName.empty())
		{
			a_Writer.AddString("Name", JsonUtils::SerializeSingleValueJsonObject("text", a_Item.m_CustomName));
		}
		if (!a_Item.m_LoreTable.empty())
		{
			a_Writer.BeginList("Lore", TAG_String);
			for (const auto & Line : a_Item.m_LoreTable)
			{
				a_Writer.AddString("", (a_DataVersion >= 1952) ? JsonUtils::SerializeSingleValueJsonObject("text", Line) : Line);
			}
			a_Writer.EndList();
		}
		if (a_Item.m_ItemColor.IsValid())
		{
			a_Writer.AddInt("color", static_cast<Int32>(a_Item.m_ItemColor.m_Color));
		}
		a_Writer.EndCompound();
	}

	std::string_view GetPotionName(short a_Damage)
	{
		const auto Effect = a_Damage & ~0x6000;
		const auto It = std::find_if(std::begin(PalettedPotions), std::end(PalettedPotions), [Effect](const auto & a_Entry)
		{
			return a_Entry.first == Effect;
		});
		if (It == std::end(PalettedPotions))
		{
			throw std::runtime_error(fmt::format(FMT_STRING("Cannot write potion with legacy damage {}"), a_Damage));
		}
		return It->second;
	}
}





bool PalettedItemNBT::Read(cItem & a_Item, const cParsedNBT & a_NBT, int a_Tag, int a_DataVersion)
{
	if (!HasOnlyFields(a_NBT, a_Tag, {"id", "Count", "Slot", "tag"}))
	{
		return false;
	}
	if (a_NBT.GetFirstChild(a_Tag) < 0)
	{
		a_Item.Empty();
		return true;
	}

	const auto ID = a_NBT.FindChildByName(a_Tag, "id");
	const auto Count = a_NBT.FindChildByName(a_Tag, "Count");
	const auto Slot = a_NBT.FindChildByName(a_Tag, "Slot");
	const auto Tag = a_NBT.FindChildByName(a_Tag, "tag");
	if (
		(ID < 0) || (a_NBT.GetType(ID) != TAG_String) || (Count < 0) || (a_NBT.GetType(Count) != TAG_Byte) ||
		((Slot >= 0) && (a_NBT.GetType(Slot) != TAG_Byte)) || (a_NBT.GetByte(Count) > 127)
	)
	{
		return false;
	}

	const auto Name = a_NBT.GetStringView(ID);
	if ((Name == "minecraft:air") || (a_NBT.GetByte(Count) == 0))
	{
		if (Tag >= 0)
		{
			return false;
		}
		a_Item.Empty();
		return true;
	}
	const auto Version = GetItemPaletteVersion(a_DataVersion);
	const auto & Registry = ItemRegistry(Version.Column);
	const auto Entry = Registry.ByName.find(Name);
	if (Entry == Registry.ByName.end())
	{
		return false;
	}
	const auto Named = Version.GetItem(Entry->second);
	const auto [Type, Meta] = PaletteUpgrade::ToItem(Named);
	if ((Type <= 0) || (PaletteUpgrade::FromItem(Type, Meta) != Named))
	{
		return false;
	}
	cItem ItemStack(Type, static_cast<char>(a_NBT.GetByte(Count)), Meta);
	// A vanilla upgrade may keep the original, already valid substitute item.
	if (
		(Tag >= 0) && (a_NBT.GetType(Tag) == TAG_Compound) &&
		(a_NBT.FindChildByName(Tag, LegacyBlockFallbacks::ItemIdentityTag) >= 0) &&
		!LegacyBlockFallbacks::RestoreItemIdentity(cProtocol::Version::v1_12, ItemStack, a_NBT, Tag)
	)
	{
		return false;
	}
	if (ItemStack.IsEmpty() || !ReadMetadata(ItemStack, a_NBT, Tag, a_DataVersion))
	{
		return false;
	}
	a_Item = std::move(ItemStack);
	return true;
}





void PalettedItemNBT::Write(cFastNBTWriter & a_Writer, const cItem & a_Item, int a_Slot, const AString & a_Name, int a_DataVersion)
{
	if (a_Item.IsEmpty())
	{
		a_Writer.BeginCompound(a_Name);
		a_Writer.EndCompound();
		return;
	}
	const auto Version = GetItemPaletteVersion(a_DataVersion);
	const auto & Registry = ItemRegistry(Version.Column);

	auto Type = a_Item.m_ItemType;
	const bool Potion = IsPotion(Type);
	if ((Type == E_ITEM_POTION) && ((a_Item.m_ItemDamage & 0x4000) != 0))
	{
		Type = E_ITEM_SPLASH_POTION;  // The legacy engine can represent thrown potions with a damage bit.
	}
	const bool Durability = HasDurability(a_Item);
	const bool Map = Type == E_ITEM_MAP;
	const auto Meta = static_cast<short>((Durability || Potion || Map) ? 0 : a_Item.m_ItemDamage);
	const auto Stored = LegacyBlockFallbacks::ForItem(Version.ProtocolVersion, Type, Meta);
	const bool IsFallback = Stored.first != Type;
	const auto Named = PaletteUpgrade::FromItem(Stored.first, Stored.second);
	const auto ID = Version.GetProtocolID(Named);
	const auto Entry = Registry.ByID.find(ID);
	if (
		(ID == 0) || (Entry == Registry.ByID.end()) || (Version.GetItem(ID) != Named) ||
		(PaletteUpgrade::ToItem(Named) != Stored)
	)
	{
		throw std::runtime_error(fmt::format(FMT_STRING("Cannot write legacy item {}:{} in Java DataVersion {}"), a_Item.m_ItemType, a_Item.m_ItemDamage, a_DataVersion));
	}
	if ((a_Slot > 255) || (static_cast<unsigned char>(a_Item.m_ItemCount) > 127) || ((Durability || Map) && (a_Item.m_ItemDamage < 0)))
	{
		throw std::runtime_error("Invalid slot, count, or damage for a Java item stack");
	}

	a_Writer.BeginCompound(a_Name);
	a_Writer.AddString("id", Entry->second);
	a_Writer.AddByte("Count", static_cast<unsigned char>(a_Item.m_ItemCount));
	if (a_Slot >= 0)
	{
		a_Writer.AddByte("Slot", static_cast<unsigned char>(a_Slot));
	}
	const bool Display = !a_Item.IsBothNameAndLoreEmpty() || a_Item.m_ItemColor.IsValid();
	const bool Firework = ((Type == E_ITEM_FIREWORK_ROCKET) || (Type == E_ITEM_FIREWORK_STAR)) && !a_Item.m_FireworkItem.IsEqualTo(cFireworkItem{});
	if (
		IsFallback || !a_Item.m_BeeHomeData.empty() || Potion || Map || (Durability && (a_Item.m_ItemDamage != 0)) ||
		Display || Firework || !a_Item.m_Enchantments.IsEmpty() || (a_Item.m_RepairCost != 0)
	)
	{
		a_Writer.BeginCompound("tag");
		if (IsFallback)
		{
			LegacyBlockFallbacks::WriteItemIdentity(a_Writer, a_Item);
		}
		BeeNBT::WriteItemData(a_Writer, a_Item.m_BeeHomeData);
		if (Potion)
		{
			a_Writer.AddString("Potion", GetPotionName(a_Item.m_ItemDamage));
		}
		if (Map)
		{
			a_Writer.AddInt("map", a_Item.m_ItemDamage);
		}
		if (Durability && (a_Item.m_ItemDamage != 0))
		{
			a_Writer.AddInt("Damage", a_Item.m_ItemDamage);
		}
		if (Display)
		{
			WriteDisplay(a_Writer, a_Item, a_DataVersion);
		}
		if (Firework)
		{
			cFireworkItem::WriteToNBTCompound(a_Item.m_FireworkItem, a_Writer, static_cast<ENUM_ITEM_TYPE>(Type));
		}
		if (!a_Item.m_Enchantments.IsEmpty())
		{
			WritePalettedEnchantments(a_Writer, a_Item);
		}
		if (a_Item.m_RepairCost != 0)
		{
			a_Writer.AddInt("RepairCost", a_Item.m_RepairCost);
		}
		a_Writer.EndCompound();
	}
	a_Writer.EndCompound();
}
