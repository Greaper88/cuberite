#include "Globals.h"
#include "../TestHelpers.h"
#include "ChunkData.h"
#include "WorldStorage/FastNBT.h"
#include "WorldStorage/PalettedAnvil.h"





namespace
{
	/** Writes the fixture with explicit packed words, independent of the codec under test. */
	ContiguousByteBuffer MakeFixture(
		int a_DataVersion,
		size_t a_BiomeCount,
		const std::vector<UInt64> & a_Words,
		bool a_DuplicateSection = false
	)
	{
		cFastNBTWriter Writer;
		Writer.AddInt("DataVersion", a_DataVersion);
		Writer.BeginCompound("Level");
		Writer.AddInt("xPos", -13);
		Writer.AddInt("zPos", 19);
		Writer.AddLong("InhabitedTime", 242);
		Writer.AddString("Status", "full");
		Writer.AddByte("isLightOn", 1);
		Writer.BeginCompound("Heightmaps");
		const UInt64 Height = 234;
		Writer.AddLongArray("WORLD_SURFACE", &Height, 1);
		Writer.AddInt("TestModHeightmap", 24);
		Writer.EndCompound();
		std::vector<Int32> Biomes(a_BiomeCount, 1);
		Biomes.back() = 132;
		Writer.AddIntArray("Biomes", Biomes.data(), Biomes.size());

		Writer.BeginList("TileEntities", TAG_Compound);
		Writer.BeginCompound("");
		Writer.AddString("id", "minecraft:beehive");
		Writer.BeginList("Bees", TAG_Compound);
		Writer.BeginCompound("");
		Writer.AddShort("TicksInHive", 75);
		Writer.EndCompound();
		Writer.EndList();
		Writer.EndCompound();
		Writer.EndList();

		Writer.BeginList("Sections", TAG_Compound);
		Writer.BeginCompound("");
		Writer.AddByte("Y", 3);
		Writer.AddInt("TestModSection", 12);
		Writer.BeginList("Palette", TAG_Compound);
		Writer.BeginCompound("");
		Writer.AddString("Name", "minecraft:air");
		Writer.EndCompound();
		for (size_t Index = 1; Index < 16; ++Index)
		{
			Writer.BeginCompound("");
			Writer.AddString("Name", fmt::format(FMT_STRING("fixture:block_{}"), Index));
			Writer.EndCompound();
		}
		Writer.BeginCompound("");
		Writer.AddString("Name", "minecraft:oak_stairs");
		Writer.AddInt("TestModPaletteEntry", 17);
		Writer.BeginCompound("Properties");
		Writer.AddString("waterlogged", "true");
		Writer.AddString("shape", "inner_left");
		Writer.AddString("half", "bottom");
		Writer.AddString("facing", "east");
		Writer.EndCompound();
		Writer.EndCompound();
		Writer.EndList();
		Writer.AddLongArray("BlockStates", a_Words.data(), a_Words.size());
		Writer.AddByteArray("SkyLight", 2048, 0xff);
		Writer.EndCompound();

		Writer.BeginCompound("");
		Writer.AddByte("Y", a_DuplicateSection ? 3 : 255);
		Writer.AddByteArray("BlockLight", 2048, 0);
		Writer.EndCompound();
		Writer.EndList();
		Writer.EndCompound();
		Writer.Finish();
		return ContiguousByteBuffer(Writer.GetResult());
	}

	void CheckChunkVersions()
	{
		const std::array<int, 11> Versions{1519, 1628, 1631, 1952, 1957, 1963, 1968, 1976, 2225, 2227, 2230};
		for (const auto Version : Versions)
		{
			std::vector<UInt64> Words(320);
			// Index 12 begins at bit 60 and spills into word 1. Index 13 begins at bit 65.
			Words[1] = 15;
			Words.back() = UInt64{3} << 59;
			const auto BiomeCount = (Version < 2225) ? 256u : 1024u;
			const auto Fixture = MakeFixture(Version, BiomeCount, Words);
			const PalettedAnvil::Chunk Chunk(Fixture);

			TEST_EQUAL(Chunk.GetDataVersion(), Version);
			TEST_EQUAL(Chunk.GetCoords(), cChunkCoords(-13, 19));
			TEST_EQUAL(Chunk.GetBiomes().size(), BiomeCount);
			TEST_EQUAL(Chunk.GetBiomes().back(), 132);
			TEST_EQUAL(Chunk.GetSection(0), nullptr);
			TEST_EQUAL(Chunk.GetSection(255), nullptr);
			const auto Section = Chunk.GetSection(3);
			TEST_NOTEQUAL(Section, nullptr);
			TEST_EQUAL(Section->Palette.size(), 17u);
			TEST_EQUAL(Section->Palette[16].Name, "minecraft:oak_stairs");
			TEST_EQUAL(Section->Palette[16].Properties.at("waterlogged"), "true");
			TEST_EQUAL(Section->Palette[7].Name, "fixture:block_7");
			TEST_EQUAL(Section->Indices[12], 16);
			TEST_EQUAL(Section->Indices[13], 7);
			TEST_EQUAL(Section->Indices.back(), 3);

			// Preserve every byte of entities, lighting, and other NBT across the rewrite.
			TEST_EQUAL(Chunk.Encode(), Fixture);
		}
	}

	void CheckSectionWriter()
	{
		PalettedAnvil::Section Section;
		Section.Palette.push_back({"minecraft:bee_nest", {{"honey_level", "5"}, {"facing", "east"}}});
		cFastNBTWriter Writer;
		PalettedAnvil::WriteBlocks(Writer, Section);
		Writer.Finish();

		const cParsedNBT NBT(Writer.GetResult());
		TEST_TRUE(NBT.IsValid());
		const auto State = NBT.GetFirstChild(NBT.FindChildByName(0, "Palette"));
		TEST_EQUAL(NBT.GetString(NBT.FindChildByName(State, "Name")), "minecraft:bee_nest");
		const auto Properties = NBT.FindChildByName(State, "Properties");
		TEST_EQUAL(NBT.GetString(NBT.FindChildByName(Properties, "honey_level")), "5");
		const auto States = NBT.FindChildByName(0, "BlockStates");
		TEST_EQUAL(NBT.GetDataLength(States), 2048u);
		for (size_t Index = 0; Index < NBT.GetDataLength(States); ++Index)
		{
			TEST_EQUAL(NBT.GetData(States)[Index], std::byte{});
		}
	}

	void CheckInvalidChunkData()
	{
		std::vector<UInt64> Words(320);
		TEST_THROWS(static_cast<void>(PalettedAnvil::Chunk{MakeFixture(1343, 256, Words)}), std::runtime_error);
		TEST_THROWS(static_cast<void>(PalettedAnvil::Chunk{MakeFixture(2586, 1024, Words)}), std::runtime_error);
		TEST_THROWS(static_cast<void>(PalettedAnvil::Chunk{MakeFixture(2230, 256, Words)}), std::runtime_error);
		TEST_THROWS(static_cast<void>(PalettedAnvil::Chunk{MakeFixture(2230, 1024, Words, true)}), std::runtime_error);

		Words[0] = 31;  // Does not fit in a palette of 17 entries.
		TEST_THROWS(static_cast<void>(PalettedAnvil::Chunk{MakeFixture(2230, 1024, Words)}), std::runtime_error);

		Words.pop_back();
		TEST_THROWS(static_cast<void>(PalettedAnvil::Chunk{MakeFixture(2230, 1024, Words)}), std::runtime_error);
	}

	void CheckEditedChunk()
	{
		std::vector<UInt64> Words(320);
		Words[1] = 1;
		const auto Fixture = MakeFixture(2230, 1024, Words);
		const PalettedAnvil::Chunk Original(Fixture);
		const auto OriginalSection = Original.GetSection(3);
		TEST_NOTEQUAL(OriginalSection, nullptr);

		auto Changed = *OriginalSection;
		Changed.Palette.push_back({"minecraft:bee_nest", {{"honey_level", "5"}, {"facing", "east"}}});
		Changed.Indices[13] = 17;
		std::map<size_t, PalettedAnvil::Section> Changes;
		Changes.emplace(3, Changed);
		Changes.emplace(4, Changed);

		const auto Encoded = Original.Encode(Changes);
		const PalettedAnvil::Chunk Edited(Encoded);
		TEST_EQUAL(Edited.GetDataVersion(), Original.GetDataVersion());
		TEST_EQUAL(Edited.GetCoords(), Original.GetCoords());
		TEST_EQUAL(Edited.GetBiomes(), Original.GetBiomes());
		for (const auto Y : {3u, 4u})
		{
			const auto Section = Edited.GetSection(Y);
			TEST_NOTEQUAL(Section, nullptr);
			TEST_EQUAL(Section->Indices[12], 16);
			TEST_EQUAL(Section->Indices[13], 17);
			TEST_EQUAL(Section->Palette[17].Properties.at("honey_level"), "5");
		}

		const cParsedNBT NBT(Encoded);
		const cParsedNBT OldNBT(Fixture);
		const auto Level = NBT.FindChildByName(0, "Level");
		const auto OldLevel = OldNBT.FindChildByName(0, "Level");
		TEST_EQUAL(
			NBT.GetTagPayload(NBT.FindChildByName(Level, "TileEntities")),
			OldNBT.GetTagPayload(OldNBT.FindChildByName(OldLevel, "TileEntities"))
		);

		TEST_EQUAL(NBT.FindChildByName(Level, "isLightOn"), -1);
		const auto Heightmaps = NBT.FindChildByName(Level, "Heightmaps");
		TEST_EQUAL(NBT.FindChildByName(Heightmaps, "WORLD_SURFACE"), -1);
		TEST_EQUAL(NBT.GetInt(NBT.FindChildByName(Heightmaps, "TestModHeightmap")), 24);
		const auto Sections = NBT.FindChildByName(Level, "Sections");
		const auto OldSections = OldNBT.FindChildByName(OldLevel, "Sections");
		const auto Section = NBT.GetFirstChild(Sections);
		const auto OldSection = OldNBT.GetFirstChild(OldSections);
		TEST_EQUAL(NBT.GetInt(NBT.FindChildByName(Section, "TestModSection")), 12);
		TEST_EQUAL(NBT.GetTagPayload(NBT.GetNextSibling(Section)), OldNBT.GetTagPayload(OldNBT.GetNextSibling(OldSection)));

		auto PaletteEntry = NBT.GetFirstChild(NBT.FindChildByName(Section, "Palette"));
		for (int Index = 0; Index < 16; ++Index)
		{
			PaletteEntry = NBT.GetNextSibling(PaletteEntry);
		}
		TEST_EQUAL(NBT.GetInt(NBT.FindChildByName(PaletteEntry, "TestModPaletteEntry")), 17);
	}

	void CheckGeneratedChunk()
	{
		PalettedAnvil::Section Stone;
		Stone.Palette.push_back({"minecraft:stone", {}});
		const std::map<size_t, PalettedAnvil::Section> Sections{{3, Stone}};
		const cChunkCoords Coords(-15, 29);
		ChunkLightData Lighting;
		ChunkLightData::SectionType BlockLight{};
		ChunkLightData::SectionType SkyLight{};
		BlockLight[0] = 0x87;
		SkyLight[0] = 0xfa;
		Lighting.SetSection(BlockLight, SkyLight, 3);
		for (const auto Version : {1519, 1976, 2230})
		{
			const auto BiomeCount = PalettedAnvil::BiomeCount(Version);
			const std::vector<Int32> Biomes(BiomeCount, 1);
			const auto Encoded = PalettedAnvil::WriteChunk(Version, Coords, Sections, Biomes, 356, &Lighting);
			const PalettedAnvil::Chunk Chunk(Encoded);
			TEST_EQUAL(Version, Chunk.GetDataVersion());
			TEST_EQUAL(Coords, Chunk.GetCoords());
			TEST_EQUAL(Biomes, Chunk.GetBiomes());
			const auto Terrain = Chunk.GetSection(3);
			TEST_NOTEQUAL(Terrain, nullptr);
			TEST_EQUAL(Terrain->Palette[0].Name, "minecraft:stone");

			const cParsedNBT NBT(Encoded);
			const auto Level = NBT.FindChildByName(0, "Level");
			TEST_EQUAL(NBT.GetLong(NBT.FindChildByName(Level, "LastUpdate")), 356);
			const auto Section = NBT.GetFirstChild(NBT.FindChildByName(Level, "Sections"));
			const auto BlockLightTag = NBT.FindChildByName(Section, "BlockLight");
			const auto SkyLightTag = NBT.FindChildByName(Section, "SkyLight");
			const auto Status = NBT.GetString(NBT.FindChildByName(Level, "Status"));
			if (Version == 1519)
			{
				TEST_EQUAL(Status, "postprocessed");
				TEST_EQUAL(NBT.GetDataLength(BlockLightTag), ChunkLightData::SectionLightCount);
				TEST_EQUAL(NBT.GetDataLength(SkyLightTag), ChunkLightData::SectionLightCount);
				TEST_EQUAL(*NBT.GetData(BlockLightTag), std::byte{0x87});
				TEST_EQUAL(*NBT.GetData(SkyLightTag), std::byte{0xfa});
			}
			else
			{
				TEST_EQUAL(Status, "full");
				TEST_EQUAL(BlockLightTag, -1);
				TEST_EQUAL(SkyLightTag, -1);
			}
		}
	}

	void CheckEntityLists()
	{
		cFastNBTWriter EntitiesWriter;
		EntitiesWriter.BeginList("Entities", TAG_Compound);
		EntitiesWriter.BeginCompound("");
		EntitiesWriter.AddString("id", "minecraft:pig");
		EntitiesWriter.AddFloat("Health", 9);
		EntitiesWriter.EndCompound();
		EntitiesWriter.EndList();
		EntitiesWriter.BeginList("TileEntities", TAG_Compound);
		EntitiesWriter.EndList();
		EntitiesWriter.Finish();
		const cParsedNBT EntityLists(EntitiesWriter.GetResult());

		for (const auto Version : {1519, 2230})
		{
			const auto Count = PalettedAnvil::BiomeCount(Version);
			const auto Fixture = MakeFixture(Version, Count, std::vector<UInt64>(320));
			const PalettedAnvil::Chunk Original(Fixture);
			const cParsedNBT OldNBT(Fixture);
			const auto OldLevel = OldNBT.FindChildByName(0, "Level");

			ChunkLightData Lighting;
			const auto Encoded = Original.Encode({}, &Lighting, &EntityLists);
			const cParsedNBT NBT(Encoded);
			TEST_TRUE(NBT.IsValid());
			TEST_EQUAL(NBT.GetInt(NBT.FindChildByName(0, "DataVersion")), Version);
			const auto Level = NBT.FindChildByName(0, "Level");
			for (const char * Name : {"Sections", "isLightOn", "Heightmaps", "Biomes", "InhabitedTime"})
			{
				TEST_EQUAL(
					NBT.GetTagPayload(NBT.FindChildByName(Level, Name)),
					OldNBT.GetTagPayload(OldNBT.FindChildByName(OldLevel, Name))
				);
			}
			for (const char * Name : {"Entities", "TileEntities"})
			{
				const auto Tag = NBT.FindChildByName(Level, Name);
				TEST_NOTEQUAL(Tag, -1);
				TEST_EQUAL(NBT.GetTagPayload(Tag), EntityLists.GetTagPayload(EntityLists.FindChildByName(0, Name)));
			}

			const auto Generated = PalettedAnvil::WriteChunk(
				Version, Original.GetCoords(), {}, std::vector<Int32>(Count, 1), 0, nullptr, &EntityLists
			);
			const cParsedNBT GeneratedNBT(Generated);
			TEST_TRUE(GeneratedNBT.IsValid());
			const auto GeneratedLevel = GeneratedNBT.FindChildByName(0, "Level");
			TEST_EQUAL(
				GeneratedNBT.GetTagPayload(GeneratedNBT.FindChildByName(GeneratedLevel, "Entities")),
				EntityLists.GetTagPayload(EntityLists.FindChildByName(0, "Entities"))
			);
		}

		cFastNBTWriter InvalidWriter;
		InvalidWriter.BeginList("Entities", TAG_Int);
		InvalidWriter.AddInt("", 5);
		InvalidWriter.EndList();
		InvalidWriter.BeginList("TileEntities", TAG_Compound);
		InvalidWriter.EndList();
		InvalidWriter.Finish();
		const cParsedNBT Invalid(InvalidWriter.GetResult());
		const auto Fixture = MakeFixture(2230, 1024, std::vector<UInt64>(320));
		const PalettedAnvil::Chunk Original(Fixture);
		TEST_THROWS(static_cast<void>(Original.Encode({}, nullptr, &Invalid)), std::runtime_error);
	}
}





IMPLEMENT_TEST_MAIN("PalettedAnvilTest",
	CheckChunkVersions();
	CheckSectionWriter();
	CheckInvalidChunkData();
	CheckEditedChunk();
	CheckGeneratedChunk();
	CheckEntityLists();
)
