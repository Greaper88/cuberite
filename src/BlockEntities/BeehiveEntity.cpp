#include "Globals.h"

#include "BeehiveEntity.h"
#include "BlockInfo.h"
#include "BoundingBox.h"
#include "Chunk.h"
#include "Entities/Player.h"
#include "Mobs/Bee.h"
#include "World.h"
#include "WorldStorage/BeeNBT.h"





namespace
{
	constexpr std::array<eBlockFace, 4> FacingFaces{ BLOCK_FACE_ZM, BLOCK_FACE_ZP, BLOCK_FACE_XM, BLOCK_FACE_XP };
	constexpr std::array<const char *, 4> FacingNames{ "north", "south", "west", "east" };
	constexpr int MaximumHoneyLevel = 5;
}





cBeehiveEntity::cBeehiveEntity(const BLOCKTYPE a_BlockType, const NIBBLETYPE a_BlockMeta, const Vector3i a_Pos, cWorld * const a_World) :
	Super(a_BlockType, a_BlockMeta, a_Pos, a_World)
{
	ASSERT(a_BlockType == E_BLOCK_BEEHIVE);
}





Vector3i cBeehiveEntity::GetEntrance() const
{
	return AddFaceDirection(m_Pos, FacingFaces[m_BlockMeta & 3]);
}





void cBeehiveEntity::SetBlockMeta(const NIBBLETYPE a_Meta)
{
	m_BlockMeta = a_Meta;
	if (m_World != nullptr)
	{
		UpdateBlockState();
	}
}





void cBeehiveEntity::SetHoneyLevel(const int a_Level)
{
	m_HoneyLevel = std::clamp(a_Level, 0, MaximumHoneyLevel);
	if (m_World != nullptr)
	{
		UpdateBlockState();
	}
}





void cBeehiveEntity::SetOccupants(std::vector<Occupant> a_Occupants)
{
	m_Occupants = std::move(a_Occupants);
}





bool cBeehiveEntity::TryEnter(cBee & a_Bee)
{
	if (!HasSpace() || !a_Bee.IsTicking() || (a_Bee.GetWorld() != m_World) || a_Bee.HasStung() || !CanUseEntrance())
	{
		return false;
	}

	auto Data = BeeNBT::SaveOccupant(a_Bee);
	if (Data.empty())
	{
		return false;
	}

	m_Occupants.push_back({ std::move(Data), 0, a_Bee.HasNectar() ? 2400 : 600 });
	const auto Chunk = cChunkDef::BlockToChunk(m_Pos);
	m_World->MarkChunkDirty(Chunk.m_ChunkX, Chunk.m_ChunkZ);
	a_Bee.Destroy();
	return true;
}





void cBeehiveEntity::CopyFrom(const cBlockEntity & a_Src)
{
	Super::CopyFrom(a_Src);
	const auto & Src = static_cast<const cBeehiveEntity &>(a_Src);
	m_HoneyLevel = Src.m_HoneyLevel;
	m_Occupants = Src.m_Occupants;
}





void cBeehiveEntity::Destroy()
{
	// Chunk data replacement can also discard an old block entity at an intact hive.
	if (m_Occupants.empty() || (m_World == nullptr) || (m_World->GetBlock(m_Pos) == E_BLOCK_BEEHIVE))
	{
		return;
	}

	auto ReleasePos = GetEntrance();
	if (!CanUseEntrance())
	{
		ReleasePos = m_Pos;
		for (const auto Face : { BLOCK_FACE_YP, BLOCK_FACE_ZM, BLOCK_FACE_ZP, BLOCK_FACE_XM, BLOCK_FACE_XP, BLOCK_FACE_YM })
		{
			const auto Candidate = AddFaceDirection(m_Pos, Face);
			if (m_World->SupportsBeesAt(Candidate) && (m_World->GetBlock(Candidate) == E_BLOCK_AIR))
			{
				ReleasePos = Candidate;
				break;
			}
		}
	}

	for (const auto & Occupant : m_Occupants)
	{
		Release(Occupant, ReleasePos, false);
	}
}





void cBeehiveEntity::OnAddToWorld(cWorld & a_World, cChunk & a_Chunk)
{
	Super::OnAddToWorld(a_World, a_Chunk);
	UpdateBlockState();
}





bool cBeehiveEntity::Tick(std::chrono::milliseconds a_Dt, cChunk & a_Chunk)
{
	UNUSED(a_Dt);
	UNUSED(a_Chunk);

	if (m_Occupants.empty())
	{
		return false;
	}

	const bool CanRelease =
		(m_World->GetTimeOfDay() < 13000_tick) &&
		!m_World->IsWeatherWet() &&
		CanUseEntrance();

	auto Occupant = m_Occupants.begin();
	while (Occupant != m_Occupants.end())
	{
		if (Occupant->TicksInHive < std::numeric_limits<int>::max())
		{
			++Occupant->TicksInHive;
		}

		if (
			CanRelease &&
			(Occupant->TicksInHive >= Occupant->MinOccupationTicks) &&
			Release(*Occupant, GetEntrance(), true)
		)
		{
			Occupant = m_Occupants.erase(Occupant);
		}
		else
		{
			++Occupant;
		}
	}
	return true;
}





bool cBeehiveEntity::UsedBy(cPlayer * a_Player)
{
	if (m_HoneyLevel != MaximumHoneyLevel)
	{
		return false;
	}

	switch (a_Player->GetEquippedItem().m_ItemType)
	{
		case E_ITEM_SHEARS:
		{
			m_World->SpawnItemPickups(cItem(E_ITEM_HONEYCOMB, 3), m_Pos);
			a_Player->UseEquippedItem();
			break;
		}
		case E_ITEM_GLASS_BOTTLE:
		{
			cItem HoneyBottle(E_ITEM_HONEY_BOTTLE);
			if (!a_Player->IsGameModeCreative())
			{
				a_Player->ReplaceOneEquippedItemTossRest(HoneyBottle);
			}
			else if (a_Player->GetInventory().AddItem(HoneyBottle) == 0)
			{
				m_World->SpawnItemPickups(std::move(HoneyBottle), m_Pos);
			}
			break;
		}
		default: return false;
	}

	SetHoneyLevel(0);

	const auto Center = Vector3d(m_Pos) + Vector3d(0.5, 0.5, 0.5);
	const Vector3d Radius(8, 6, 8);
	m_World->ForEachEntityInBox(cBoundingBox(Center - Radius, Center + Radius), [a_Player](cEntity & a_Entity)
	{
		if (a_Entity.IsMob() && (static_cast<cMonster &>(a_Entity).GetMobType() == mtBee))
		{
			auto & Bee = static_cast<cBee &>(a_Entity);
			if (Bee.GetTarget() == nullptr)
			{
				Bee.AngerAt(*a_Player);
			}
		}
		return false;
	});

	const auto ReleasePos = GetEntrance();
	if (CanUseEntrance())
	{
		m_Occupants.erase(
			std::remove_if(m_Occupants.begin(), m_Occupants.end(), [this, ReleasePos, a_Player](const Occupant & a_Occupant)
			{
				return Release(a_Occupant, ReleasePos, false, a_Player);
			}),
			m_Occupants.end()
		);
	}
	return true;
}





void cBeehiveEntity::UpdateBlockState()
{
	m_World->UpdateNamedBlock(
		m_Pos,
		E_BLOCK_BEEHIVE,
		{
			IsNest() ? "minecraft:bee_nest" : "minecraft:beehive",
			{
				{ "facing", FacingNames[m_BlockMeta & 3] },
				{ "honey_level", fmt::format("{}", m_HoneyLevel) },
			},
		}
	);
}





bool cBeehiveEntity::CanUseEntrance() const
{
	if (m_World == nullptr)
	{
		return false;
	}
	const auto Entrance = GetEntrance();
	if (!m_World->SupportsBeesAt(Entrance))
	{
		return false;
	}
	const auto EntranceBlock = m_World->GetBlock(Entrance);
	return !cBlockInfo::IsSolid(EntranceBlock) && !IsBlockLiquid(EntranceBlock);
}





bool cBeehiveEntity::Release(
	const Occupant & a_Occupant, const Vector3i a_Position, const bool a_DeliverNectar, cPlayer * const a_AngerTarget
)
{
	auto Bee = BeeNBT::LoadOccupant(a_Occupant.EntityData, Vector3d(a_Position) + Vector3d(0.5, 0.15, 0.5));
	if (Bee == nullptr)
	{
		return false;
	}

	const bool DeliveredNectar = a_DeliverNectar && Bee->HasNectar();
	if (m_World->GetBlock(m_Pos) == E_BLOCK_BEEHIVE)
	{
		Bee->SetHivePos(m_Pos);
	}
	else
	{
		Bee->SetHivePos(std::nullopt);
	}
	Bee->SetCannotEnterHiveTicks(400);
	if (DeliveredNectar)
	{
		Bee->SetHasNectar(false);
		Bee->SetTicksSincePollination(0);
	}
	auto * BeePtr = Bee.get();
	if (!BeePtr->Initialize(std::move(Bee), *m_World))
	{
		return false;
	}

	if (a_AngerTarget != nullptr)
	{
		const auto BeeID = BeePtr->GetUniqueID();
		const auto PlayerID = a_AngerTarget->GetUniqueID();
		// Targeting requires the bee to be attached to a chunk.
		m_World->ScheduleTask(1_tick, [BeeID, PlayerID](cWorld & a_World)
		{
			a_World.DoWithEntityByID(BeeID, [&](cEntity & a_Bee)
			{
				if (a_Bee.IsTicking())
				{
					a_World.DoWithEntityByID(PlayerID, [&](cEntity & a_Player)
					{
						static_cast<cBee &>(a_Bee).AngerAt(static_cast<cPlayer &>(a_Player));
						return true;
					});
				}
				return true;
			});
		});
	}
	if (DeliveredNectar)
	{
		SetHoneyLevel(m_HoneyLevel + 1);
	}
	return true;
}
