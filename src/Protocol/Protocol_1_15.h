#pragma once

#include "Protocol_1_14.h"

#include <optional>





/** Experimental Java 1.15.2 adapter for Cuberite's existing world simulation. */
class cProtocol_1_15_2 final:
	public cProtocol_1_14_4
{
	using Super = cProtocol_1_14_4;

public:
	using Super::Super;

protected:
	void SendChunkView(cChunkCoords a_Center, int a_ViewDistance) override;
	void SendEditSign(Vector3i a_BlockPos) override;
	void SendEntitySpawn(const cEntity & a_Entity, UInt8 a_ObjectType, Int32 a_ObjectData) override;
	void SendInitRecipes(UInt32 a_RecipeID) override;
	void SendLogin(const cPlayer & a_Player, const cWorld & a_World) override;
	void SendParticleEffect(const AString & a_Name, Vector3f a_Position, Vector3f a_Offset, float a_Speed, int a_Count) override;
	void SendParticleEffect(const AString & a_Name, Vector3f a_Position, Vector3f a_Offset, float a_Speed, int a_Count, std::array<int, 2> a_Data) override;
	void SendPlayerSpawn(const cPlayer & a_Player) override;
	void SendRespawn(eDimension a_Dimension) override;
	void SendSoundEffect(const AString & a_Name, Vector3d a_Position, float a_Volume, float a_Pitch) override;
	void SendSoundParticleEffect(EffectID a_EffectID, Vector3i a_Position, int a_Data) override;
	void SendSpawnMob(const cMonster & a_Mob) override;
	void SendUnlockRecipe(UInt32 a_RecipeID) override;

	UInt8 GetEntityMetadataID(EntityMetadata a_Field) const override;
	std::pair<short, short> GetItemFromProtocolID(UInt32 a_ProtocolID) const override;
	UInt32 GetPacketID(ePacketType a_PacketType) const override;
	UInt32 GetProtocolBlockType(BLOCKTYPE a_Type, NIBBLETYPE a_Meta) const override;
	UInt8 GetProtocolEntityType(const cEntity & a_Entity) const override;
	UInt32 GetProtocolItemType(short a_Type, short a_Damage) const override;
	UInt32 GetProtocolMobType(eMonsterType a_Type) const override;
	UInt32 GetProtocolStatisticType(CustomStatistic a_Statistic) const override;
	Version GetProtocolVersion() const override;

	bool HandlePacket(cByteBuffer & a_Buffer, UInt32 a_PacketType) override;
	void HandlePacketBlockDig(cByteBuffer & a_Buffer) override;
	void HandlePacketUpdateSign(cByteBuffer & a_Buffer) override;
	void WriteEntityMetadata(cPacketizer & a_Packet, const cEntity & a_Entity) const override;

private:
	std::optional<cChunkCoords> m_ChunkCenter;
	int m_ChunkViewDistance = 0;
};
