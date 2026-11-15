#pragma once

#include "BlockEntity.h"
#include "Item.h"
#include "NamedBlockState.h"





/** Models a campfire's smoke, block state, and four independently cooking slots. */
class cCampfireEntity final :
	public cBlockEntity
{
	using Super = cBlockEntity;

public:

	cCampfireEntity(BLOCKTYPE a_BlockType, NIBBLETYPE a_BlockMeta, Vector3i a_Pos, cWorld * a_World);

	bool IsWaterlogged() const { return m_Waterlogged; }
	void SetWaterlogged(bool a_Waterlogged);
	void SetBlockMeta(NIBBLETYPE a_Meta);
	struct cCookingSlot
	{
		cItem Item;
		int Progress = 0;
		int Total = 0;
	};
	using cCookingSlots = std::array<cCookingSlot, 4>;

	const cCookingSlots & GetCookingSlots() const { return m_CookingSlots; }
	void SetCookingSlots(cCookingSlots a_Slots) { m_CookingSlots = std::move(a_Slots); }
	cNamedBlockState GetBlockState() const;

	/** Campfires under hives work through a vertical smoke column up to five blocks tall. */
	static bool IsSmokey(const cWorld & a_World, Vector3i a_Position);

private:

	void CopyFrom(const cBlockEntity & a_Src) override;
	cItems ConvertToPickups() const override;
	void OnAddToWorld(cWorld & a_World, cChunk & a_Chunk) override;
	void SendTo(cClientHandle & a_Client) override;
	bool Tick(std::chrono::milliseconds a_Dt, cChunk & a_Chunk) override;
	bool UsedBy(cPlayer * a_Player) override;

	bool EjectSlot(cCookingSlot & a_Slot, const cItem & a_Stack);
	void OnContentsChanged();
	void UpdateBlockState();

	bool m_Waterlogged = false;
	cCookingSlots m_CookingSlots;
};
