#include "Globals.h"
#include "WSSPalettedAnvil.h"
#include "PalettedAnvil.h"
#include "PalettedItemNBT.h"
#include "PalettedStateBlockEntities.h"
#include "NBTChunkSerializer.h"
#include "../FastRandom.h"
#include "../Mobs/Monster.h"
#include "../BlockType.h"
#include "../ChunkDataCallback.h"
#include "../Entities/Entity.h"
#include "../Protocol/Palettes/NamedBlockPalette.h"
#include "../SetChunkData.h"
#include "../World.h"





namespace
{
	ContiguousByteBuffer CopyEntityRecord(const cParsedNBT & a_NBT, int a_Tag)
	{
		cFastNBTWriter Writer;
		for (int Field = a_NBT.GetFirstChild(a_Tag); Field >= 0; Field = a_NBT.GetNextSibling(Field))
		{
			Writer.CopyTag(a_NBT, Field);
		}
		Writer.Finish();
		return ContiguousByteBuffer(Writer.GetResult());
	}

	bool SameTag(const cParsedNBT & a_First, int a_FirstTag, const cParsedNBT & a_Second, int a_SecondTag)
	{
		if ((a_FirstTag < 0) || (a_SecondTag < 0))
		{
			return a_FirstTag == a_SecondTag;
		}
		return
			(a_First.GetType(a_FirstTag) == a_Second.GetType(a_SecondTag)) &&
			(a_First.GetTagPayload(a_FirstTag) == a_Second.GetTagPayload(a_SecondTag));
	}

	/** Preserve source fields unless the corresponding runtime field changed. */
	void WriteMergedEntity(
		cFastNBTWriter & a_Writer,
		const cParsedNBT & a_Original,
		const cParsedNBT & a_Baseline,
		const cParsedNBT & a_Live
	)
	{
		for (int Tag = a_Original.GetFirstChild(0); Tag >= 0; Tag = a_Original.GetNextSibling(Tag))
		{
			const auto Name = a_Original.GetName(Tag);
			if ((Name == "UUIDMost") || (Name == "UUIDLeast"))
			{
				continue;
			}
			const auto Previous = a_Baseline.FindChildByName(0, Name);
			const auto Current = a_Live.FindChildByName(0, Name);
			if (SameTag(a_Baseline, Previous, a_Live, Current))
			{
				a_Writer.CopyTag(a_Original, Tag);
			}
			else if (Current >= 0)
			{
				a_Writer.CopyTag(a_Live, Current);
			}
		}
		for (int Tag = a_Live.GetFirstChild(0); Tag >= 0; Tag = a_Live.GetNextSibling(Tag))
		{
			const auto Name = a_Live.GetName(Tag);
			if (
				(a_Original.FindChildByName(0, Name) < 0) &&
				!SameTag(a_Baseline, a_Baseline.FindChildByName(0, Name), a_Live, Tag)
			)
			{
				a_Writer.CopyTag(a_Live, Tag);
			}
		}
	}

	void SetEntityUUID(cPalettedEntityData & a_Data, const cParsedNBT * a_Source = nullptr, int a_Tag = -1)
	{
		if (a_Source != nullptr)
		{
			const auto Most = a_Source->FindChildByName(a_Tag, "UUIDMost");
			const auto Least = a_Source->FindChildByName(a_Tag, "UUIDLeast");
			if (
				(Most >= 0) && (a_Source->GetType(Most) == TAG_Long) &&
				(Least >= 0) && (a_Source->GetType(Least) == TAG_Long)
			)
			{
				a_Data.UUIDMost = a_Source->GetLong(Most);
				a_Data.UUIDLeast = a_Source->GetLong(Least);
			}
		}
		if ((a_Data.UUIDMost != 0) || (a_Data.UUIDLeast != 0))
		{
			return;
		}

		const auto RandomLong = [] { return GetRandomProvider().RandInt<UInt64>(); };
		a_Data.UUIDMost = static_cast<Int64>((RandomLong() & ~UInt64{0xf000}) | 0x4000);
		a_Data.UUIDLeast = static_cast<Int64>((RandomLong() & ~(UInt64{3} << 62)) | (UInt64{2} << 62));
	}

	ContiguousByteBuffer BuildEntityRecord(
		ContiguousByteBuffer a_Live,
		const cPalettedEntityData * a_Data,
		bool a_Mobile
	)
	{
		if (a_Live.empty())
		{
			// A mobile entity may have entered a chunk predating its native type.
			return (a_Data == nullptr) ? ContiguousByteBuffer{} : a_Data->Original;
		}
		cFastNBTWriter Writer;
		const cParsedNBT Live(a_Live);
		if ((a_Data != nullptr) && !a_Data->Original.empty())
		{
			const cParsedNBT Original(a_Data->Original);
			const cParsedNBT Baseline(a_Data->Baseline);
			WriteMergedEntity(Writer, Original, Baseline, Live);
		}
		else
		{
			for (int Tag = Live.GetFirstChild(0); Tag >= 0; Tag = Live.GetNextSibling(Tag))
			{
				Writer.CopyTag(Live, Tag);
			}
		}
		if (a_Mobile && (a_Data != nullptr))
		{
			Writer.AddLong("UUIDMost", a_Data->UUIDMost);
			Writer.AddLong("UUIDLeast", a_Data->UUIDLeast);
		}
		Writer.Finish();
		return ContiguousByteBuffer(Writer.GetResult());
	}

	bool HasUnreadableInventory(const cParsedNBT & a_NBT, int a_Tag, int a_DataVersion)
	{
		if (a_NBT.GetType(a_Tag) == TAG_Compound)
		{
			// The legacy inventory has no way to represent unresolved native loot tables.
			if (a_NBT.FindChildByName(a_Tag, "LootTable") >= 0)
			{
				return true;
			}
			if (a_NBT.FindChildByName(a_Tag, "Count") >= 0)
			{
				cItem Item;
				return !PalettedItemNBT::Read(Item, a_NBT, a_Tag, a_DataVersion);
			}
		}
		if ((a_NBT.GetType(a_Tag) == TAG_Compound) || (a_NBT.GetType(a_Tag) == TAG_List))
		{
			for (int Child = a_NBT.GetFirstChild(a_Tag); Child >= 0; Child = a_NBT.GetNextSibling(Child))
			{
				if (HasUnreadableInventory(a_NBT, Child, a_DataVersion))
				{
					return true;
				}
			}
		}
		return false;
	}

	void LoadMobEquipment(cMonster & a_Mob, const cParsedNBT & a_NBT, int a_Tag, int a_Version)
	{
		cMonster::Equipment Equipment;
		const auto Load = [&](const AString & a_Name, size_t a_Begin, size_t a_End)
		{
			const auto List = a_NBT.FindChildByName(a_Tag, a_Name);
			if (List < 0)
			{
				return;
			}
			if (a_NBT.GetType(List) != TAG_List)
			{
				throw std::runtime_error("Invalid Java mob equipment list");
			}
			size_t Slot = a_Begin;
			for (int Item = a_NBT.GetFirstChild(List); Item >= 0; Item = a_NBT.GetNextSibling(Item))
			{
				if ((Slot == a_End) || !PalettedItemNBT::Read(Equipment[Slot], a_NBT, Item, a_Version))
				{
					throw std::runtime_error("Unsupported Java mob equipment");
				}
				++Slot;
			}
		};
		Load("HandItems", 0, 2);
		Load("ArmorItems", 2, Equipment.size());
		if (std::any_of(Equipment.begin(), Equipment.end(), [](const auto & a_Item) { return !a_Item.IsEmpty(); }))
		{
			a_Mob.SetEquipment(std::move(Equipment));
		}
	}

	void WriteEntityList(
		cFastNBTWriter & a_Writer,
		const AString & a_Name,
		const std::vector<ContiguousByteBuffer> & a_Live,
		const std::vector<ContiguousByteBuffer> & a_Unloaded
	)
	{
		a_Writer.BeginList(a_Name, TAG_Compound);
		for (const auto * List : { &a_Live, &a_Unloaded })
		{
			for (const auto & Record : *List)
			{
				const cParsedNBT NBT(Record);
				a_Writer.CopyTag(NBT, 0);
			}
		}
		a_Writer.EndList();
	}

	class cPalettedChunkSnapshot final :
		public cChunkDataCopyCollector
	{
	public:
		explicit cPalettedChunkSnapshot(int a_DataVersion):
			m_DataVersion(a_DataVersion)
		{
		}

		cChunkDef::BiomeMap Biomes{};
		bool IsLightValid = false;
		cUnloadedEntityRecords Unloaded;
		std::map<size_t, cNamedBlockState> StateBackedBlocks;

		ContiguousByteBuffer EncodeEntities() const
		{
			cFastNBTWriter Writer;
			WriteEntityList(Writer, "Entities", m_Entities, Unloaded.Entities);
			WriteEntityList(Writer, "TileEntities", m_BlockEntities, Unloaded.BlockEntities);
			Writer.Finish();
			return ContiguousByteBuffer(Writer.GetResult());
		}

	private:
		const int m_DataVersion;
		std::vector<ContiguousByteBuffer> m_Entities;
		std::vector<ContiguousByteBuffer> m_BlockEntities;

		void BiomeMap(const cChunkDef::BiomeMap & a_Biomes) override
		{
			std::copy(std::begin(a_Biomes), std::end(a_Biomes), Biomes);
		}

		void LightIsValid(bool a_IsValid) override
		{
			IsLightValid = a_IsValid;
		}

		void UnloadedEntities(const cUnloadedEntityRecords & a_Records) override
		{
			Unloaded = a_Records;
		}

		void Entity(cEntity * a_Entity) override
		{
			if (!a_Entity->IsTicking() || a_Entity->IsPlayer() || a_Entity->IsFloater())
			{
				return;
			}
			auto Serialized = NBTChunkSerializer::SerializeEntity(*a_Entity, m_DataVersion);
			const auto Source = a_Entity->GetPalettedStorageData();
			if (Serialized.empty() && (Source != nullptr) && (Source->OriginalVersion > m_DataVersion))
			{
				// Preserve the entity at its live position even when the destination has an older format.
				Serialized = NBTChunkSerializer::SerializeEntity(*a_Entity, Source->OriginalVersion);
			}
			if (!Serialized.empty() && (Source == nullptr))
			{
				auto Data = std::make_unique<cPalettedEntityData>();
				SetEntityUUID(*Data);
				a_Entity->SetPalettedStorageData(std::move(Data));
			}
			auto Record = BuildEntityRecord(std::move(Serialized), a_Entity->GetPalettedStorageData(), true);
			if (!Record.empty())
			{
				m_Entities.push_back(std::move(Record));
			}
		}

		void BlockEntity(cBlockEntity * a_Entity) override
		{
			const auto Pos = a_Entity->GetRelPos();
			const auto OriginalState = m_BlockData.GetNamedBlock(Pos);
			if (auto State = PalettedStateBlockEntities::ToState(*a_Entity, OriginalState, m_DataVersion); State.has_value())
			{
				StateBackedBlocks.emplace(cChunkDef::MakeIndex(Pos), std::move(*State));
			}
			auto Serialized = NBTChunkSerializer::SerializeBlockEntity(*a_Entity, m_DataVersion);
			auto Record = BuildEntityRecord(std::move(Serialized), a_Entity->GetPalettedStorageData(), false);
			if (!Record.empty())
			{
				m_BlockEntities.push_back(std::move(Record));
			}
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

	/** Returns the relative positions of opaque native block entities. */
	std::set<size_t> NativeBlockEntities(const cUnloadedEntityRecords & a_Records, cChunkCoords a_Chunk)
	{
		std::set<size_t> Positions;
		for (const auto & Record : a_Records.BlockEntities)
		{
			const cParsedNBT NBT(Record);
			const auto X = NBT.FindChildByName(0, "x");
			const auto Y = NBT.FindChildByName(0, "y");
			const auto Z = NBT.FindChildByName(0, "z");
			if (
				(X < 0) || (NBT.GetType(X) != TAG_Int) ||
				(Y < 0) || (NBT.GetType(Y) != TAG_Int) ||
				(Z < 0) || (NBT.GetType(Z) != TAG_Int)
			)
			{
				continue;
			}
			const Vector3i Absolute(NBT.GetInt(X), NBT.GetInt(Y), NBT.GetInt(Z));
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
		const std::set<size_t> & a_PreservedTiles,
		const std::map<size_t, cNamedBlockState> & a_StateBackedBlocks
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
				const auto ChunkIndex = Y * ChunkBlockData::SectionBlockCount + Index;
				const auto Overlay = a_StateBackedBlocks.find(ChunkIndex);
				const auto * State = (Overlay == a_StateBackedBlocks.end()) ? a_Blocks.GetNamedBlock(Y, Index) : &Overlay->second;
				if (State == nullptr)
				{
					if (a_PreservedTiles.find(ChunkIndex) != a_PreservedTiles.end())
					{
						State = &OldState;
					}
					else
					{
						const BLOCKTYPE Block = (Blocks == nullptr) ? BLOCKTYPE{E_BLOCK_AIR} : (*Blocks)[Index];
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
		const auto Version = ReadAnvilDataVersion(NBT);
		m_EntityDataVersion = 0;
		if (Version <= LegacyAnvilMaxDataVersion)
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
				if (State.Name == "minecraft:flower_pot")
				{
					Projected.emplace_back(E_BLOCK_FLOWER_POT, 0);
				}
				else
				{
					Projected.push_back(NamedBlockPalette::ToLegacy(State));
				}
			}
			ChunkBlockData::SectionType Blocks{};
			ChunkBlockData::SectionMetaType Metas{};
			for (size_t Index = 0; Index < ChunkBlockData::SectionBlockCount; ++Index)
			{
				const auto [Block, Meta] = Projected[Section->Indices[Index]];
				Blocks[Index] = Block;
				cChunkDef::PackNibble(Metas, Index, Meta);
			}
			Data.BlockData.SetSection(Blocks, Metas, Y);
			Data.BlockData.SetNamedSection(Y, Section->Palette, Section->Indices);
		}

		m_EntityDataVersion = Version;
		LoadNativeObjects(NBT, Data);
		m_EntityDataVersion = 0;
		for (size_t Y = 0; Y < cChunkDef::NumSections; ++Y)
		{
			const auto Blocks = Data.BlockData.GetSection(Y);
			if (Blocks == nullptr)
			{
				continue;
			}
			for (size_t Index = 0; Index < Blocks->size(); ++Index)
			{
				const auto Block = (*Blocks)[Index];
				if ((Block != E_BLOCK_NOTE_BLOCK) && (Block != E_BLOCK_FLOWER_POT))
				{
					continue;
				}
				const auto RelPos = Vector3i(
					static_cast<int>(Index % cChunkDef::Width),
					static_cast<int>(Y * cChunkDef::SectionHeight + Index / (cChunkDef::Width * cChunkDef::Width)),
					static_cast<int>((Index / cChunkDef::Width) % cChunkDef::Width)
				);
				const auto Name = Data.BlockData.GetNamedBlock(Y, Index);
				if (Name == nullptr)
				{
					continue;
				}
				const auto AbsPos = cChunkDef::RelativeToAbsolute(RelPos, a_Chunk);
				auto Entity = PalettedStateBlockEntities::FromState(*Name, Block, Data.BlockData.GetMeta(RelPos), AbsPos, nullptr);
				if (Entity != nullptr)
				{
					Data.BlockEntities.try_emplace(cChunkDef::MakeIndex(RelPos), std::move(Entity));
				}
				else
				{
					// Preserve plants without an equivalent in the engine's pot inventory.
					Data.BlockData.SetNamedProjection(RelPos, E_BLOCK_BEDROCK, 0);
				}
			}
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
		m_EntityDataVersion = 0;
		ChunkLoadFailed(a_Chunk, Error.what(), a_Data);
		return false;
	}
}





void cWSSPalettedAnvil::LoadNativeObjects(const cParsedNBT & a_NBT, struct SetChunkData & a_Data)
{
	auto Unloaded = std::make_unique<cUnloadedEntityRecords>();
	const auto Entities = a_NBT.FindTagByPath(0, "Level\\Entities");
	if ((Entities >= 0) && (a_NBT.GetType(Entities) == TAG_List) && (a_NBT.GetChildrenType(Entities) == TAG_Compound))
	{
		for (int Tag = a_NBT.GetFirstChild(Entities); Tag >= 0; Tag = a_NBT.GetNextSibling(Tag))
		{
			const auto ID = a_NBT.FindChildByName(Tag, "id");
			const auto Passengers = a_NBT.FindChildByName(Tag, "Passengers");
			const auto Count = a_Data.Entities.size();
			try
			{
				if (
					(ID >= 0) && (a_NBT.GetType(ID) == TAG_String) &&
					((Passengers < 0) || (a_NBT.GetFirstChild(Passengers) < 0)) &&
					!HasUnreadableInventory(a_NBT, Tag, m_EntityDataVersion)
				)
				{
					LoadEntityFromNBT(a_Data.Entities, a_NBT, Tag, a_NBT.GetStringView(ID));
				}
				if (a_Data.Entities.size() == Count + 1)
				{
					auto & Entity = *a_Data.Entities.back();
					if (Entity.IsMob())
					{
						auto & Mob = static_cast<cMonster &>(Entity);
						LoadMonsterBaseFromNBT(Mob, a_NBT, Tag);
						LoadMobEquipment(Mob, a_NBT, Tag, m_EntityDataVersion);
						const auto Age = a_NBT.FindChildByName(Tag, "Age");
						if ((Age >= 0) && (a_NBT.GetType(Age) == TAG_Int))
						{
							Mob.SetAge(a_NBT.GetInt(Age));
						}
						else
						{
							const auto IsBaby = a_NBT.FindChildByName(Tag, "IsBaby");
							if ((IsBaby >= 0) && (a_NBT.GetType(IsBaby) == TAG_Byte) && a_NBT.GetByte(IsBaby))
							{
								Mob.SetAge(-24000);
							}
						}
					}
					auto Baseline = NBTChunkSerializer::SerializeEntity(Entity, m_EntityDataVersion);
					if (!Baseline.empty())
					{
						auto Data = std::make_unique<cPalettedEntityData>();
						Data->OriginalVersion = m_EntityDataVersion;
						Data->Original = CopyEntityRecord(a_NBT, Tag);
						Data->Baseline = std::move(Baseline);
						SetEntityUUID(*Data, &a_NBT, Tag);
						Entity.SetPalettedStorageData(std::move(Data));
						continue;
					}
				}
			}
			catch (const std::exception & Error)
			{
				LOGWARNING("Leaving a Java entity in chunk %s offline: %s", a_Data.Chunk.ToString(), Error.what());
			}
			a_Data.Entities.resize(Count);
			Unloaded->Entities.push_back(CopyEntityRecord(a_NBT, Tag));
		}
	}

	const auto Tiles = a_NBT.FindTagByPath(0, "Level\\TileEntities");
	if ((Tiles >= 0) && (a_NBT.GetType(Tiles) == TAG_List) && (a_NBT.GetChildrenType(Tiles) == TAG_Compound))
	{
		for (int Tag = a_NBT.GetFirstChild(Tiles); Tag >= 0; Tag = a_NBT.GetNextSibling(Tag))
		{
			Vector3i AbsPos;
			if (
				GetBlockEntityNBTPos(a_NBT, Tag, AbsPos) && (AbsPos.y >= 0) && (AbsPos.y < cChunkDef::Height) &&
				(cChunkDef::BlockToChunk(AbsPos) == a_Data.Chunk)
			)
			{
				const auto RelPos = cChunkDef::AbsoluteToRelative(AbsPos);
				const auto Block = a_Data.BlockData.GetBlock(RelPos);
				const auto Meta = a_Data.BlockData.GetMeta(RelPos);
				const auto Index = cChunkDef::MakeIndex(RelPos);
				try
				{
					if (
						cBlockEntity::IsBlockEntityBlockType(Block) && (a_Data.BlockEntities.find(Index) == a_Data.BlockEntities.end()) &&
						!HasUnreadableInventory(a_NBT, Tag, m_EntityDataVersion)
					)
					{
						auto Entity = LoadBlockEntityFromNBT(a_NBT, Tag, AbsPos, Block, Meta);
						if (Entity != nullptr)
						{
							if (const auto State = a_Data.BlockData.GetNamedBlock(RelPos); State != nullptr)
							{
								PalettedStateBlockEntities::ApplyState(*Entity, *State);
							}
							auto Baseline = NBTChunkSerializer::SerializeBlockEntity(*Entity, m_EntityDataVersion);
							if (!Baseline.empty())
							{
								auto Data = std::make_unique<cPalettedEntityData>();
								Data->Original = CopyEntityRecord(a_NBT, Tag);
								Data->Baseline = std::move(Baseline);
									Entity->SetPalettedStorageData(std::move(Data));
								a_Data.BlockEntities.emplace(Index, std::move(Entity));
								continue;
							}
						}
					}
				}
				catch (const std::exception & Error)
				{
					LOGWARNING("Leaving a Java block entity in chunk %s offline: %s", a_Data.Chunk.ToString(), Error.what());
				}

				// Prevent the legacy simulators from changing an unsupported block entity.
				if (a_Data.BlockEntities.find(Index) == a_Data.BlockEntities.end())
				{
					const auto Named = a_Data.BlockData.GetNamedBlock(RelPos);
					if (Named != nullptr)
					{
						a_Data.BlockData.SetNamedProjection(RelPos, E_BLOCK_BEDROCK, 0);
					}
				}
			}
			Unloaded->BlockEntities.push_back(CopyEntityRecord(a_NBT, Tag));
		}
	}
	if (!Unloaded->Entities.empty() || !Unloaded->BlockEntities.empty())
	{
		a_Data.UnloadedEntities = std::move(Unloaded);
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
		cPalettedChunkSnapshot Snapshot(Version);
		if (!m_World->GetChunkData(a_Chunk, Snapshot))
		{
			throw std::runtime_error("Chunk unloaded before save");
		}
		const auto Tiles = NativeBlockEntities(Snapshot.Unloaded, a_Chunk);
		const auto Changes = CollectTerrainChanges(Snapshot.m_BlockData, Version, &Original, Tiles, Snapshot.StateBackedBlocks);
		const auto EntityBytes = Snapshot.EncodeEntities();
		const cParsedNBT Entities(EntityBytes);
		const bool EntitiesChanged =
			!SameTag(NBT, NBT.FindTagByPath(0, "Level\\Entities"), Entities, Entities.FindChildByName(0, "Entities")) ||
			!SameTag(NBT, NBT.FindTagByPath(0, "Level\\TileEntities"), Entities, Entities.FindChildByName(0, "TileEntities"));
		if (Changes.empty() && !EntitiesChanged)
		{
			return std::nullopt;
		}
		const auto Bytes = Original.Encode(
			Changes,
			Snapshot.IsLightValid ? &Snapshot.m_LightData : nullptr,
			EntitiesChanged ? &Entities : nullptr
		);
		return m_Compressor.CompressZLib(Bytes);
	}
	if (m_ProtectedChunks.find(a_Chunk) != m_ProtectedChunks.end())
	{
		throw std::runtime_error("Existing Anvil region data could not be read");
	}

	cPalettedChunkSnapshot Snapshot(m_DataVersion);
	if (!m_World->GetChunkData(a_Chunk, Snapshot))
	{
		throw std::runtime_error("Chunk unloaded before save");
	}
	const auto Sections = CollectTerrainChanges(Snapshot.m_BlockData, m_DataVersion, nullptr, {}, Snapshot.StateBackedBlocks);
	const auto Biomes = WritePalettedBiomes(Snapshot.Biomes, m_DataVersion);
	const auto EntityBytes = Snapshot.EncodeEntities();
	const cParsedNBT Entities(EntityBytes);
	const auto Bytes = PalettedAnvil::WriteChunk(
		m_DataVersion,
		a_Chunk,
		Sections,
		Biomes,
		m_World->GetWorldAge().count(),
		Snapshot.IsLightValid ? &Snapshot.m_LightData : nullptr,
		&Entities
	);
	return m_Compressor.CompressZLib(Bytes);
}
