#pragma once

#include "BlockEntity.h"
#include "Item.h"
#include "NamedBlockState.h"





/** Models a campfire's smoke and block state. Java cooking data is retained by paletted storage. */
class cCampfireEntity final :
	public cBlockEntity
{
	using Super = cBlockEntity;

public:

	cCampfireEntity(BLOCKTYPE a_BlockType, NIBBLETYPE a_BlockMeta, Vector3i a_Pos, cWorld * a_World);

	bool IsWaterlogged() const { return m_Waterlogged; }
	void SetWaterlogged(bool a_Waterlogged);
	void SetBlockMeta(NIBBLETYPE a_Meta);
	void SetImportedItems(cItems a_Items) { m_ImportedItems = std::move(a_Items); }
	cNamedBlockState GetBlockState() const;

	/** Campfires under hives work through a vertical smoke column up to five blocks tall. */
	static bool IsSmokey(const cWorld & a_World, Vector3i a_Position);

private:

	void CopyFrom(const cBlockEntity & a_Src) override;
	cItems ConvertToPickups() const override;
	void OnAddToWorld(cWorld & a_World, cChunk & a_Chunk) override;
	void SendTo(cClientHandle & a_Client) override {}
	bool UsedBy(cPlayer * a_Player) override;

	void UpdateBlockState();

	bool m_Waterlogged = false;
	cItems m_ImportedItems;
};
