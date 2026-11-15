
// ChunkData.cpp

// Implements the cChunkData class that represents the block's type, meta, blocklight and skylight storage for a chunk

#include "Globals.h"
#include "ChunkData.h"
#include "BlockType.h"





namespace
{
	struct SectionIndices
	{
		size_t Section = 0;  // Index into m_Sections
		size_t Index = 0;    // Index into a single sChunkSection
	};

	inline SectionIndices IndicesFromRelPos(const Vector3i a_RelPos)
	{
		ASSERT(cChunkDef::IsValidRelPos(a_RelPos));

		return
		{
			static_cast<size_t>(a_RelPos.y / cChunkDef::SectionHeight),
			cChunkDef::MakeIndex(a_RelPos.x, a_RelPos.y % cChunkDef::SectionHeight, a_RelPos.z)
		};
	}

	bool IsCompressed(const size_t ElementCount)
	{
		return ElementCount != ChunkBlockData::SectionBlockCount;
	}

	template <size_t ElementCount, typename ValueType>
	ValueType UnpackDefaultValue(const ValueType DefaultValue)
	{
		if (IsCompressed(ElementCount))
		{
			return DefaultValue & 0xF;
		}

		return DefaultValue;
	}
}  // namespace (anonymous)





template<class ElementType, size_t ElementCount, ElementType DefaultValue>
void ChunkDataStore<ElementType, ElementCount, DefaultValue>::Assign(const ChunkDataStore<ElementType, ElementCount, DefaultValue> & a_Other)
{
	for (size_t Y = 0; Y != cChunkDef::NumSections; Y++)
	{
		Store[Y].reset();

		if (const auto & Other = a_Other.Store[Y]; Other != nullptr)
		{
			Store[Y] = std::make_unique<Type>(*Other);
		}
	}
}





template<class ElementType, size_t ElementCount, ElementType DefaultValue>
ElementType ChunkDataStore<ElementType, ElementCount, DefaultValue>::Get(const Vector3i a_Position) const
{
	const auto Indices = IndicesFromRelPos(a_Position);
	const auto & Section = Store[Indices.Section];

	if (Section != nullptr)
	{
		if (IsCompressed(ElementCount))
		{
			return cChunkDef::ExpandNibble(Section->data(), Indices.Index);
		}
		else
		{
			return (*Section)[Indices.Index];
		}
	}

	return UnpackDefaultValue<ElementCount>(DefaultValue);
}





template<class ElementType, size_t ElementCount, ElementType DefaultValue>
typename ChunkDataStore<ElementType, ElementCount, DefaultValue>::Type * ChunkDataStore<ElementType, ElementCount, DefaultValue>::GetSection(const size_t a_Y) const
{
	return Store[a_Y].get();
}





template<class ElementType, size_t ElementCount, ElementType DefaultValue>
void ChunkDataStore<ElementType, ElementCount, DefaultValue>::Set(const Vector3i a_Position, const ElementType a_Value)
{
	const auto Indices = IndicesFromRelPos(a_Position);
	auto & Section = Store[Indices.Section];

	if (Section == nullptr)
	{
		if (a_Value == UnpackDefaultValue<ElementCount>(DefaultValue))
		{
			return;
		}

		Section = cpp20::make_unique_for_overwrite<Type>();
		std::fill(Section->begin(), Section->end(), DefaultValue);
	}

	if (IsCompressed(ElementCount))
	{
		cChunkDef::PackNibble(Section->data(), Indices.Index, a_Value);
	}
	else
	{
		(*Section)[Indices.Index] = a_Value;
	}
}





template<class ElementType, size_t ElementCount, ElementType DefaultValue>
void ChunkDataStore<ElementType, ElementCount, DefaultValue>::SetSection(const ElementType (& a_Source)[ElementCount], const size_t a_Y)
{
	auto & Section = Store[a_Y];
	const auto SourceEnd = std::end(a_Source);

	if (Section != nullptr)
	{
		std::copy(a_Source, SourceEnd, Section->begin());
	}
	else if (std::any_of(a_Source, SourceEnd, [](const auto Value) { return Value != DefaultValue; }))
	{
		Section = cpp20::make_unique_for_overwrite<Type>();
		std::copy(a_Source, SourceEnd, Section->begin());
	}
}





template<class ElementType, size_t ElementCount, ElementType DefaultValue>
void ChunkDataStore<ElementType, ElementCount, DefaultValue>::SetAll(const ElementType (& a_Source)[cChunkDef::NumSections * ElementCount])
{
	for (size_t Y = 0; Y != cChunkDef::NumSections; Y++)
	{
		SetSection(*reinterpret_cast<const ElementType (*)[ElementCount]>(a_Source + Y * ElementCount), Y);
	}
}





void ChunkBlockData::Assign(const ChunkBlockData & a_Other)
{
	if (&a_Other == this)
	{
		return;
	}

	m_Blocks.Assign(a_Other.m_Blocks);
	m_Metas.Assign(a_Other.m_Metas);
	for (size_t Y = 0; Y != cChunkDef::NumSections; ++Y)
	{
		const auto & Other = a_Other.m_NamedSections[Y];
		m_NamedSections[Y] = (Other == nullptr) ? nullptr : std::make_unique<NamedSection>(*Other);
	}
}





const cNamedBlockState * ChunkBlockData::GetNamedBlock(const Vector3i a_Position) const
{
	const auto [Section, Index] = IndicesFromRelPos(a_Position);
	return GetNamedBlock(Section, Index);
}





const cNamedBlockState * ChunkBlockData::GetNamedBlock(const size_t a_Y, const size_t a_Index) const
{
	ASSERT(a_Y < cChunkDef::NumSections);
	ASSERT(a_Index < SectionBlockCount);

	const auto & Section = m_NamedSections[a_Y];
	if (Section == nullptr)
	{
		return nullptr;
	}

	const auto Index = Section->Indices[a_Index];
	return (Index == MissingNamedState) ? nullptr : &Section->Palette[Index];
}





void ChunkBlockData::SetNamedSection(const size_t a_Y, std::vector<cNamedBlockState> a_Palette, const NamedIndices & a_Indices)
{
	ASSERT(a_Y < cChunkDef::NumSections);
	ASSERT(a_Palette.size() <= SectionBlockCount);
	ASSERT(std::all_of(a_Indices.begin(), a_Indices.end(), [&](const UInt16 Index) { return Index < a_Palette.size(); }));

	auto Section = std::make_unique<NamedSection>();
	Section->Palette = std::move(a_Palette);
	Section->Indices = a_Indices;
	m_NamedSections[a_Y] = std::move(Section);
}





void ChunkBlockData::ClearNamedBlock(const size_t a_Y, const size_t a_Index)
{
	auto & Section = m_NamedSections[a_Y];
	if (Section->Indices[a_Index] == MissingNamedState)
	{
		return;
	}

	Section->Indices[a_Index] = MissingNamedState;
	if (--Section->Remaining == 0)
	{
		Section.reset();
	}
}





void ChunkBlockData::SetBlock(const Vector3i a_Position, const BLOCKTYPE a_Block)
{
	const auto [Section, Index] = IndicesFromRelPos(a_Position);
	const bool ProjectionChanged = HasNamedSection(Section) && (m_Blocks.Get(a_Position) != a_Block);
	m_Blocks.Set(a_Position, a_Block);
	if (ProjectionChanged)
	{
		ClearNamedBlock(Section, Index);
	}
}





void ChunkBlockData::SetMeta(const Vector3i a_Position, const NIBBLETYPE a_Meta)
{
	const auto [Section, Index] = IndicesFromRelPos(a_Position);
	const bool ProjectionChanged = HasNamedSection(Section) && (m_Metas.Get(a_Position) != a_Meta);
	m_Metas.Set(a_Position, a_Meta);
	if (ProjectionChanged)
	{
		ClearNamedBlock(Section, Index);
	}
}





void ChunkBlockData::SetAll(const cChunkDef::BlockTypes & a_BlockSource, const cChunkDef::BlockNibbles & a_MetaSource)
{
	for (size_t Y = 0; Y != cChunkDef::NumSections; ++Y)
	{
		SetSection(
			*reinterpret_cast<const SectionType *>(a_BlockSource + Y * SectionBlockCount),
			*reinterpret_cast<const SectionMetaType *>(a_MetaSource + Y * SectionMetaCount),
			Y
		);
	}
}





void ChunkBlockData::SetSection(const SectionType & a_BlockSource, const SectionMetaType & a_MetaSource, const size_t a_Y)
{
	if (HasNamedSection(a_Y))
	{
		const auto OldBlocks = GetSection(a_Y);
		const auto OldMetas = GetMetaSection(a_Y);
		for (size_t Index = 0; (Index < SectionBlockCount) && HasNamedSection(a_Y); ++Index)
		{
			const auto OldBlock = (OldBlocks == nullptr) ? DefaultValue : (*OldBlocks)[Index];
			const auto OldMeta = (OldMetas == nullptr) ? DefaultMetaValue : cChunkDef::ExpandNibble(OldMetas->data(), Index);
			if ((OldBlock != a_BlockSource[Index]) || (OldMeta != cChunkDef::ExpandNibble(a_MetaSource, Index)))
			{
				ClearNamedBlock(a_Y, Index);
			}
		}
	}

	m_Blocks.SetSection(a_BlockSource, a_Y);
	m_Metas.SetSection(a_MetaSource, a_Y);
}





void ChunkLightData::Assign(const ChunkLightData & a_Other)
{
	m_BlockLights.Assign(a_Other.m_BlockLights);
	m_SkyLights.Assign(a_Other.m_SkyLights);
}





void ChunkLightData::SetAll(const cChunkDef::BlockNibbles & a_BlockLightSource, const cChunkDef::BlockNibbles & a_SkyLightSource)
{
	m_BlockLights.SetAll(a_BlockLightSource);
	m_SkyLights.SetAll(a_SkyLightSource);
}





void ChunkLightData::SetSection(const SectionType & a_BlockLightSource, const SectionType & a_SkyLightSource, const size_t a_Y)
{
	m_BlockLights.SetSection(a_BlockLightSource, a_Y);
	m_SkyLights.SetSection(a_SkyLightSource, a_Y);
}





template struct ChunkDataStore<BLOCKTYPE, ChunkBlockData::SectionBlockCount, ChunkBlockData::DefaultValue>;
template struct ChunkDataStore<NIBBLETYPE, ChunkBlockData::SectionMetaCount, ChunkLightData::DefaultBlockLightValue>;
template struct ChunkDataStore<NIBBLETYPE, ChunkLightData::SectionLightCount, ChunkLightData::DefaultSkyLightValue>;
