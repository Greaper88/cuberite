#pragma once

#include "ChunkDef.h"





class cFastNBTWriter;





/** Chunk NBT used by Java releases 1.13 through 1.15.2. */
namespace PalettedAnvil
{
	struct Block
	{
		AString Name;
		AStringMap Properties;
	};

	struct Section
	{
		std::vector<Block> Palette;
		std::array<UInt16, cChunkDef::Width * cChunkDef::Width * cChunkDef::SectionHeight> Indices{};
	};

	/** Biome array size for a supported release, or zero for an unsupported data version. */
	size_t BiomeCount(int a_DataVersion);

	/** Writes a terrain section's Palette and BlockStates to an open NBT compound. */
	void WriteBlocks(cFastNBTWriter & a_Writer, const Section & a_Section);

	/** A chunk read from uncompressed Anvil NBT. Keeps original tags outside the terrain palette. */
	class Chunk
	{
	public:
		explicit Chunk(ContiguousByteBufferView a_NBT);

		int GetDataVersion() const { return m_DataVersion; }
		cChunkCoords GetCoords() const { return m_Coords; }

		/** Returns 256 column biomes in Java 1.13 through 1.14, or 1024 quart biomes in 1.15.
		An empty array indicates that the generator had not yet written the biomes. */
		const std::vector<Int32> & GetBiomes() const { return m_Biomes; }

		/** Returns terrain at the section Y, or nullptr for a missing or light-only section. */
		const Section * GetSection(size_t a_SectionY) const;

		/** Writes the chunk in the same data version, preserving other NBT byte for byte. */
		ContiguousByteBuffer Encode() const;

	private:
		ContiguousByteBuffer m_OriginalNBT;
		int m_DataVersion = 0;
		cChunkCoords m_Coords{0, 0};
		std::vector<Int32> m_Biomes;
		std::map<size_t, Section> m_Sections;
	};
}
