#include "Globals.h"
#include "WSSPalettedAnvil.h"
#include "PalettedAnvil.h"
#include "../BlockType.h"
#include "../ChunkDataCallback.h"
#include "../Entities/Entity.h"
#include "../Protocol/Palettes/NamedBlockPalette.h"
#include "../SetChunkData.h"
#include "../World.h"





namespace
{
	class cTerrainSnapshot final :
		public cChunkDataCopyCollector
	{
	public:
		cChunkDef::BiomeMap Biomes{};
		bool IsLightValid = false;

	private:
		void BiomeMap(const cChunkDef::BiomeMap & a_Biomes) override
		{
			std::copy(std::begin(a_Biomes), std::end(a_Biomes), Biomes);
		}

		void LightIsValid(bool a_IsValid) override
		{
			IsLightValid = a_IsValid;
		}
	};

	int ReadAnvilDataVersion(const cParsedNBT & a_NBT)
	{
		if (!a_NBT.IsValid())
		{
			throw std::runtime_error(fmt::format(FMT_STRING("Invalid Anvil NBT: {}"), a_NBT.GetErrorCode().message()));
		}
		const auto Tag = a_NBT.FindChildByName(0, "DataVersion");
		if (Tag < 0)
		{
			return 0;
		}
		if (a_NBT.GetType(Tag) != TAG_Int)
		{
			throw std::runtime_error("Invalid Anvil chunk DataVersion tag");
		}
		return a_NBT.GetInt(Tag);
	}

	/** Returns the relative positions of preserved native block entities. */
	std::set<size_t> NativeBlockEntities(const cParsedNBT & a_NBT, cChunkCoords a_Chunk)
	{
		std::set<size_t> Positions;
		const auto Tiles = a_NBT.FindTagByPath(0, "Level/TileEntities");
		if ((Tiles < 0) || (a_NBT.GetType(Tiles) != TAG_List) || (a_NBT.GetChildrenType(Tiles) != TAG_Compound))
		{
			return Positions;
		}
		for (int Tag = a_NBT.GetFirstChild(Tiles); Tag >= 0; Tag = a_NBT.GetNextSibling(Tag))
		{
			const auto X = a_NBT.FindChildByName(Tag, "x");
			const auto Y = a_NBT.FindChildByName(Tag, "y");
			const auto Z = a_NBT.FindChildByName(Tag, "z");
			if (
				(X < 0) || (a_NBT.GetType(X) != TAG_Int) ||
				(Y < 0) || (a_NBT.GetType(Y) != TAG_Int) ||
				(Z < 0) || (a_NBT.GetType(Z) != TAG_Int)
			)
			{
				continue;
			}
			const Vector3i Absolute(a_NBT.GetInt(X), a_NBT.GetInt(Y), a_NBT.GetInt(Z));
			if ((Absolute.y >= 0) && (Absolute.y < cChunkDef::Height) && (cChunkDef::BlockToChunk(Absolute) == a_Chunk))
			{
				Positions.insert(cChunkDef::MakeIndex(cChunkDef::AbsoluteToRelative(Absolute)));
			}
		}
		return Positions;
	}

	void ReadPalettedBiomes(const std::vector<Int32> & a_StoredBiomes, SetChunkData & a_Data)
	{
		std::fill(std::begin(a_Data.BiomeMap), std::end(a_Data.BiomeMap), biPlains);
		if (a_StoredBiomes.empty())
		{
			return;
		}

		for (int Z = 0; Z < cChunkDef::Width; ++Z)
		{
			for (int X = 0; X < cChunkDef::Width; ++X)
			{
				const auto Column = static_cast<size_t>(X + Z * cChunkDef::Width);
				size_t BiomeIndex = Column;
				if (a_StoredBiomes.size() == 1024)
				{
					// The legacy simulation needs one biome per column. Sample near its surface.
					const auto CellY = static_cast<size_t>(a_Data.HeightMap[Column] / 4);
					BiomeIndex = static_cast<size_t>(X / 4 + Z / 4 * 4) + CellY * 16;
				}
				const auto Biome = a_StoredBiomes[BiomeIndex];
				if ((Biome >= 0) && (Biome <= biMaxVariantBiome))
				{
					a_Data.BiomeMap[Column] = static_cast<EMCSBiome>(Biome);
				}
			}
		}
	}

	std::vector<Int32> WritePalettedBiomes(const cChunkDef::BiomeMap & a_Biomes, int a_DataVersion)
	{
		const auto Count = PalettedAnvil::BiomeCount(a_DataVersion);
		if (Count == 256)
		{
			return {std::begin(a_Biomes), std::end(a_Biomes)};
		}

		std::vector<Int32> Biomes(Count);
		for (size_t Index = 0; Index < Count; ++Index)
		{
			const size_t X = (Index % 4) * 4;
			const size_t Z = ((Index / 4) % 4) * 4;
			Biomes[Index] = static_cast<Int32>(a_Biomes[X + Z * cChunkDef::Width]);
		}
		return Biomes;
	}

	struct cCompareBlocks
	{
		bool operator () (const cNamedBlockState * a_Left, const cNamedBlockState * a_Right) const
		{
			return std::tie(a_Left->Name, a_Left->Properties) < std::tie(a_Right->Name, a_Right->Properties);
		}
	};

	std::map<size_t, PalettedAnvil::Section> CollectTerrainChanges(
		const ChunkBlockData & a_Blocks,
		int a_DataVersion,
		const PalettedAnvil::Chunk * a_Original,
		const std::set<size_t> & a_PreservedTiles
	)
	{
		std::map<size_t, PalettedAnvil::Section> Changes;
		const auto & Air = NamedBlockPalette::FromLegacy(a_DataVersion, E_BLOCK_AIR, 0);
		for (size_t Y = 0; Y < cChunkDef::NumSections; ++Y)
		{
			const auto Blocks = a_Blocks.GetSection(Y);
			const auto Metas = a_Blocks.GetMetaSection(Y);
			const auto Original = (a_Original == nullptr) ? nullptr : a_Original->GetSection(Y);
			if ((Original == nullptr) && (Blocks == nullptr) && (Metas == nullptr) && !a_Blocks.HasNamedSection(Y))
			{
				continue;
			}

			PalettedAnvil::Section Section;
			std::map<const cNamedBlockState *, UInt16, cCompareBlocks> Indices;
			bool Changed = false;
			for (size_t Index = 0; Index < ChunkBlockData::SectionBlockCount; ++Index)
			{
				const auto & OldState = (Original == nullptr) ? Air : Original->Palette[Original->Indices[Index]];
				const auto * State = a_Blocks.GetNamedBlock(Y, Index);
				if (State == nullptr)
				{
					const auto ChunkIndex = Y * ChunkBlockData::SectionBlockCount + Index;
					if (a_PreservedTiles.find(ChunkIndex) != a_PreservedTiles.end())
					{
						State = &OldState;
					}
					else
					{
						const BLOCKTYPE Block = (Blocks == nullptr) ? E_BLOCK_AIR : (*Blocks)[Index];
						const auto Meta = (Metas == nullptr) ? NIBBLETYPE{0} : cChunkDef::ExpandNibble(Metas->data(), Index);
						State = &NamedBlockPalette::FromLegacy(a_DataVersion, Block, Meta);
					}
				}

				Changed |= (*State != OldState);
				const auto [Entry, Inserted] = Indices.try_emplace(State, static_cast<UInt16>(Section.Palette.size()));
				if (Inserted)
				{
					Section.Palette.push_back(*State);
				}
				Section.Indices[Index] = Entry->second;
			}
			if (Changed)
			{
				Changes.emplace(Y, std::move(Section));
			}
		}
		return Changes;
	}
}





cWSSPalettedAnvil::cWSSPalettedAnvil(cWorld * a_World, int a_CompressionFactor, const cWorldStorageFormat & a_Format):
	cWSSAnvil(a_World, a_CompressionFactor, a_Format),
	m_DataVersion(a_Format.DataVersion)
{
	// Load the version's registry before processing any writes.
	static_cast<void>(NamedBlockPalette::FromLegacy(m_DataVersion, E_BLOCK_AIR, 0));
}





bool cWSSPalettedAnvil::LoadChunkFromData(const cChunkCoords & a_Chunk, ContiguousByteBufferView a_Data)
{
	try
	{
		const auto Extracted = m_Extractor.ExtractZLib(a_Data);
		const cParsedNBT NBT(Extracted.GetView());
		if (ReadAnvilDataVersion(NBT) <= LegacyAnvilMaxDataVersion)
		{
			// Vanilla worlds may still have older chunks that Vanilla hasn't upgraded.
			return LoadChunkFromNBT(a_Chunk, NBT, a_Data);
		}

		const PalettedAnvil::Chunk Chunk(Extracted.GetView());
		if (Chunk.GetCoords() != a_Chunk)
		{
			throw std::runtime_error("Anvil chunk coordinates don't match its region slot");
		}

		struct SetChunkData Data(a_Chunk);
		const auto PreservedTiles = NativeBlockEntities(NBT, a_Chunk);
		for (size_t Y = 0; Y < cChunkDef::NumSections; ++Y)
		{
			const auto Section = Chunk.GetSection(Y);
			if (Section == nullptr)
			{
				continue;
			}

			std::vector<std::pair<BLOCKTYPE, NIBBLETYPE>> Projected;
			Projected.reserve(Section->Palette.size());
			for (const auto & State : Section->Palette)
			{
				Projected.push_back(NamedBlockPalette::ToLegacy(State));
			}
			ChunkBlockData::SectionType Blocks{};
			ChunkBlockData::SectionMetaType Metas{};
			for (size_t Index = 0; Index < ChunkBlockData::SectionBlockCount; ++Index)
			{
				const auto [Block, Meta] = Projected[Section->Indices[Index]];
				const auto ChunkIndex = Y * ChunkBlockData::SectionBlockCount + Index;
				if (PreservedTiles.find(ChunkIndex) != PreservedTiles.end())
				{
					// A legacy block-entity simulator must not operate on native data it cannot persist.
					Blocks[Index] = E_BLOCK_BEDROCK;
				}
				else
				{
					Blocks[Index] = Block;
					cChunkDef::PackNibble(Metas, Index, Meta);
				}
			}
			Data.BlockData.SetSection(Blocks, Metas, Y);
			Data.BlockData.SetNamedSection(Y, Section->Palette, Section->Indices);
		}

		// Recalculate derived lighting and heights using the engine's simulation projection.
		Data.IsLightValid = false;
		Data.UpdateHeightMap();
		ReadPalettedBiomes(Chunk.GetBiomes(), Data);
		m_World->QueueSetChunkData(std::move(Data));
		return true;
	}
	catch (const std::exception & Error)
	{
		ChunkLoadFailed(a_Chunk, Error.what(), a_Data);
		return false;
	}
}





std::optional<Compression::Result> cWSSPalettedAnvil::SaveChunkToData(const cChunkCoords & a_Chunk)
{
	ContiguousByteBuffer Compressed;
	if (GetChunkData(a_Chunk, Compressed))
	{
		const auto Extracted = m_Extractor.ExtractZLib(Compressed);
		const cParsedNBT NBT(Extracted.GetView());
		const auto Version = ReadAnvilDataVersion(NBT);
		if (Version <= LegacyAnvilMaxDataVersion)
		{
			return SaveLegacyChunkToData(a_Chunk, Version);
		}

		const PalettedAnvil::Chunk Original(Extracted.GetView());
		if (Original.GetCoords() != a_Chunk)
		{
			throw std::runtime_error("Anvil chunk coordinates don't match its region slot");
		}
		cTerrainSnapshot Snapshot;
		if (!m_World->GetChunkData(a_Chunk, Snapshot))
		{
			throw std::runtime_error("Chunk unloaded before save");
		}
		const auto Tiles = NativeBlockEntities(NBT, a_Chunk);
		const auto Changes = CollectTerrainChanges(Snapshot.m_BlockData, Version, &Original, Tiles);
		if (Changes.empty())
		{
			return std::nullopt;
		}
		const auto Bytes = Original.Encode(Changes, Snapshot.IsLightValid ? &Snapshot.m_LightData : nullptr);
		return m_Compressor.CompressZLib(Bytes);
	}
	if (m_ProtectedChunks.find(a_Chunk) != m_ProtectedChunks.end())
	{
		throw std::runtime_error("Existing Anvil region data could not be read");
	}

	cTerrainSnapshot Snapshot;
	if (!m_World->GetChunkData(a_Chunk, Snapshot))
	{
		throw std::runtime_error("Chunk unloaded before save");
	}
	const auto Sections = CollectTerrainChanges(Snapshot.m_BlockData, m_DataVersion, nullptr, {});
	const auto Biomes = WritePalettedBiomes(Snapshot.Biomes, m_DataVersion);
	const auto Bytes = PalettedAnvil::WriteChunk(
		m_DataVersion,
		a_Chunk,
		Sections,
		Biomes,
		m_World->GetWorldAge().count(),
		Snapshot.IsLightValid ? &Snapshot.m_LightData : nullptr
	);
	return m_Compressor.CompressZLib(Bytes);
}
