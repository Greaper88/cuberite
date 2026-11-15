
#include "Globals.h"
#include "../TestHelpers.h"
#include "ChunkData.h"
#include "BlockType.h"





/** Helper that copies a data store into a contiguous flat array, filling in a default value for sections that aren't present. */
template <class StoreType, typename GetType, typename DefaultType, typename OutType>
static void CopyAll(const StoreType & Data, GetType Getter, DefaultType Default, OutType & Out)
{
	constexpr auto SectionCount = std::extent_v<OutType> / 16;

	for (size_t Y = 0; Y != 16; Y++)
	{
		const auto Section = (Data.*Getter)(Y);
		static_assert(SectionCount == std::tuple_size<std::remove_pointer_t<decltype(Section)>>::value, "Output array has wrong size");

		if (Section == nullptr)
		{
			std::fill_n(Out + Y * SectionCount, SectionCount, Default);
		}
		else
		{
			std::copy(Section->begin(), Section->end(), Out + Y * SectionCount);
		}
	}
}





/** Performs the entire Copies test. */
static void Test()
{
	LOGD("Test started");

	{
		ChunkBlockData buffer;

		buffer.SetBlock({ 3, 1, 4 }, 0xDE);
		buffer.SetMeta({ 3, 1, 4 }, 0xA);

		ChunkBlockData copy;
		copy.Assign(buffer);
		TEST_EQUAL(copy.GetBlock({ 3, 1, 4 }), 0xDE);
		TEST_EQUAL(copy.GetMeta({ 3, 1, 4 }), 0xA);

		BLOCKTYPE SrcBlockBuffer[16 * 16 * 256];
		NIBBLETYPE SrcNibbleBuffer[16 * 16 * 256 / 2]{};
		for (int i = 0; i < 16 * 16 * 256; i += 4)
		{
			SrcBlockBuffer[i + 0] = 0xde;
			SrcBlockBuffer[i + 1] = 0xad;
			SrcBlockBuffer[i + 2] = 0xbe;
			SrcBlockBuffer[i + 3] = 0xef;
		}

		buffer.SetAll(SrcBlockBuffer, SrcNibbleBuffer);
		BLOCKTYPE DstBlockBuffer[16 * 16 * 256];
		CopyAll(buffer, &ChunkBlockData::GetSection, ChunkBlockData::DefaultValue, DstBlockBuffer);
		TEST_EQUAL(memcmp(SrcBlockBuffer, DstBlockBuffer, (16 * 16 * 256) - 1), 0);

		memset(SrcBlockBuffer, 0x00, 16 * 16 * 256);
		buffer.SetAll(SrcBlockBuffer, SrcNibbleBuffer);
		CopyAll(buffer, &ChunkBlockData::GetSection, ChunkBlockData::DefaultValue, DstBlockBuffer);
		TEST_EQUAL(memcmp(SrcBlockBuffer, DstBlockBuffer, (16 * 16 * 256) - 1), 0);
	}

	{
		ChunkBlockData buffer;

		BLOCKTYPE SrcBlockBuffer[16 * 16 * 256]{};
		NIBBLETYPE SrcNibbleBuffer[16 * 16 * 256 / 2];
		for (int i = 0; i < 16 * 16 * 256 / 2; i += 4)
		{
			SrcNibbleBuffer[i + 0] = 0xde;
			SrcNibbleBuffer[i + 1] = 0xad;
			SrcNibbleBuffer[i + 2] = 0xbe;
			SrcNibbleBuffer[i + 3] = 0xef;
		}

		buffer.SetAll(SrcBlockBuffer, SrcNibbleBuffer);
		NIBBLETYPE DstNibbleBuffer[16 * 16 * 256/ 2];
		CopyAll(buffer, &ChunkBlockData::GetMetaSection, ChunkBlockData::DefaultMetaValue, DstNibbleBuffer);
		TEST_EQUAL(memcmp(SrcNibbleBuffer, DstNibbleBuffer, (16 * 16 * 256 / 2) - 1), 0);

		memset(SrcNibbleBuffer, 0x00, 16 * 16 * 256 /2);
		buffer.SetAll(SrcBlockBuffer, SrcNibbleBuffer);
		CopyAll(buffer, &ChunkBlockData::GetMetaSection, ChunkBlockData::DefaultMetaValue, DstNibbleBuffer);
		TEST_EQUAL(memcmp(SrcNibbleBuffer, DstNibbleBuffer, (16 * 16 * 256 / 2) - 1), 0);
	}

	{
		ChunkLightData buffer;

		NIBBLETYPE SrcNibbleBuffer[16 * 16 * 256 / 2];
		for (int i = 0; i < 16 * 16 * 256 / 2; i += 4)
		{
			SrcNibbleBuffer[i + 0] = 0xde;
			SrcNibbleBuffer[i + 1] = 0xad;
			SrcNibbleBuffer[i + 2] = 0xbe;
			SrcNibbleBuffer[i + 3] = 0xef;
		}

		buffer.SetAll(SrcNibbleBuffer, SrcNibbleBuffer);
		NIBBLETYPE DstNibbleBuffer[16 * 16 * 256 / 2];
		CopyAll(buffer, &ChunkLightData::GetBlockLightSection, ChunkLightData::DefaultBlockLightValue, DstNibbleBuffer);
		TEST_EQUAL(memcmp(SrcNibbleBuffer, DstNibbleBuffer, (16 * 16 * 256 /2) - 1), 0);

		memset(SrcNibbleBuffer, 0x00, 16 * 16 * 256 /2);
		buffer.SetAll(SrcNibbleBuffer, SrcNibbleBuffer);
		CopyAll(buffer, &ChunkLightData::GetBlockLightSection, ChunkLightData::DefaultBlockLightValue, DstNibbleBuffer);
		TEST_EQUAL(memcmp(SrcNibbleBuffer, DstNibbleBuffer, (16 * 16 * 256 /2) - 1), 0);
	}

	{
		ChunkLightData buffer;

		NIBBLETYPE SrcNibbleBuffer[16 * 16 * 256 / 2];
		for (int i = 0; i < 16 * 16 * 256 / 2; i += 4)
		{
			SrcNibbleBuffer[i + 0] = 0xde;
			SrcNibbleBuffer[i + 1] = 0xad;
			SrcNibbleBuffer[i + 2] = 0xbe;
			SrcNibbleBuffer[i + 3] = 0xef;
		}

		buffer.SetAll(SrcNibbleBuffer, SrcNibbleBuffer);
		NIBBLETYPE DstNibbleBuffer[16 * 16 * 256/ 2];
		CopyAll(buffer, &ChunkLightData::GetSkyLightSection, ChunkLightData::DefaultSkyLightValue, DstNibbleBuffer);
		TEST_EQUAL(memcmp(SrcNibbleBuffer, DstNibbleBuffer, (16 * 16 * 256 / 2) - 1), 0);

		memset(SrcNibbleBuffer, 0xFF, 16 * 16 * 256 / 2);
		buffer.SetAll(SrcNibbleBuffer, SrcNibbleBuffer);
		CopyAll(buffer, &ChunkLightData::GetSkyLightSection, ChunkLightData::DefaultSkyLightValue, DstNibbleBuffer);
		TEST_EQUAL(memcmp(SrcNibbleBuffer, DstNibbleBuffer, (16 * 16 * 256 / 2) - 1), 0);
	}

	{
		ChunkBlockData buffer;

		BLOCKTYPE SrcBlockBuffer[16 * 16 * 256];
		memset(SrcBlockBuffer, 0x00, 16 * 16 * 256);
		BLOCKTYPE DstBlockBuffer[16 * 16 * 256];
		CopyAll(buffer, &ChunkBlockData::GetSection, ChunkBlockData::DefaultValue, DstBlockBuffer);
		TEST_EQUAL(memcmp(SrcBlockBuffer, DstBlockBuffer, (16 * 16 * 256) - 1), 0);

		NIBBLETYPE SrcNibbleBuffer[16 * 16 * 256 / 2];
		memset(SrcNibbleBuffer, 0x00, 16 * 16 * 256 / 2);
		NIBBLETYPE DstNibbleBuffer[16 * 16 * 256 / 2];
		CopyAll(buffer, &ChunkBlockData::GetMetaSection, ChunkBlockData::DefaultMetaValue, DstNibbleBuffer);
		TEST_EQUAL(memcmp(SrcNibbleBuffer, DstNibbleBuffer, (16 * 16 * 256 / 2) - 1), 0);
	}

	{
		ChunkLightData buffer;

		NIBBLETYPE SrcNibbleBuffer[16 * 16 * 256 / 2];
		memset(SrcNibbleBuffer, 0x00, 16 * 16 * 256 / 2);
		NIBBLETYPE DstNibbleBuffer[16 * 16 * 256 / 2];
		CopyAll(buffer, &ChunkLightData::GetBlockLightSection, ChunkLightData::DefaultBlockLightValue, DstNibbleBuffer);
		TEST_EQUAL(memcmp(SrcNibbleBuffer, DstNibbleBuffer, (16 * 16 * 256 / 2) - 1), 0);

		memset(SrcNibbleBuffer, 0xFF, 16 * 16 * 256 / 2);
		CopyAll(buffer, &ChunkLightData::GetSkyLightSection, ChunkLightData::DefaultSkyLightValue, DstNibbleBuffer);
		TEST_EQUAL(memcmp(SrcNibbleBuffer, DstNibbleBuffer, (16 * 16 * 256 / 2) - 1), 0);
	}
}





static void TestNamedStates()
{
	ChunkBlockData Data;
	const ChunkLightData Light;
	const size_t SectionY = 4;
	const Vector3i Stairs{3, 65, 4};
	const Vector3i Neighbor{4, 65, 4};
	const auto StairsIndex = cChunkDef::MakeIndex(3, 1, 4);
	const cNamedBlockState CaveAir{"minecraft:cave_air", {}};
	const cNamedBlockState WetStairs{"minecraft:oak_stairs", {{"waterlogged", "true"}, {"facing", "west"}}};

	// The section contains names even though the entire numeric projection is air.
	ChunkBlockData::NamedIndices Indices{};
	Data.SetNamedSection(SectionY, {CaveAir}, Indices);
	TEST_EQUAL(Data.HasNamedSection(SectionY), true);
	TEST_EQUAL(Data.GetSection(SectionY), nullptr);
	TEST_EQUAL(*Data.GetNamedBlock(Neighbor), CaveAir);
	TEST_EQUAL(Data.GetNamedBlock(SectionY - 1, StairsIndex), nullptr);
	size_t SectionCount = 0;
	ChunkDef_ForEachSection(Data, Light,
	{
		TEST_EQUAL(Y, SectionY);
		++SectionCount;
	});
	TEST_EQUAL(SectionCount, 1u);

	Data.SetBlock(Stairs, E_BLOCK_WOODEN_STAIRS);
	Data.SetMeta(Stairs, 1);
	Indices[StairsIndex] = 1;
	Data.SetNamedSection(SectionY, {CaveAir, WetStairs}, Indices);
	TEST_EQUAL(*Data.GetNamedBlock(SectionY, StairsIndex), WetStairs);

	// Legacy no-op writes retain the exact state.
	Data.SetBlock(Stairs, E_BLOCK_WOODEN_STAIRS);
	Data.SetMeta(Stairs, 1);
	TEST_EQUAL(*Data.GetNamedBlock(Stairs), WetStairs);

	// Copies preserve names and are independent of later mutations.
	ChunkBlockData Copy;
	Copy.Assign(Data);
	Copy.Assign(Copy);
	Data.SetMeta(Stairs, 2);
	TEST_EQUAL(Data.GetNamedBlock(Stairs), nullptr);
	TEST_EQUAL(*Data.GetNamedBlock(Neighbor), CaveAir);
	TEST_EQUAL(Copy.GetMeta(Stairs), 1);
	TEST_EQUAL(*Copy.GetNamedBlock(Stairs), WetStairs);

	// Bulk writes invalidate only the positions whose projected type or metadata changes.
	cChunkDef::BlockTypes Blocks{};
	cChunkDef::BlockNibbles Metas{};
	Blocks[cChunkDef::MakeIndex(Stairs)] = E_BLOCK_WOODEN_STAIRS;
	cChunkDef::PackNibble(Metas, cChunkDef::MakeIndex(Stairs), 1);
	Blocks[cChunkDef::MakeIndex(Neighbor)] = E_BLOCK_STONE;
	Copy.SetAll(Blocks, Metas);
	TEST_EQUAL(*Copy.GetNamedBlock(Stairs), WetStairs);
	TEST_EQUAL(Copy.GetNamedBlock(Neighbor), nullptr);
	TEST_EQUAL(Copy.GetBlock(Neighbor), E_BLOCK_STONE);

	ChunkBlockData::SectionType BlockSection;
	std::fill(std::begin(BlockSection), std::end(BlockSection), E_BLOCK_DIRT);
	ChunkBlockData::SectionMetaType MetaSection{};
	Copy.SetSection(BlockSection, MetaSection, SectionY);
	TEST_EQUAL(Copy.HasNamedSection(SectionY), false);
}





IMPLEMENT_TEST_MAIN("ChunkData Copies",
	Test();
	TestNamedStates();
)
