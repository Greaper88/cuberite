#include "Globals.h"
#include "PalettedAnvil.h"
#include "ChunkData.h"
#include "FastNBT.h"





namespace
{
	// Java 1.13 reads the light arrays directly. Java 1.14 introduced the lighting engine.
	constexpr int LastInlineLightingDataVersion = 1631;

	int RequireTag(const cParsedNBT & a_NBT, int a_Parent, const AString & a_Name, eTagType a_Type)
	{
		const auto Tag = a_NBT.FindChildByName(a_Parent, a_Name);
		if ((Tag < 0) || (a_NBT.GetType(Tag) != a_Type))
		{
			throw std::runtime_error(fmt::format(FMT_STRING("Missing or invalid Anvil tag: {}"), a_Name));
		}
		return Tag;
	}

	size_t IndexWidth(size_t a_PaletteSize)
	{
		size_t Width = 4;
		while ((size_t{1} << Width) < a_PaletteSize)
		{
			++Width;
		}
		return Width;
	}

	PalettedAnvil::Block ReadBlock(const cParsedNBT & a_NBT, int a_Tag)
	{
		PalettedAnvil::Block Block;
		Block.Name = a_NBT.GetString(RequireTag(a_NBT, a_Tag, "Name", TAG_String));

		const auto Properties = a_NBT.FindChildByName(a_Tag, "Properties");
		if (Properties >= 0)
		{
			if (a_NBT.GetType(Properties) != TAG_Compound)
			{
				throw std::runtime_error("Invalid Anvil block Properties compound");
			}
			for (int Tag = a_NBT.GetFirstChild(Properties); Tag >= 0; Tag = a_NBT.GetNextSibling(Tag))
			{
				if ((a_NBT.GetType(Tag) != TAG_String) || !Block.Properties.emplace(a_NBT.GetName(Tag), a_NBT.GetString(Tag)).second)
				{
					throw std::runtime_error("Invalid or duplicate Anvil block property");
				}
			}
		}
		return Block;
	}

	PalettedAnvil::Section ReadBlocks(const cParsedNBT & a_NBT, int a_Section)
	{
		const auto PaletteTag = RequireTag(a_NBT, a_Section, "Palette", TAG_List);
		const auto StatesTag = RequireTag(a_NBT, a_Section, "BlockStates", TAG_LongArray);
		PalettedAnvil::Section Section;
		if (a_NBT.GetChildrenType(PaletteTag) != TAG_Compound)
		{
			throw std::runtime_error("Expected a nonempty list of Anvil palette compounds");
		}
		for (int Entry = a_NBT.GetFirstChild(PaletteTag); Entry >= 0; Entry = a_NBT.GetNextSibling(Entry))
		{
			if (Section.Palette.size() == Section.Indices.size())
			{
				throw std::runtime_error("Anvil palette has more entries than blocks in a section");
			}
			Section.Palette.push_back(ReadBlock(a_NBT, Entry));
		}

		const auto DataLength = a_NBT.GetDataLength(StatesTag);
		const auto BytesPerBit = Section.Indices.size() / 8;
		const auto Width = DataLength / BytesPerBit;
		if ((DataLength % BytesPerBit != 0) || (Width < IndexWidth(Section.Palette.size())) || (Width > 32))
		{
			throw std::runtime_error("Invalid Anvil BlockStates array length");
		}

		std::vector<UInt64> Words(DataLength / sizeof(UInt64));
		const auto Data = a_NBT.GetData(StatesTag);
		for (size_t Index = 0; Index < Words.size(); ++Index)
		{
			Words[Index] = NetworkBufToHost<UInt64>(Data + Index * sizeof(UInt64));
		}

		// Indices start at the low bit of each word and can cross a word boundary.
		const auto Mask = (UInt64{1} << Width) - 1;
		for (size_t Index = 0; Index < Section.Indices.size(); ++Index)
		{
			const auto Bit = Index * Width;
			const auto WordIndex = Bit / 64;
			const auto Shift = Bit % 64;
			auto Value = Words[WordIndex] >> Shift;
			if (Shift + Width > 64)
			{
				Value |= Words[WordIndex + 1] << (64 - Shift);
			}
			Value &= Mask;
			if (Value >= Section.Palette.size())
			{
				throw std::runtime_error("Anvil block index is outside its palette");
			}
			Section.Indices[Index] = static_cast<UInt16>(Value);
		}
		return Section;
	}

	std::vector<UInt64> PackIndices(const PalettedAnvil::Section & a_Section)
	{
		if (a_Section.Palette.empty() || (a_Section.Palette.size() > a_Section.Indices.size()))
		{
			throw std::runtime_error("Invalid Anvil palette size");
		}
		const auto Width = IndexWidth(a_Section.Palette.size());
		std::vector<UInt64> Words(a_Section.Indices.size() * Width / 64);
		for (size_t Index = 0; Index < a_Section.Indices.size(); ++Index)
		{
			const UInt64 Value = a_Section.Indices[Index];
			if (Value >= a_Section.Palette.size())
			{
				throw std::runtime_error("Anvil block index is outside its palette");
			}
			const auto Bit = Index * Width;
			const auto WordIndex = Bit / 64;
			const auto Shift = Bit % 64;
			Words[WordIndex] |= Value << Shift;
			if (Shift + Width > 64)
			{
				Words[WordIndex + 1] |= Value >> (64 - Shift);
			}
		}
		return Words;
	}

	void WriteBlock(cFastNBTWriter & a_Writer, const PalettedAnvil::Block & a_Block)
	{
		a_Writer.BeginCompound("");
		a_Writer.AddString("Name", a_Block.Name);
		if (!a_Block.Properties.empty())
		{
			a_Writer.BeginCompound("Properties");
			for (const auto & [Name, Value] : a_Block.Properties)
			{
				a_Writer.AddString(Name, Value);
			}
			a_Writer.EndCompound();
		}
		a_Writer.EndCompound();
	}

	void RewritePalette(
		cFastNBTWriter & a_Writer,
		const cParsedNBT & a_Source,
		int a_SourcePalette,
		const PalettedAnvil::Section * a_Original,
		const PalettedAnvil::Section & a_Replacement
	)
	{
		const auto Words = PackIndices(a_Replacement);
		const auto Compare = [](const PalettedAnvil::Block * a_Left, const PalettedAnvil::Block * a_Right)
		{
			return std::tie(a_Left->Name, a_Left->Properties) < std::tie(a_Right->Name, a_Right->Properties);
		};
		std::map<const PalettedAnvil::Block *, int, decltype(Compare)> OriginalEntries(Compare);
		if (a_Original != nullptr)
		{
			auto Tag = a_Source.GetFirstChild(a_SourcePalette);
			for (const auto & Block : a_Original->Palette)
			{
				OriginalEntries.try_emplace(&Block, Tag);
				Tag = a_Source.GetNextSibling(Tag);
			}
		}

		a_Writer.BeginList("Palette", TAG_Compound);
		for (const auto & Block : a_Replacement.Palette)
		{
			const auto Original = OriginalEntries.find(&Block);
			if (Original == OriginalEntries.end())
			{
				WriteBlock(a_Writer, Block);
			}
			else
			{
				a_Writer.CopyTag(a_Source, Original->second);
			}
		}
		a_Writer.EndList();
		a_Writer.AddLongArray("BlockStates", Words.data(), Words.size());
	}

	void WriteLightArray(
		cFastNBTWriter & a_Writer,
		const AString & a_Name,
		const ChunkLightData::LightArray * a_Values,
		unsigned char a_Default,
		const cParsedNBT * a_Source = nullptr,
		int a_SectionTag = -1
	)
	{
		const auto SourceTag = (a_Source == nullptr) ? -1 : a_Source->FindChildByName(a_SectionTag, a_Name);
		if ((a_Source != nullptr) && (SourceTag >= 0) && (a_Source->GetType(SourceTag) == TAG_ByteArray) &&
			(a_Source->GetDataLength(SourceTag) == ChunkLightData::SectionLightCount))
		{
			a_Writer.CopyTag(*a_Source, SourceTag);
		}
		else if (a_Values != nullptr)
		{
			a_Writer.AddByteArray(a_Name, reinterpret_cast<const char *>(a_Values->data()), a_Values->size());
		}
		else
		{
			a_Writer.AddByteArray(a_Name, ChunkLightData::SectionLightCount, a_Default);
		}
	}

	void WriteInlineLight(
		cFastNBTWriter & a_Writer,
		size_t a_Y,
		const ChunkLightData * a_LightData,
		const cParsedNBT * a_Source = nullptr,
		int a_SectionTag = -1
	)
	{
		if (a_LightData != nullptr)
		{
			WriteLightArray(a_Writer, "BlockLight", a_LightData->GetBlockLightSection(a_Y), ChunkLightData::DefaultBlockLightValue);
			WriteLightArray(a_Writer, "SkyLight", a_LightData->GetSkyLightSection(a_Y), ChunkLightData::DefaultSkyLightValue);
		}
		else
		{
			WriteLightArray(a_Writer, "BlockLight", nullptr, ChunkLightData::DefaultBlockLightValue, a_Source, a_SectionTag);
			WriteLightArray(a_Writer, "SkyLight", nullptr, ChunkLightData::DefaultSkyLightValue, a_Source, a_SectionTag);
		}
	}

	void WriteNewSection(
		cFastNBTWriter & a_Writer,
		size_t a_Y,
		const PalettedAnvil::Section & a_Section,
		bool a_InlineLighting,
		const ChunkLightData * a_LightData
	)
	{
		a_Writer.BeginCompound("");
		a_Writer.AddByte("Y", static_cast<unsigned char>(a_Y));
		PalettedAnvil::WriteBlocks(a_Writer, a_Section);
		if (a_InlineLighting)
		{
			WriteInlineLight(a_Writer, a_Y, a_LightData);
		}
		a_Writer.EndCompound();
	}

	void CopyOtherHeightmaps(cFastNBTWriter & a_Writer, const cParsedNBT & a_Source, int a_Heightmaps)
	{
		constexpr std::array<std::string_view, 7> DerivedHeightmaps
		{
			"WORLD_SURFACE_WG", "OCEAN_FLOOR_WG", "LIGHT_BLOCKING", "MOTION_BLOCKING",
			"MOTION_BLOCKING_NO_LEAVES", "OCEAN_FLOOR", "WORLD_SURFACE",
		};
		a_Writer.BeginCompound("Heightmaps");
		for (int Tag = a_Source.GetFirstChild(a_Heightmaps); Tag >= 0; Tag = a_Source.GetNextSibling(Tag))
		{
			const auto Name = a_Source.GetName(Tag);
			if (std::find(DerivedHeightmaps.begin(), DerivedHeightmaps.end(), Name) == DerivedHeightmaps.end())
			{
				a_Writer.CopyTag(a_Source, Tag);
			}
		}
		a_Writer.EndCompound();
	}
}





size_t PalettedAnvil::BiomeCount(int a_DataVersion)
{
	switch (a_DataVersion)
	{
		case 1519:
		case 1628:
		case 1631:
		case 1952:
		case 1957:
		case 1963:
		case 1968:
		case 1976:
		{
			return 256;
		}
		case 2225:
		case 2227:
		case 2230:
		{
			return 1024;
		}
		default:
		{
			return 0;
		}
	}
}





void PalettedAnvil::WriteBlocks(cFastNBTWriter & a_Writer, const Section & a_Section)
{
	const auto Words = PackIndices(a_Section);

	a_Writer.BeginList("Palette", TAG_Compound);
	for (const auto & Block : a_Section.Palette)
	{
		WriteBlock(a_Writer, Block);
	}
	a_Writer.EndList();
	a_Writer.AddLongArray("BlockStates", Words.data(), Words.size());
}





ContiguousByteBuffer PalettedAnvil::WriteChunk(
	int a_DataVersion,
	cChunkCoords a_Coords,
	const std::map<size_t, Section> & a_Sections,
	const std::vector<Int32> & a_Biomes,
	Int64 a_LastUpdate,
	const ChunkLightData * a_LightData
)
{
	const auto ExpectedBiomes = BiomeCount(a_DataVersion);
	if ((ExpectedBiomes == 0) || (a_Biomes.size() != ExpectedBiomes))
	{
		throw std::runtime_error("Cannot write an Anvil chunk with an unsupported data version or biome array");
	}
	const auto InlineLighting = (a_DataVersion <= LastInlineLightingDataVersion);

	cFastNBTWriter Writer;
	Writer.AddInt("DataVersion", a_DataVersion);
	Writer.BeginCompound("Level");
	Writer.AddInt("xPos", a_Coords.m_ChunkX);
	Writer.AddInt("zPos", a_Coords.m_ChunkZ);
	Writer.AddLong("LastUpdate", a_LastUpdate);
	Writer.AddLong("InhabitedTime", 0);
	Writer.AddString("Status", InlineLighting ? "postprocessed" : "full");
	Writer.AddIntArray("Biomes", a_Biomes.data(), a_Biomes.size());

	Writer.BeginList("Sections", TAG_Compound);
	for (const auto & [Y, Section] : a_Sections)
	{
		if (Y >= cChunkDef::NumSections)
		{
			throw std::runtime_error("Anvil terrain section exceeds world height");
		}
		WriteNewSection(Writer, Y, Section, InlineLighting, a_LightData);
	}
	Writer.EndList();

	Writer.BeginList("Entities", TAG_Compound);
	Writer.EndList();
	Writer.BeginList("TileEntities", TAG_Compound);
	Writer.EndList();
	Writer.BeginList("TileTicks", TAG_Compound);
	Writer.EndList();
	Writer.BeginList("LiquidTicks", TAG_Compound);
	Writer.EndList();
	Writer.BeginList("PostProcessing", TAG_List);
	Writer.EndList();

	// Vanilla computes these from the terrain on load.
	Writer.BeginCompound("Heightmaps");
	Writer.EndCompound();
	Writer.BeginCompound("Structures");
	Writer.BeginCompound("Starts");
	Writer.EndCompound();
	Writer.BeginCompound("References");
	Writer.EndCompound();
	Writer.EndCompound();
	Writer.EndCompound();
	Writer.Finish();
	return ContiguousByteBuffer(Writer.GetResult());
}





PalettedAnvil::Chunk::Chunk(ContiguousByteBufferView a_NBT):
	m_OriginalNBT(a_NBT)
{
	const cParsedNBT NBT(m_OriginalNBT);
	if (!NBT.IsValid())
	{
		throw std::runtime_error(fmt::format(FMT_STRING("Invalid Anvil NBT: {}"), NBT.GetErrorCode().message()));
	}
	const auto Root = NBT.GetRoot();
	m_DataVersion = NBT.GetInt(RequireTag(NBT, Root, "DataVersion", TAG_Int));
	const auto ExpectedBiomes = BiomeCount(m_DataVersion);
	if (ExpectedBiomes == 0)
	{
		throw std::runtime_error(fmt::format(FMT_STRING("Unsupported paletted Anvil DataVersion {}"), m_DataVersion));
	}

	const auto Level = RequireTag(NBT, Root, "Level", TAG_Compound);
	m_Coords =
	{
		NBT.GetInt(RequireTag(NBT, Level, "xPos", TAG_Int)),
		NBT.GetInt(RequireTag(NBT, Level, "zPos", TAG_Int)),
	};
	const auto Biomes = NBT.FindChildByName(Level, "Biomes");
	if (Biomes >= 0)
	{
		if ((NBT.GetType(Biomes) != TAG_IntArray) || (NBT.GetDataLength(Biomes) != ExpectedBiomes * sizeof(Int32)))
		{
			throw std::runtime_error("Invalid Anvil Biomes array for this DataVersion");
		}
		m_Biomes.reserve(ExpectedBiomes);
		const auto Data = NBT.GetData(Biomes);
		for (size_t Index = 0; Index < ExpectedBiomes; ++Index)
		{
			m_Biomes.push_back(NetworkBufToHost<Int32>(Data + Index * sizeof(Int32)));
		}
	}

	const auto Sections = RequireTag(NBT, Level, "Sections", TAG_List);
	const auto SectionType = NBT.GetChildrenType(Sections);
	if ((SectionType != TAG_End) && (SectionType != TAG_Compound))
	{
		throw std::runtime_error("Expected Anvil section compounds");
	}

	std::set<unsigned char> FoundY;
	for (int Tag = NBT.GetFirstChild(Sections); Tag >= 0; Tag = NBT.GetNextSibling(Tag))
	{
		const auto Y = NBT.GetByte(RequireTag(NBT, Tag, "Y", TAG_Byte));
		if (!FoundY.insert(Y).second)
		{
			throw std::runtime_error("Duplicate Anvil section Y");
		}
		if ((NBT.FindChildByName(Tag, "Palette") < 0) && (NBT.FindChildByName(Tag, "BlockStates") < 0))
		{
			if (NBT.FindChildByName(Tag, "Blocks") >= 0)
			{
				throw std::runtime_error("Legacy block data in a paletted Anvil chunk");
			}
			// 1.14 and 1.15 can also store light-only sections outside the terrain bounds.
			continue;
		}
		if (Y >= cChunkDef::NumSections)
		{
			throw std::runtime_error("Anvil terrain section exceeds world height");
		}
		m_Sections.emplace(Y, ReadBlocks(NBT, Tag));
	}
}





const PalettedAnvil::Section * PalettedAnvil::Chunk::GetSection(size_t a_SectionY) const
{
	const auto Section = m_Sections.find(a_SectionY);
	return (Section == m_Sections.end()) ? nullptr : &Section->second;
}





ContiguousByteBuffer PalettedAnvil::Chunk::Encode() const
{
	return m_OriginalNBT;
}





ContiguousByteBuffer PalettedAnvil::Chunk::Encode(
	const std::map<size_t, Section> & a_Changes,
	const ChunkLightData * a_LightData
) const
{
	if (a_Changes.empty())
	{
		return m_OriginalNBT;
	}
	if (a_Changes.rbegin()->first >= cChunkDef::NumSections)
	{
		throw std::runtime_error("Anvil terrain section exceeds world height");
	}

	const cParsedNBT NBT(m_OriginalNBT);
	const auto Root = NBT.GetRoot();
	const auto Level = NBT.FindChildByName(Root, "Level");
	const auto Sections = NBT.FindChildByName(Level, "Sections");
	const auto InlineLighting = (m_DataVersion <= LastInlineLightingDataVersion);

	cFastNBTWriter Writer(NBT.GetName(Root));
	for (int Tag = NBT.GetFirstChild(Root); Tag >= 0; Tag = NBT.GetNextSibling(Tag))
	{
		if (Tag != Level)
		{
			Writer.CopyTag(NBT, Tag);
			continue;
		}

		Writer.BeginCompound("Level");
		for (int Child = NBT.GetFirstChild(Level); Child >= 0; Child = NBT.GetNextSibling(Child))
		{
			const auto Name = NBT.GetName(Child);
			if ((Name == "isLightOn") && !InlineLighting)
			{
				continue;
			}
			if ((Name == "Heightmaps") && (NBT.GetType(Child) == TAG_Compound))
			{
				CopyOtherHeightmaps(Writer, NBT, Child);
				continue;
			}
			if (Child != Sections)
			{
				Writer.CopyTag(NBT, Child);
				continue;
			}

			Writer.BeginList("Sections", TAG_Compound);
			std::array<bool, cChunkDef::NumSections> Replaced{};
			for (int Entry = NBT.GetFirstChild(Sections); Entry >= 0; Entry = NBT.GetNextSibling(Entry))
			{
				const auto Y = NBT.GetByte(NBT.FindChildByName(Entry, "Y"));
				const auto Existing = GetSection(Y);
				const auto Replacement = a_Changes.find(Y);
				const auto ReplaceTerrain = (Replacement != a_Changes.end());
				const auto ReplaceLighting = InlineLighting && (ReplaceTerrain || ((Existing != nullptr) && (a_LightData != nullptr)));
				if (!ReplaceTerrain && !ReplaceLighting)
				{
					Writer.CopyTag(NBT, Entry);
					continue;
				}

				Writer.BeginCompound("");
				for (int Field = NBT.GetFirstChild(Entry); Field >= 0; Field = NBT.GetNextSibling(Field))
				{
					const auto FieldName = NBT.GetName(Field);
					if (
						(ReplaceTerrain && ((FieldName == "Palette") || (FieldName == "BlockStates"))) ||
						(ReplaceLighting && ((FieldName == "BlockLight") || (FieldName == "SkyLight")))
					)
					{
						continue;
					}
					Writer.CopyTag(NBT, Field);
				}
				if (ReplaceTerrain)
				{
					RewritePalette(Writer, NBT, NBT.FindChildByName(Entry, "Palette"), Existing, Replacement->second);
					Replaced[Y] = true;
				}
				if (ReplaceLighting)
				{
					WriteInlineLight(Writer, Y, a_LightData, &NBT, Entry);
				}
				Writer.EndCompound();
			}
			for (const auto & [Y, Section] : a_Changes)
			{
				if (!Replaced[Y])
				{
					WriteNewSection(Writer, Y, Section, InlineLighting, a_LightData);
				}
			}
			Writer.EndList();
		}
		Writer.EndCompound();
	}
	Writer.Finish();
	return ContiguousByteBuffer(Writer.GetResult());
}
