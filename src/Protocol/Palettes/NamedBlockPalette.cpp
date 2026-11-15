#include "Globals.h"
#include "NamedBlockPalette.h"
#include "BlockType.h"
#include "OSSupport/File.h"

#include <sstream>





namespace
{
	constexpr UInt32 NumLegacyStates = 256 * 16;
	using cLegacyStates = std::array<std::optional<cNamedBlockState>, NumLegacyStates>;
	using cWireStates = std::map<AStringMap, UInt32>;
	using cWirePalette = std::map<AString, cWireStates, std::less<>>;

	bool EndsWith(std::string_view a_Text, std::string_view a_Suffix)
	{
		return (a_Text.size() >= a_Suffix.size()) && (a_Text.substr(a_Text.size() - a_Suffix.size()) == a_Suffix);
	}

	std::string_view CanonicalName(std::string_view a_Name)
	{
		// The upgrade table predates the final names in the flattened registries.
		static constexpr std::pair<std::string_view, std::string_view> Aliases[] =
		{
			{"minecraft:acacia_bark", "minecraft:acacia_wood"},
			{"minecraft:birch_bark", "minecraft:birch_wood"},
			{"minecraft:dark_oak_bark", "minecraft:dark_oak_wood"},
			{"minecraft:jungle_bark", "minecraft:jungle_wood"},
			{"minecraft:oak_bark", "minecraft:oak_wood"},
			{"minecraft:spruce_bark", "minecraft:spruce_wood"},
			{"minecraft:melon_block", "minecraft:melon"},
			{"minecraft:mob_spawner", "minecraft:spawner"},
			{"minecraft:portal", "minecraft:nether_portal"},
			{"minecraft:sign", "minecraft:oak_sign"},
			{"minecraft:wall_sign", "minecraft:oak_wall_sign"},
		};
		const auto Alias = std::find_if(std::begin(Aliases), std::end(Aliases), [a_Name](const auto & a_Entry)
		{
			return a_Entry.first == a_Name;
		});
		return (Alias == std::end(Aliases)) ? a_Name : Alias->second;
	}

	std::string_view ReleaseName(UInt32 a_Protocol, std::string_view a_Name)
	{
		const auto Name = CanonicalName(a_Name);
		if (a_Protocol <= 404)
		{
			if (Name == "minecraft:oak_sign")
			{
				return "minecraft:sign";
			}
			if (Name == "minecraft:oak_wall_sign")
			{
				return "minecraft:wall_sign";
			}
		}
		return Name;
	}

	template <typename Callback>
	void ReadPaletteRows(const AString & a_Path, bool a_Upgrade, Callback && a_Row)
	{
		std::istringstream File(cFile::ReadWholeFile(a_Path));
		const auto ReadLine = [&File](AString & a_Line)
		{
			if (!std::getline(File, a_Line))
			{
				return false;
			}
			if (!a_Line.empty() && (a_Line.back() == '\r'))
			{
				a_Line.pop_back();
			}
			return true;
		};

		AString Line;
		if (
			!ReadLine(Line) || (Line != (a_Upgrade ? "UpgradeBlockTypePalette" : "BlockTypePalette")) ||
			!ReadLine(Line) || (Line != "FileVersion\t1") ||
			!ReadLine(Line) || (Line != "CommonPrefix\tminecraft:") ||
			!ReadLine(Line) || !Line.empty()
		)
		{
			throw std::runtime_error(fmt::format(FMT_STRING("Missing or invalid block palette {}"), a_Path));
		}

		const size_t NameColumn = a_Upgrade ? 2 : 1;
		while (ReadLine(Line))
		{
			const auto Fields = StringSplit(Line, "\t");
			UInt32 ID;
			if (
				(Fields.size() <= NameColumn) || ((Fields.size() - NameColumn) % 2 != 1) ||
				!StringToInteger(Fields[0], ID)
			)
			{
				throw std::runtime_error(fmt::format(FMT_STRING("Invalid block palette row in {}"), a_Path));
			}
			if (a_Upgrade)
			{
				UInt32 Meta;
				if ((ID >= 256) || !StringToInteger(Fields[1], Meta) || (Meta >= 16))
				{
					throw std::runtime_error(fmt::format(FMT_STRING("Invalid legacy block state in {}"), a_Path));
				}
				ID = (ID << 4) | Meta;
			}
			cNamedBlockState State{"minecraft:" + Fields[NameColumn], {}};
			for (size_t Index = NameColumn + 1; Index < Fields.size(); Index += 2)
			{
				State.Properties.emplace(Fields[Index], Fields[Index + 1]);
			}
			a_Row(ID, std::move(State));
		}
	}

	struct cUpgradePalette
	{
		cLegacyStates States;
		std::map<AString, std::vector<UInt32>, std::less<>> LegacyByName;

		cUpgradePalette()
		{
			ReadPaletteRows("Protocol/UpgradeBlockTypePalette.txt", true, [&](UInt32 a_ID, cNamedBlockState a_State)
			{
				if (a_State.Name == "minecraft:%%FILTER_ME%%")
				{
					// The legacy skull stores its variant and rotation in a separate block entity.
					// Terrain without that information uses the default skeleton skull.
					const auto Facing = a_State.Properties.at("facing");
					if ((Facing == "down") || (Facing == "up"))
					{
						a_State = {"minecraft:skeleton_skull", {{"rotation", "0"}}};
					}
					else
					{
						a_State = {"minecraft:skeleton_wall_skull", {{"facing", Facing}}};
					}
				}
				a_State.Name = AString(CanonicalName(a_State.Name));
				const auto Decayable = a_State.Properties.find("decayable");
				if (Decayable != a_State.Properties.end())
				{
					a_State.Properties.emplace("persistent", (Decayable->second == "true") ? "false" : "true");
					a_State.Properties.erase(Decayable);
					a_State.Properties.erase("check_decay");
				}
				LegacyByName[a_State.Name].push_back(a_ID);
				States[a_ID].emplace(std::move(a_State));
			});
		}
	};

	const cUpgradePalette & UpgradePalette()
	{
		static const cUpgradePalette Palette;
		return Palette;
	}

	UInt32 Shift1131BlockState(UInt32 a_ID)
	{
		// The 1.13.1 and 1.13.2 registries inserted states in three locations.
		// Blocks that changed their state properties are replaced by the overlay.
		if (a_ID >= 8574)
		{
			return a_ID + 17;
		}
		if (a_ID >= 8464)
		{
			return a_ID + 16;
		}
		return (a_ID >= 1127) ? (a_ID + 1) : a_ID;
	}

	cWirePalette ReadWirePalette(const AString & a_Path, const AString & a_Changes = {}, bool a_Shift1131 = false)
	{
		cWirePalette Palette;
		ReadPaletteRows(a_Path, false, [&](UInt32 a_ID, cNamedBlockState a_State)
		{
			Palette[std::move(a_State.Name)].emplace(std::move(a_State.Properties), a_Shift1131 ? Shift1131BlockState(a_ID) : a_ID);
		});
		if (!a_Changes.empty())
		{
			// An overlay supplies every state of each block it changes.
			cWirePalette Changes;
			ReadPaletteRows(a_Changes, false, [&](UInt32 a_ID, cNamedBlockState a_State)
			{
				Changes[std::move(a_State.Name)].emplace(std::move(a_State.Properties), a_ID);
			});
			for (auto & [Name, States] : Changes)
			{
				Palette.insert_or_assign(Name, std::move(States));
			}
		}
		return Palette;
	}

	std::optional<cWirePalette> TryReadWirePalette(const AString & a_Path, const AString & a_Changes = {}, bool a_Shift1131 = false)
	{
		try
		{
			return ReadWirePalette(a_Path, a_Changes, a_Shift1131);
		}
		catch (const std::runtime_error & Error)
		{
			// Clients can still use the built-in legacy protocol mappings if an asset is missing.
			LOGWARNING("Named block palette unavailable. %s", Error.what());
			return std::nullopt;
		}
	}

	const cWirePalette * WirePalette(UInt32 a_Protocol)
	{
		switch (a_Protocol)
		{
			case 393:
			{
				static const auto Palette = TryReadWirePalette("Protocol/1.13/base.btp.txt");
				return Palette.has_value() ? &*Palette : nullptr;
			}
			case 401:
			case 404:
			{
				static const auto Palette = TryReadWirePalette("Protocol/1.13/base.btp.txt", "Protocol/1.13.2/changes.btp.txt", true);
				return Palette.has_value() ? &*Palette : nullptr;
			}
			case 477:
			case 480:
			case 485:
			case 490:
			case 498:
			{
				static const auto Palette = TryReadWirePalette("Protocol/1.14.4/base.btp.txt");
				return Palette.has_value() ? &*Palette : nullptr;
			}
			case 578:
			{
				static const auto Palette = TryReadWirePalette("Protocol/1.14.4/base.btp.txt", "Protocol/1.15.2/changes.btp.txt");
				return Palette.has_value() ? &*Palette : nullptr;
			}
			default:
			{
				return nullptr;
			}
		}
	}

	std::string_view ProjectionName(std::string_view a_Name)
	{
		if (a_Name.substr(0, 10) != "minecraft:")
		{
			return a_Name;
		}
		// These variants share one numeric block ID. Their rotation or orientation still matters.
		if (EndsWith(a_Name, "_wall_sign"))
		{
			return "minecraft:oak_wall_sign";
		}
		if (EndsWith(a_Name, "_sign"))
		{
			return "minecraft:oak_sign";
		}
		if (EndsWith(a_Name, "_bed"))
		{
			return "minecraft:red_bed";
		}
		if (EndsWith(a_Name, "_wall_head") || EndsWith(a_Name, "_wall_skull"))
		{
			return "minecraft:skeleton_wall_skull";
		}
		if (EndsWith(a_Name, "_head") || EndsWith(a_Name, "_skull"))
		{
			return "minecraft:skeleton_skull";
		}
		if (EndsWith(a_Name, "_wall_banner"))
		{
			return "minecraft:white_wall_banner";
		}
		if (EndsWith(a_Name, "_banner"))
		{
			return "minecraft:white_banner";
		}
		return CanonicalName(a_Name);
	}

	int ProjectionCost(const cNamedBlockState & a_State, const AStringMap & a_Candidate)
	{
		const auto Half = a_State.Properties.find("half");
		const bool Door = EndsWith(a_State.Name, "_door");
		const bool UpperDoor = Door && (Half != a_State.Properties.end()) && (Half->second == "upper");
		int Cost = 0;
		for (const auto & [Name, Value] : a_Candidate)
		{
			// Legacy door halves encode disjoint properties.
			if (Door && (UpperDoor ? ((Name == "facing") || (Name == "open")) : ((Name == "hinge") || (Name == "powered"))))
			{
				continue;
			}
			const auto Actual = a_State.Properties.find(Name);
			if ((Actual != a_State.Properties.end()) && (Actual->second != Value))
			{
				Cost += (Name == "half") ? 16 : 1;
			}
		}
		return Cost;
	}

	std::pair<BLOCKTYPE, NIBBLETYPE> FallbackProjection(const cNamedBlockState & a_State)
	{
		const auto & Name = a_State.Name;
		if ((Name == "minecraft:cave_air") || (Name == "minecraft:void_air"))
		{
			return {E_BLOCK_AIR, 0};
		}
		if ((Name == "minecraft:bubble_column") || (Name == "minecraft:kelp") || (Name == "minecraft:kelp_plant") ||
			(Name == "minecraft:seagrass") || (Name == "minecraft:tall_seagrass"))
		{
			return {E_BLOCK_STATIONARY_WATER, 0};
		}
		if (Name == "minecraft:blue_ice")
		{
			return {E_BLOCK_PACKED_ICE, 0};
		}
		if (Name.compare(0, 17, "minecraft:potted_") == 0)
		{
			return {E_BLOCK_FLOWER_POT, 0};
		}
		return {E_BLOCK_STONE, 0};
	}

	std::string_view DefaultProperty(std::string_view a_Name)
	{
		if ((a_Name == "waterlogged") || (a_Name == "powered"))
		{
			return "false";
		}
		if (a_Name == "distance")
		{
			return "7";
		}
		if (a_Name == "axis")
		{
			return "y";
		}
		if (a_Name == "note")
		{
			return "0";
		}
		if (a_Name == "instrument")
		{
			return "harp";
		}
		return {};
	}

	const AStringMap & CompleteProperties(const AStringMap & a_Properties, const cWireStates & a_Choices)
	{
		const auto Exact = a_Choices.find(a_Properties);
		if (Exact != a_Choices.end())
		{
			return Exact->first;
		}
		const AStringMap * Best = nullptr;
		int BestCost = std::numeric_limits<int>::max();
		for (const auto & [Properties, ID] : a_Choices)
		{
			UNUSED(ID);
			int Cost = 0;
			for (const auto & [Name, Value] : Properties)
			{
				const auto Desired = a_Properties.find(Name);
				if (Desired != a_Properties.end())
				{
					Cost += (Desired->second == Value) ? 0 : 16;
				}
				else
				{
					const auto Default = DefaultProperty(Name);
					Cost += (!Default.empty() && (Default != Value)) ? 1 : 0;
				}
			}
			if (Cost < BestCost)
			{
				Best = &Properties;
				BestCost = Cost;
			}
		}
		ASSERT(Best != nullptr);
		return *Best;
	}

	cLegacyStates MakeSavePalette(UInt32 a_Protocol)
	{
		const auto Protocol = WirePalette(a_Protocol);
		if (Protocol == nullptr)
		{
			throw std::runtime_error(fmt::format(FMT_STRING("Missing block palette for Java protocol {}"), a_Protocol));
		}
		const auto & Source = UpgradePalette().States;
		cLegacyStates Result;
		for (size_t ID = 0; ID < Source.size(); ++ID)
		{
			if (!Source[ID].has_value())
			{
				continue;
			}
			const auto & State = *Source[ID];
			const auto Found = Protocol->find(ReleaseName(a_Protocol, State.Name));
			if (Found != Protocol->end())
			{
				Result[ID].emplace(cNamedBlockState{Found->first, CompleteProperties(State.Properties, Found->second)});
			}
		}
		return Result;
	}

	const cLegacyStates & SavePalette(int a_DataVersion)
	{
		switch (a_DataVersion)
		{
			case 1519:  // 1.13
			{
				static const auto Palette = MakeSavePalette(393);
				return Palette;
			}
			case 1628:  // 1.13.1
			case 1631:  // 1.13.2
			{
				static const auto Palette = MakeSavePalette(404);
				return Palette;
			}
			case 1952:  // 1.14
			case 1957:
			case 1963:
			case 1968:
			case 1976:  // 1.14.4
			{
				static const auto Palette = MakeSavePalette(498);
				return Palette;
			}
			case 2225:  // 1.15
			case 2227:
			case 2230:  // 1.15.2
			{
				static const auto Palette = MakeSavePalette(578);
				return Palette;
			}
			default:
			{
				throw std::runtime_error(fmt::format(FMT_STRING("No block palette for Java DataVersion {}"), a_DataVersion));
			}
		}
	}
}





std::pair<BLOCKTYPE, NIBBLETYPE> NamedBlockPalette::ToLegacy(const cNamedBlockState & a_State)
{
	const auto & Palette = UpgradePalette();
	const auto Candidates = Palette.LegacyByName.find(ProjectionName(a_State.Name));
	if (Candidates == Palette.LegacyByName.end())
	{
		return FallbackProjection(a_State);
	}

	UInt32 Best = Candidates->second.front();
	int BestCost = std::numeric_limits<int>::max();
	for (const auto ID : Candidates->second)
	{
		const auto Cost = ProjectionCost(a_State, Palette.States[ID]->Properties);
		if (Cost < BestCost)
		{
			Best = ID;
			BestCost = Cost;
		}
	}
	const auto Type = static_cast<BLOCKTYPE>(Best >> 4);
	const auto Meta = static_cast<NIBBLETYPE>(Best & 15);
	if (Meta == 0)
	{
		if (Type == E_BLOCK_WATER)
		{
			return {E_BLOCK_STATIONARY_WATER, 0};
		}
		if (Type == E_BLOCK_LAVA)
		{
			return {E_BLOCK_STATIONARY_LAVA, 0};
		}
	}
	return {Type, Meta};
}





const cNamedBlockState & NamedBlockPalette::FromLegacy(int a_DataVersion, BLOCKTYPE a_Type, NIBBLETYPE a_Meta)
{
	const auto & Palette = SavePalette(a_DataVersion);
	const auto First = static_cast<UInt32>(a_Type) << 4;
	const auto & Exact = Palette[First | (a_Meta & 15)];
	if (Exact.has_value())
	{
		return *Exact;
	}
	for (UInt32 Index = First; Index < First + 16; ++Index)
	{
		if (Palette[Index].has_value())
		{
			return *Palette[Index];
		}
	}
	throw std::runtime_error(fmt::format(FMT_STRING("Block {}:{} has no palette entry for Java DataVersion {}"), a_Type, a_Meta, a_DataVersion));
}





std::optional<UInt32> NamedBlockPalette::ToProtocol(UInt32 a_Protocol, const cNamedBlockState & a_State)
{
	const auto Palette = WirePalette(a_Protocol);
	if (Palette == nullptr)
	{
		return std::nullopt;
	}
	const auto Found = Palette->find(ReleaseName(a_Protocol, a_State.Name));
	if (Found == Palette->end())
	{
		return std::nullopt;
	}
	const auto State = Found->second.find(a_State.Properties);
	return (State == Found->second.end()) ? std::nullopt : std::optional<UInt32>(State->second);
}
