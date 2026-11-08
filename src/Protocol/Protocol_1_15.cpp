#include "Globals.h"
#include "Protocol/Protocol_1_15.h"
#include "Protocol/Packetizer.h"
#include "Protocol/Palettes/Palette_1_15.h"
#include "Protocol/Palettes/Upgrade.h"

#include "ClientHandle.h"
#include "Entities/FallingBlock.h"
#include "Entities/Minecart.h"
#include "Entities/Player.h"
#include "JsonUtils.h"
#include "Mobs/Monster.h"
#include "Root.h"
#include "Server.h"





namespace
{
	// Cuberite supplies a 2D biome map. Use the same client-side biome zoom seed across joins and respawns.
	constexpr Int64 BiomeZoomSeed = 0;
}





void cProtocol_1_15_2::SendChunkView(cChunkCoords a_Center, int a_ViewDistance)
{
	if (m_ChunkViewDistance != a_ViewDistance)
	{
		cPacketizer Packet(*this, pktChunkViewDistance);
		Packet.WriteVarInt32(ToUnsigned(a_ViewDistance));
		m_ChunkViewDistance = a_ViewDistance;
	}

	if (!m_ChunkCenter.has_value() || (*m_ChunkCenter != a_Center))
	{
		cPacketizer Packet(*this, pktChunkCenter);
		Packet.WriteVarInt32(static_cast<UInt32>(a_Center.m_ChunkX));
		Packet.WriteVarInt32(static_cast<UInt32>(a_Center.m_ChunkZ));
		m_ChunkCenter = a_Center;
	}
}





void cProtocol_1_15_2::SendEditSign(Vector3i a_BlockPos)
{
	cPacketizer Packet(*this, pktEditSign);
	Packet.WriteXZYPosition64(a_BlockPos);
}





void cProtocol_1_15_2::SendEntitySpawn(const cEntity & a_Entity, UInt8 a_ObjectType, Int32 a_ObjectData)
{
	if (a_Entity.IsFallingBlock())
	{
		const auto & Block = static_cast<const cFallingBlock &>(a_Entity);
		a_ObjectData = static_cast<Int32>(GetProtocolBlockType(Block.GetBlockType(), Block.GetBlockMeta()));
	}
	else if (a_Entity.IsMinecart())
	{
		a_ObjectData = 0;  // The minecart payload is encoded in its entity type.
	}
	Super::SendEntitySpawn(a_Entity, a_ObjectType, a_ObjectData);
}





void cProtocol_1_15_2::SendInitRecipes(UInt32 a_RecipeID)
{
	// Recipe IDs are namespaced on this protocol. The legacy numeric recipe book is unavailable.
}





void cProtocol_1_15_2::SendLogin(const cPlayer & a_Player, const cWorld & a_World)
{
	const auto * Server = cRoot::Get()->GetServer();
	{
		cPacketizer Packet(*this, pktJoinGame);
		Packet.WriteBEUInt32(a_Player.GetUniqueID());
		Packet.WriteBEUInt8(static_cast<UInt8>(a_Player.GetEffectiveGameMode()) | (Server->IsHardcore() ? 0x08 : 0));
		Packet.WriteBEInt32(static_cast<Int32>(a_World.GetDimension()));
		Packet.WriteBEInt64(BiomeZoomSeed);
		Packet.WriteBEUInt8(static_cast<UInt8>(std::min<size_t>(Server->GetMaxPlayers(), 255)));
		Packet.WriteString("default");
		Packet.WriteVarInt32(ToUnsigned(a_World.GetMaxViewDistance()));
		Packet.WriteBool(false);  // Reduced debug info
		Packet.WriteBool(true);  // Show the respawn screen
	}
	{
		cPacketizer Packet(*this, pktSpawnPosition);
		Packet.WriteXZYPosition64(a_World.GetSpawnX(), a_World.GetSpawnY(), a_World.GetSpawnZ());
	}
	{
		cPacketizer Packet(*this, pktDifficulty);
		Packet.WriteBEUInt8(1);
		Packet.WriteBool(false);
	}
}





void cProtocol_1_15_2::SendParticleEffect(const AString & a_Name, Vector3f a_Position, Vector3f a_Offset, float a_Speed, int a_Count)
{
	SendParticleEffect(a_Name, a_Position, a_Offset, a_Speed, a_Count, {});
}





void cProtocol_1_15_2::SendParticleEffect(const AString & a_Name, Vector3f a_Position, Vector3f a_Offset, float a_Speed, int a_Count, std::array<int, 2> a_Data)
{
	const auto Type = GetProtocolParticleID(a_Name);
	if ((Type == 14) || (Type == 32))
	{
		// Dust and item particles require additional color / item fields.
		return;
	}
	cPacketizer Packet(*this, pktParticleEffect);
	Packet.WriteBEInt32(Type);
	Packet.WriteBool(false);
	Packet.WriteBEDouble(a_Position.x);
	Packet.WriteBEDouble(a_Position.y);
	Packet.WriteBEDouble(a_Position.z);
	Packet.WriteBEFloat(a_Offset.x);
	Packet.WriteBEFloat(a_Offset.y);
	Packet.WriteBEFloat(a_Offset.z);
	Packet.WriteBEFloat(a_Speed);
	Packet.WriteBEInt32(a_Count);
	if ((Type == 3) || (Type == 23))
	{
		Packet.WriteVarInt32(GetProtocolBlockType(static_cast<BLOCKTYPE>(a_Data[0]), static_cast<NIBBLETYPE>(a_Data[1])));
	}
}





void cProtocol_1_15_2::SendPlayerSpawn(const cPlayer & a_Player)
{
	{
		cPacketizer Packet(*this, pktSpawnOtherPlayer);
		const auto Position = a_Player.GetLastSentPosition();
		Packet.WriteVarInt32(a_Player.GetUniqueID());
		Packet.WriteUUID(a_Player.GetUUID());
		Packet.WriteBEDouble(Position.x);
		Packet.WriteBEDouble(Position.y);
		Packet.WriteBEDouble(Position.z);
		Packet.WriteByteAngle(a_Player.GetYaw());
		Packet.WriteByteAngle(a_Player.GetPitch());
	}
	SendEntityMetadata(a_Player);
}





void cProtocol_1_15_2::SendRespawn(eDimension a_Dimension)
{
	cPacketizer Packet(*this, pktRespawn);
	Packet.WriteBEInt32(static_cast<Int32>(a_Dimension));
	Packet.WriteBEInt64(BiomeZoomSeed);
	Packet.WriteBEUInt8(static_cast<UInt8>(m_Client->GetPlayer()->GetEffectiveGameMode()));
	Packet.WriteString("default");
	m_ChunkCenter.reset();
	m_ChunkViewDistance = 0;
}





void cProtocol_1_15_2::SendSoundEffect(const AString & a_Name, Vector3d a_Position, float a_Volume, float a_Pitch)
{
	cPacketizer Packet(*this, pktSoundEffect);
	Packet.WriteString(a_Name);
	Packet.WriteVarInt32(0);  // Master sound category
	Packet.WriteBEInt32(static_cast<Int32>(a_Position.x * 8));
	Packet.WriteBEInt32(static_cast<Int32>(a_Position.y * 8));
	Packet.WriteBEInt32(static_cast<Int32>(a_Position.z * 8));
	Packet.WriteBEFloat(a_Volume);
	Packet.WriteBEFloat(a_Pitch);
}





void cProtocol_1_15_2::SendSoundParticleEffect(EffectID a_EffectID, Vector3i a_Position, int a_Data)
{
	cPacketizer Packet(*this, pktSoundParticleEffect);
	Packet.WriteBEInt32(static_cast<Int32>(a_EffectID));
	Packet.WriteXZYPosition64(a_Position);
	if (a_EffectID == EffectID::PARTICLE_BLOCK_BREAK)
	{
		a_Data = static_cast<Int32>(GetProtocolBlockType(static_cast<BLOCKTYPE>(a_Data & 0xfff), static_cast<NIBBLETYPE>((a_Data >> 12) & 15)));
	}
	Packet.WriteBEInt32(a_Data);
	Packet.WriteBool(false);
}





void cProtocol_1_15_2::SendSpawnMob(const cMonster & a_Mob)
{
	const auto Type = GetProtocolMobType(a_Mob.GetMobType());
	if (Type == 0)
	{
		return;
	}

	{
		cPacketizer Packet(*this, pktSpawnMob);
		const auto Position = a_Mob.GetLastSentPosition();
		Packet.WriteVarInt32(a_Mob.GetUniqueID());
		Packet.WriteBEUInt64(0);
		Packet.WriteBEUInt64(a_Mob.GetUniqueID());
		Packet.WriteVarInt32(Type);
		Packet.WriteBEDouble(Position.x);
		Packet.WriteBEDouble(Position.y);
		Packet.WriteBEDouble(Position.z);
		Packet.WriteByteAngle(a_Mob.GetYaw());
		Packet.WriteByteAngle(a_Mob.GetPitch());
		Packet.WriteByteAngle(a_Mob.GetHeadYaw());
		Packet.WriteBEInt16(static_cast<Int16>(a_Mob.GetSpeedX() * 400));
		Packet.WriteBEInt16(static_cast<Int16>(a_Mob.GetSpeedY() * 400));
		Packet.WriteBEInt16(static_cast<Int16>(a_Mob.GetSpeedZ() * 400));
	}
	SendEntityMetadata(a_Mob);
}





void cProtocol_1_15_2::SendUnlockRecipe(UInt32 a_RecipeID)
{
	// See SendInitRecipes.
}





UInt8 cProtocol_1_15_2::GetEntityMetadataID(EntityMetadata a_Field) const
{
	const auto Index = Super::GetEntityMetadataID(a_Field);
	if ((a_Field == EntityMetadata::PlayerDisplayedSkinParts) || (a_Field == EntityMetadata::PlayerMainHand))
	{
		// Living entities added a stinger count ahead of player-specific fields.
		return Index + 1;
	}
	return Index;
}





std::pair<short, short> cProtocol_1_15_2::GetItemFromProtocolID(UInt32 a_ProtocolID) const
{
	return PaletteUpgrade::ToItem(Palette_1_15::ToItem(a_ProtocolID));
}





UInt32 cProtocol_1_15_2::GetPacketID(ePacketType a_PacketType) const
{
	// Play-state IDs are explicit because several inherited 1.14 IDs still use older layouts.
	switch (a_PacketType)
	{
		case pktAcknowledgeBlockDig:     return 0x08;
		case pktAttachEntity:           return 0x4b;
		case pktBlockAction:            return 0x0b;
		case pktBlockBreakAnim:         return 0x09;
		case pktBlockChange:            return 0x0c;
		case pktBlockChanges:           return 0x10;
		case pktBossBar:                return 0x0d;
		case pktCameraSetTo:            return 0x3f;
		case pktChatRaw:                return 0x0f;
		case pktChunkCenter:            return 0x41;
		case pktChunkViewDistance:      return 0x42;
		case pktCollectEntity:          return 0x56;
		case pktDestroyEntity:          return 0x38;
		case pktDifficulty:             return 0x0e;
		case pktDisconnectDuringGame:   return 0x1b;
		case pktDisplayObjective:       return 0x43;
		case pktEditSign:               return 0x30;
		case pktEntityAnimation:        return 0x06;
		case pktEntityEffect:           return 0x5a;
		case pktEntityEquipment:        return 0x47;
		case pktEntityHeadLook:         return 0x3c;
		case pktEntityLook:             return 0x2b;
		case pktEntityMeta:             return 0x44;
		case pktEntityProperties:       return 0x59;
		case pktEntityRelMove:          return 0x29;
		case pktEntityRelMoveLook:      return 0x2a;
		case pktEntityStatus:           return 0x1c;
		case pktEntityVelocity:         return 0x46;
		case pktExperience:             return 0x48;
		case pktExplosion:              return 0x1d;
		case pktGameMode:               return 0x1f;
		case pktHeldItemChange:         return 0x40;
		case pktHorseWindowOpen:        return 0x20;
		case pktInventorySlot:          return 0x17;
		case pktJoinGame:               return 0x26;
		case pktKeepAlive:              return 0x21;
		case pktLeashEntity:            return 0x45;
		case pktMapData:                return 0x27;
		case pktParticleEffect:         return 0x24;
		case pktPlayerAbilities:        return 0x32;
		case pktPlayerList:             return 0x34;
		case pktPlayerListHeaderFooter: return 0x54;
		case pktPlayerMoveLook:         return 0x36;
		case pktPluginMessage:          return 0x19;
		case pktRemoveEntityEffect:     return 0x39;
		case pktResourcePack:           return 0x3a;
		case pktRespawn:                return 0x3b;
		case pktScoreboardObjective:    return 0x4a;
		case pktSoundEffect:            return 0x1a;
		case pktSoundParticleEffect:    return 0x23;
		case pktSpawnExperienceOrb:     return 0x01;
		case pktSpawnGlobalEntity:       return 0x02;
		case pktSpawnMob:               return 0x03;
		case pktSpawnObject:            return 0x00;
		case pktSpawnOtherPlayer:       return 0x05;
		case pktSpawnPainting:          return 0x04;
		case pktSpawnPosition:          return 0x4e;
		case pktStatistics:             return 0x07;
		case pktTabCompletionResults:   return 0x11;
		case pktTeleportEntity:         return 0x57;
		case pktTimeUpdate:             return 0x4f;
		case pktTitle:                  return 0x50;
		case pktUnloadChunk:            return 0x1e;
		case pktUnlockRecipe:           return 0x37;
		case pktUpdateBlockEntity:      return 0x0a;
		case pktUpdateHealth:           return 0x49;
		case pktUpdateScore:            return 0x4d;
		case pktWeather:                return 0x1f;
		case pktWindowClose:            return 0x14;
		case pktWindowItems:            return 0x15;
		case pktWindowOpen:             return 0x2f;
		case pktWindowProperty:         return 0x16;
		default:                       return Super::GetPacketID(a_PacketType);
	}
}





UInt32 cProtocol_1_15_2::GetProtocolBlockType(BLOCKTYPE a_Type, NIBBLETYPE a_Meta) const
{
	return Palette_1_15::From(PaletteUpgrade::FromBlock(a_Type, a_Meta));
}





UInt8 cProtocol_1_15_2::GetProtocolEntityType(const cEntity & a_Entity) const
{
	if (a_Entity.IsMinecart())
	{
		switch (static_cast<const cMinecart &>(a_Entity).GetPayload())
		{
			case cMinecart::mpNone:    return 42;
			case cMinecart::mpChest:   return 43;
			case cMinecart::mpFurnace: return 45;
			case cMinecart::mpHopper:  return 46;
			case cMinecart::mpTNT:     return 48;
		}
	}

	// Bee was inserted at ID 4 in the entity registry.
	const auto Type = Super::GetProtocolEntityType(a_Entity);
	return (Type >= 4) ? static_cast<UInt8>(Type + 1) : Type;
}





UInt32 cProtocol_1_15_2::GetProtocolItemType(short a_Type, short a_Damage) const
{
	return Palette_1_15::From(PaletteUpgrade::FromItem(a_Type, a_Damage));
}





UInt32 cProtocol_1_15_2::GetProtocolMobType(eMonsterType a_Type) const
{
	const auto Type = Super::GetProtocolMobType(a_Type);
	return (Type >= 4) ? Type + 1 : Type;
}





UInt32 cProtocol_1_15_2::GetProtocolStatisticType(CustomStatistic a_Statistic) const
{
	return Palette_1_15::From(a_Statistic);
}





cProtocol::Version cProtocol_1_15_2::GetProtocolVersion() const
{
	return Version::v1_15_2;
}





bool cProtocol_1_15_2::HandlePacket(cByteBuffer & a_Buffer, UInt32 a_PacketType)
{
	if (m_State != State::Game)
	{
		return Super::HandlePacket(a_Buffer, a_PacketType);
	}
	const auto * Player = m_Client->GetPlayer();
	if ((Player == nullptr) || (Player->GetWorld() == nullptr))
	{
		return a_Buffer.SkipRead(a_Buffer.GetReadableSpace());
	}

	switch (a_PacketType)
	{
		case 0x00: HandleConfirmTeleport(a_Buffer); return true;
		case 0x03: HandlePacketChatMessage(a_Buffer); return true;
		case 0x04: HandlePacketClientStatus(a_Buffer); return true;
		case 0x05: HandlePacketClientSettings(a_Buffer); return true;
		case 0x08: HandlePacketEnchantItem(a_Buffer); return true;
		case 0x09: HandlePacketWindowClick(a_Buffer); return true;
		case 0x0a: HandlePacketWindowClose(a_Buffer); return true;
		case 0x0b: HandlePacketPluginMessage(a_Buffer); return true;
		case 0x0e: HandlePacketUseEntity(a_Buffer); return true;
		case 0x0f: HandlePacketKeepAlive(a_Buffer); return true;
		case 0x11: HandlePacketPlayerPos(a_Buffer); return true;
		case 0x12: HandlePacketPlayerPosLook(a_Buffer); return true;
		case 0x13: HandlePacketPlayerLook(a_Buffer); return true;
		case 0x14: HandlePacketPlayer(a_Buffer); return true;
		case 0x15: HandlePacketVehicleMove(a_Buffer); return true;
		case 0x16: HandlePacketBoatSteer(a_Buffer); return true;
		case 0x19: HandlePacketPlayerAbilities(a_Buffer); return true;
		case 0x1a: HandlePacketBlockDig(a_Buffer); return true;
		case 0x1b: HandlePacketEntityAction(a_Buffer); return true;
		case 0x1c: HandlePacketSteerVehicle(a_Buffer); return true;
		case 0x1e: HandlePacketNameItem(a_Buffer); return true;
		case 0x1f: HandlePacketResourcePackStatus(a_Buffer); return true;
		case 0x20: HandlePacketAdvancementTab(a_Buffer); return true;
		case 0x22: HandlePacketSetBeaconEffect(a_Buffer); return true;
		case 0x23: HandlePacketSlotSelect(a_Buffer); return true;
		case 0x26: HandlePacketCreativeInventoryAction(a_Buffer); return true;
		case 0x29: HandlePacketUpdateSign(a_Buffer); return true;
		case 0x2a: HandlePacketAnimation(a_Buffer); return true;
		case 0x2b: HandlePacketSpectate(a_Buffer); return true;
		case 0x2c: HandlePacketBlockPlace(a_Buffer); return true;
		case 0x2d: HandlePacketUseItem(a_Buffer); return true;

		// Known packets for features that have no handler in this adapter.
		case 0x01:  // Query block NBT
		case 0x02:  // Set difficulty
		case 0x06:  // Command suggestions
		case 0x07:  // Confirm transaction
		case 0x0c:  // Edit book
		case 0x0d:  // Query entity NBT
		case 0x10:  // Lock difficulty
		case 0x17:  // Pick item
		case 0x18:  // Craft recipe
		case 0x1d:  // Recipe book settings
		case 0x21:  // Select trade
		case 0x24:  // Update command block
		case 0x25:  // Update command block minecart
		case 0x27:  // Update jigsaw block
		case 0x28:  // Update structure block
		{
			return a_Buffer.SkipRead(a_Buffer.GetReadableSpace());
		}
		default: return false;
	}
}





void cProtocol_1_15_2::HandlePacketBlockDig(cByteBuffer & a_Buffer)
{
	HANDLE_READ(a_Buffer, ReadVarInt32, UInt32, Status);
	HANDLE_READ(a_Buffer, ReadXZYPosition64, Vector3i, Position);
	HANDLE_READ(a_Buffer, ReadBEInt8, Int8, Face);
	if (Status > 6)
	{
		return;
	}

	m_Client->HandleLeftClick(Position, FaceIntToBlockFace(Face), static_cast<UInt8>(Status));
	if (Status > 2)
	{
		return;
	}

	BLOCKTYPE Type;
	NIBBLETYPE Meta;
	if (m_Client->GetPlayer()->GetWorld()->GetBlockTypeMeta(Position, Type, Meta))
	{
		cPacketizer Packet(*this, pktAcknowledgeBlockDig);
		Packet.WriteXZYPosition64(Position);
		Packet.WriteVarInt32(GetProtocolBlockType(Type, Meta));
		Packet.WriteVarInt32(Status);
		// Always reconcile with the server's block state, including plugin vetoes.
		Packet.WriteBool(false);
	}
}





void cProtocol_1_15_2::HandlePacketUpdateSign(cByteBuffer & a_Buffer)
{
	HANDLE_READ(a_Buffer, ReadXZYPosition64, Vector3i, Position);
	HANDLE_READ(a_Buffer, ReadVarUTF8String, AString, Line1);
	HANDLE_READ(a_Buffer, ReadVarUTF8String, AString, Line2);
	HANDLE_READ(a_Buffer, ReadVarUTF8String, AString, Line3);
	HANDLE_READ(a_Buffer, ReadVarUTF8String, AString, Line4);
	m_Client->HandleUpdateSign(Position, Line1, Line2, Line3, Line4);
}





void cProtocol_1_15_2::WriteEntityMetadata(cPacketizer & a_Packet, const cEntity & a_Entity) const
{
	if (!a_Entity.IsPawn())
	{
		Super::WriteEntityMetadata(a_Packet, a_Entity);
		return;
	}

	// Specialised mob fields need a version-specific audit before being sent to 1.15 clients.
	WriteEntityFlags(a_Packet, a_Entity);
	Super::WriteEntityMetadata(a_Packet, EntityMetadata::LivingHealth, EntityMetadataType::Float);
	a_Packet.WriteBEFloat(static_cast<float>(a_Entity.GetHealth()));
	if (a_Entity.IsPlayer())
	{
		const auto & Player = static_cast<const cPlayer &>(a_Entity);
		Super::WriteEntityMetadata(a_Packet, EntityMetadata::PlayerDisplayedSkinParts, EntityMetadataType::Byte);
		a_Packet.WriteBEUInt8(static_cast<UInt8>(Player.GetSkinParts()));
		Super::WriteEntityMetadata(a_Packet, EntityMetadata::PlayerMainHand, EntityMetadataType::Byte);
		a_Packet.WriteBEUInt8(Player.IsLeftHanded() ? 0 : 1);
	}
	else
	{
		const auto & Mob = static_cast<const cMonster &>(a_Entity);
		if (Mob.HasCustomName())
		{
			Super::WriteEntityMetadata(a_Packet, EntityMetadata::EntityCustomName, EntityMetadataType::OptChat);
			a_Packet.WriteBool(true);
			a_Packet.WriteString(JsonUtils::SerializeSingleValueJsonObject("text", Mob.GetCustomName()));
			Super::WriteEntityMetadata(a_Packet, EntityMetadata::EntityCustomNameVisible, EntityMetadataType::Boolean);
			a_Packet.WriteBool(Mob.IsCustomNameAlwaysVisible());
		}
	}
}
