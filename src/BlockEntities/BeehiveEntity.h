#pragma once

#include "BlockEntity.h"
#include "BlockType.h"





class cBee;

class cBeehiveEntity final :
	public cBlockEntity
{
	using Super = cBlockEntity;

public:

	struct Occupant
	{
		ContiguousByteBuffer EntityData;
		int TicksInHive;
		int MinOccupationTicks;
	};

	cBeehiveEntity(BLOCKTYPE a_BlockType, NIBBLETYPE a_BlockMeta, Vector3i a_Pos, cWorld * a_World);

	bool IsNest() const { return (m_BlockMeta & E_META_BEEHIVE_NEST) != 0; }
	bool HasSpace() const { return m_Occupants.size() < 3; }
	Vector3i GetEntrance() const;
	void SetBlockMeta(NIBBLETYPE a_Meta);

	int GetHoneyLevel() const { return m_HoneyLevel; }
	void SetHoneyLevel(int a_Level);

	const std::vector<Occupant> & GetOccupants() const { return m_Occupants; }
	void SetOccupants(std::vector<Occupant> a_Occupants);

	/** Stores the bee and removes it from the active world if it can enter. */
	bool TryEnter(cBee & a_Bee);

private:

	virtual void CopyFrom(const cBlockEntity & a_Src) override;
	virtual void Destroy() override;
	virtual void OnAddToWorld(cWorld & a_World, cChunk & a_Chunk) override;
	virtual void SendTo(cClientHandle &) override {}
	virtual bool Tick(std::chrono::milliseconds a_Dt, cChunk & a_Chunk) override;
	virtual bool UsedBy(cPlayer * a_Player) override;

	void UpdateBlockState();
	bool CanUseEntrance() const;
	bool Release(const Occupant & a_Occupant, Vector3i a_Position, bool a_DeliverNectar, cPlayer * a_AngerTarget = nullptr);

	int m_HoneyLevel = 0;
	std::vector<Occupant> m_Occupants;
};
