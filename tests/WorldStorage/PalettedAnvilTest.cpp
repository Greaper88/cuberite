#include "Globals.h"
#include "../TestHelpers.h"
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
}





IMPLEMENT_TEST_MAIN("PalettedAnvilTest",
	CheckChunkVersions();
	CheckSectionWriter();
	CheckInvalidChunkData();
)
