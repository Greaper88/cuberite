#include "Globals.h"

#include "Bee.h"
#include "../BlockEntities/BeehiveEntity.h"
#include "../BlockInfo.h"
#include "../Chunk.h"
#include "../ChunkDef.h"
#include "../Entities/Player.h"
#include "../FastRandom.h"
#include "../LineBlockTracer.h"
#include "../World.h"





namespace
{
	constexpr int PollinationTicks = 400;
	constexpr int HiveSearchInterval = 60;
	constexpr int FlowerSearchInterval = 40;
	constexpr int SearchRadius = 8;

	bool IsSmallFlower(BLOCKTYPE a_Type)
	{
		return (a_Type == E_BLOCK_DANDELION) || (a_Type == E_BLOCK_FLOWER);
	}
}





cBee::cBee() :
	Super("", mtBee, "entity.bee.hurt", "entity.bee.death", "", 0.7f, 0.6f)
{
	SetMaxHealth(10);
	SetAttackDamage(2);
	SetSightDistance(10);
	GetMonsterConfig("Bee");
	SetHealth(GetMaxHealth());
	SetGravity(0);
	SetAirDrag(0.05f);
	SetCanPickUpLoot(false);
}





void cBee::BroadcastFlags()
{
	if (IsTicking())
	{
		GetWorld()->BroadcastEntityMetadata(*this);
	}
}





void cBee::SetHasNectar(bool a_HasNectar)
{
	if (m_HasNectar != a_HasNectar)
	{
		m_HasNectar = a_HasNectar;
		m_TicksSincePollination = 0;
		m_CropsGrownSincePollination = 0;
		BroadcastFlags();
	}
}





void cBee::SetHasStung(bool a_HasStung)
{
	if (m_HasStung != a_HasStung)
	{
		m_HasStung = a_HasStung;
		m_TicksSinceSting = 0;
		BroadcastFlags();
	}
}





void cBee::SetRolling(bool a_IsRolling)
{
	if (m_IsRolling != a_IsRolling)
	{
		m_IsRolling = a_IsRolling;
		BroadcastFlags();
	}
}





void cBee::SetAngerTicks(int a_Ticks)
{
	const auto NewTicks = std::max(a_Ticks, 0);
	if (m_AngerTicks != NewTicks)
	{
		m_AngerTicks = NewTicks;
		BroadcastFlags();
	}
}





void cBee::AngerAt(cPawn & a_Target)
{
	if (
		HasStung() || !IsTicking() || !a_Target.IsTicking() || (&a_Target == this) || (a_Target.GetWorld() != GetWorld()) ||
		(a_Target.IsPlayer() && !static_cast<cPlayer &>(a_Target).CanMobsTarget())
	)
	{
		return;
	}
	SetTarget(&a_Target);
	SetAngerTicks(GetRandomProvider().RandInt(400, 780));
}





bool cBee::DoTakeDamage(TakeDamageInfo & a_TDI)
{
	if (!cMonster::DoTakeDamage(a_TDI))
	{
		return false;
	}
	if (!HasStung() && (GetHealth() > 0) && (a_TDI.Attacker != nullptr) && a_TDI.Attacker->IsPawn())
	{
		AngerAt(static_cast<cPawn &>(*a_TDI.Attacker));
	}
	else if (HasStung())
	{
		SetTarget(nullptr);
	}
	return true;
}





void cBee::Tick(std::chrono::milliseconds a_Dt, cChunk & a_Chunk)
{
	Super::Tick(a_Dt, a_Chunk);
	if (!IsTicking() || (GetHealth() <= 0))
	{
		return;
	}

	if ((m_AngerTicks > 0) && (--m_AngerTicks == 0))
	{
		SetTarget(nullptr);
		m_IsRolling = false;
		BroadcastFlags();
	}
	if (m_CannotEnterHiveTicks > 0)
	{
		--m_CannotEnterHiveTicks;
	}
	if (m_HiveSearchCooldown > 0)
	{
		--m_HiveSearchCooldown;
	}
	if (m_FlowerSearchCooldown > 0)
	{
		--m_FlowerSearchCooldown;
	}
	if (!HasNectar() && (m_TicksSincePollination < std::numeric_limits<int>::max()))
	{
		++m_TicksSincePollination;
	}
	if (HasStung())
	{
		if (m_TicksSinceSting >= 1200)
		{
			TakeDamage(dtEnvironment, nullptr, static_cast<int>(std::ceil(GetHealth())), 0);
			return;
		}
		++m_TicksSinceSting;
	}

	const auto RequestedDestination = std::exchange(m_RequestedDestination, std::nullopt);
	if (PursueTarget())
	{
		return;
	}
	if (RequestedDestination.has_value())
	{
		m_PollinationTicks = 0;
		FlyTowards(*RequestedDestination);
		return;
	}
	if (IsLeashed())
	{
		return;
	}
	if (ShouldSeekHive())
	{
		m_PollinationTicks = 0;
		GrowCropsBelow();
		if (SeekHive())
		{
			return;
		}
	}
	else if (!HasStung() && !HasNectar() && !IsAngry() && SeekFlower())
	{
		return;
	}
	Wander();
}





void cBee::GetFollowedItems(cItems & a_Items)
{
	a_Items.Add(E_BLOCK_DANDELION);
	a_Items.Add(E_BLOCK_FLOWER);
}





void cBee::MoveToPosition(const Vector3d & a_Position)
{
	m_RequestedDestination = a_Position;
}





void cBee::HandlePhysics(std::chrono::milliseconds a_Dt, cChunk & a_Chunk)
{
	const auto PreviousPosition = GetPosition();
	Super::HandlePhysics(a_Dt, a_Chunk);
	const auto NewPosition = GetPosition();
	if (
		(cChunkDef::BlockToChunk(PreviousPosition) != cChunkDef::BlockToChunk(NewPosition)) &&
		!GetWorld()->SupportsBeesAt(NewPosition.Floor())
	)
	{
		SetPosition(PreviousPosition);
		SetSpeed({});
	}
}





bool cBee::PursueTarget()
{
	auto Target = GetTarget();
	if (!IsAngry() || HasStung() || (Target == nullptr))
	{
		SetRolling(false);
		return false;
	}
	if (
		!Target->IsTicking() || (Target->GetHealth() <= 0) ||
		(Target->IsPlayer() && !static_cast<cPlayer *>(Target)->CanMobsTarget())
	)
	{
		SetTarget(nullptr);
		SetRolling(false);
		return false;
	}

	const auto TargetPosition = Target->GetPosition().addedY(Target->GetHeight() * 0.5);
	const auto Distance = (TargetPosition - GetPosition()).SqrLength();
	SetRolling(Distance <= 4);
	m_PollinationTicks = 0;
	if (
		(Distance <= m_AttackRange * m_AttackRange) &&
		cLineBlockTracer::LineOfSightTrace(*GetWorld(), GetPosition(), TargetPosition, cLineBlockTracer::losAir)
	)
	{
		const auto OldHealth = Target->GetHealth();
		Target->TakeDamage(dtMobAttack, this, m_AttackDamage, GetKnockbackAmountAgainst(*Target));
		if (Target->GetHealth() < OldHealth)
		{
			if (Target->GetHealth() > 0)
			{
				Target->AddEntityEffect(cEntityEffect::effPoison, 200, 0);
			}
			GetWorld()->BroadcastSoundEffect("entity.bee.sting", GetPosition(), 1, 1);
			SetHasStung(true);
			SetRolling(false);
			SetTarget(nullptr);
			SetAngerTicks(0);
			return true;
		}
	}
	FlyTowards(TargetPosition);
	return true;
}





bool cBee::ShouldSeekHive() const
{
	if (HasStung() || IsAngry())
	{
		return false;
	}
	const auto World = GetWorld();
	const auto Time = World->GetTimeOfDay();
	const bool IsNight = (World->GetDimension() == dimOverworld) && (Time >= 13000_tick) && (Time < 23000_tick);
	return HasNectar() || IsNight || World->IsWeatherWet() || (m_TicksSincePollination >= 3600);
}





bool cBee::SeekHive()
{
	if (m_CannotEnterHiveTicks > 0)
	{
		return false;
	}

	std::optional<Vector3d> Entrance;
	if (m_HivePos.has_value() && ((Vector3d(*m_HivePos) - GetPosition()).SqrLength() < 32 * 32))
	{
		Entrance = GetHiveEntrance(*m_HivePos);
	}
	if (!Entrance.has_value())
	{
		if (m_HiveSearchCooldown > 0)
		{
			return false;
		}
		m_HiveSearchCooldown = HiveSearchInterval;
		m_HivePos = FindNearbyBlock(true);
		if (!m_HivePos.has_value())
		{
			return false;
		}
		Entrance = GetHiveEntrance(*m_HivePos);
		if (!Entrance.has_value())
		{
			return false;
		}
	}

	if ((GetPosition() - *Entrance).SqrLength() > 0.36)
	{
		FlyTowards(*Entrance);
		return true;
	}
	if (GetWorld()->DoWithBlockEntityAt(*m_HivePos, [this](cBlockEntity & a_Entity)
		{
			return (a_Entity.GetBlockType() == E_BLOCK_BEEHIVE) && static_cast<cBeehiveEntity &>(a_Entity).TryEnter(*this);
		}
	))
	{
		return true;
	}
	m_CannotEnterHiveTicks = HiveSearchInterval;
	return false;
}





bool cBee::SeekFlower()
{
	if (
		!m_FlowerPos.has_value() || ((Vector3d(*m_FlowerPos) - GetPosition()).SqrLength() >= 16 * 16) ||
		!IsFlowerAt(*m_FlowerPos)
	)
	{
		m_PollinationTicks = 0;
		if (m_FlowerSearchCooldown > 0)
		{
			return false;
		}
		m_FlowerSearchCooldown = FlowerSearchInterval;
		m_FlowerPos = FindNearbyBlock(false);
		if (!m_FlowerPos.has_value())
		{
			return false;
		}
	}

	const auto HoverPosition = Vector3d(*m_FlowerPos) + Vector3d(0.5, 0.7, 0.5);
	FlyTowards(HoverPosition);
	if ((GetPosition() - HoverPosition).SqrLength() <= 0.16)
	{
		if (++m_PollinationTicks >= PollinationTicks)
		{
			m_PollinationTicks = 0;
			SetHasNectar(true);
		}
	}
	else
	{
		m_PollinationTicks = 0;
	}
	return true;
}





std::optional<Vector3d> cBee::GetHiveEntrance(Vector3i a_Pos) const
{
	std::optional<Vector3d> Entrance;
	GetWorld()->DoWithChunkAt(a_Pos, [this, a_Pos, &Entrance](cChunk & a_Chunk)
		{
			const auto Entity = a_Chunk.GetBlockEntity(a_Pos);
			if ((Entity == nullptr) || (Entity->GetBlockType() != E_BLOCK_BEEHIVE) || !GetWorld()->SupportsBeesAt(a_Pos))
			{
				return false;
			}
			const auto & Hive = static_cast<const cBeehiveEntity &>(*Entity);
			const auto Position = Hive.GetEntrance();
			BLOCKTYPE Block;
			NIBBLETYPE Meta;
			if (
				Hive.HasSpace() && GetWorld()->GetBlockTypeMeta(Position, Block, Meta) &&
				!cBlockInfo::IsSolid(Block) && !IsBlockLiquid(Block) &&
				GetWorld()->SupportsBeesAt(Position)
			)
			{
				Entrance = Vector3d(Position) + Vector3d(0.5, 0.3, 0.5);
			}
			return true;
		}
	);
	return Entrance;
}





void cBee::GrowCropsBelow()
{
	if (!HasNectar() || (m_CropsGrownSincePollination >= 10) || !GetRandomProvider().RandBool(0.025))
	{
		return;
	}
	for (int Depth = 1; Depth <= 2; ++Depth)
	{
		const auto Position = GetPosition().Floor().addedY(-Depth);
		BLOCKTYPE Block;
		NIBBLETYPE Meta;
		if (!GetWorld()->GetBlockTypeMeta(Position, Block, Meta))
		{
			return;
		}
		if (
			(Block == E_BLOCK_WHEAT) || (Block == E_BLOCK_POTATOES) || (Block == E_BLOCK_CARROTS) ||
			(Block == E_BLOCK_BEETROOTS) || (Block == E_BLOCK_MELON_STEM) || (Block == E_BLOCK_PUMPKIN_STEM)
		)
		{
			if (GetWorld()->GrowPlantAt(Position) > 0)
			{
				++m_CropsGrownSincePollination;
			}
			return;
		}
		if (cBlockInfo::IsSolid(Block))
		{
			return;
		}
	}
}





bool cBee::IsFlowerAt(Vector3i a_Pos) const
{
	BLOCKTYPE Type;
	NIBBLETYPE Meta;
	if (!GetWorld()->GetBlockTypeMeta(a_Pos, Type, Meta))
	{
		return false;
	}
	if (IsSmallFlower(Type))
	{
		return true;
	}
	return (
		(Type == E_BLOCK_BIG_FLOWER) && ((Meta & 0x08) != 0) &&
		GetWorld()->GetBlockTypeMeta(a_Pos.addedY(-1), Type, Meta) && (Type == E_BLOCK_BIG_FLOWER) &&
		((Meta == E_META_BIG_FLOWER_SUNFLOWER) || (Meta == E_META_BIG_FLOWER_LILAC) ||
		(Meta == E_META_BIG_FLOWER_ROSE_BUSH) || (Meta == E_META_BIG_FLOWER_PEONY))
	);
}





std::optional<Vector3i> cBee::FindNearbyBlock(bool a_FindHive) const
{
	const auto Center = GetPosition().Floor();
	int BestDistance = SearchRadius * SearchRadius + 1;
	std::optional<Vector3i> BestPosition;
	for (int Y = -4; Y <= 4; ++Y)
	{
		for (int Z = -SearchRadius; Z <= SearchRadius; ++Z)
		{
			for (int X = -SearchRadius; X <= SearchRadius; ++X)
			{
				const int Distance = X * X + Y * Y + Z * Z;
				if (Distance >= BestDistance)
				{
					continue;
				}
				const auto Position = Center + Vector3i(X, Y, Z);
				if (a_FindHive)
				{
					BLOCKTYPE Block;
					NIBBLETYPE Meta;
					if (!GetWorld()->GetBlockTypeMeta(Position, Block, Meta) || (Block != E_BLOCK_BEEHIVE) || !GetHiveEntrance(Position).has_value())
					{
						continue;
					}
				}
				else if (!IsFlowerAt(Position) || !GetWorld()->SupportsBeesAt(Position))
				{
					continue;
				}
				BestPosition = Position;
				BestDistance = Distance;
			}
		}
	}
	return BestPosition;
}





bool cBee::CanFlyThrough(Vector3i a_Pos) const
{
	BLOCKTYPE Type;
	NIBBLETYPE Meta;
	return (
		GetWorld()->GetBlockTypeMeta(a_Pos, Type, Meta) && !cBlockInfo::IsSolid(Type) &&
		!IsBlockWater(Type) && !IsBlockLava(Type) && GetWorld()->SupportsBeesAt(a_Pos)
	);
}





void cBee::FlyTowards(Vector3d a_Destination)
{
	const auto Position = GetPosition();
	const auto Difference = a_Destination - Position;
	const auto Distance = Difference.Length();
	if (Distance < 0.1)
	{
		SetSpeed({});
		return;
	}
	const auto Direction = Difference * (1 / Distance);
	const auto Forward = Direction * std::min(0.7, Distance);
	const Vector3d Side(-Forward.z, 0, Forward.x);
	const std::array<Vector3d, 5> Moves = { Forward, Forward.addedY(0.7), Forward + Side, Forward - Side, Forward.addedY(-0.6) };
	for (const auto & Step : Moves)
	{
		const auto Next = Position + Step;
		if (Step.SqrLength() > 0.001 && CanFlyThrough(Next.Floor()) && CanFlyThrough(Next.addedY(GetHeight()).Floor()))
		{
			SetSpeed(Step.NormalizeCopy() * std::min(IsAngry() ? 4.5 : 3.0, Distance * 8));
			SetYawFromSpeed();
			SetHeadYaw(GetYaw());
			return;
		}
	}
	SetSpeed({});
}





void cBee::Wander()
{
	if ((m_WanderTicks > 0) && m_WanderDestination.has_value())
	{
		--m_WanderTicks;
		FlyTowards(*m_WanderDestination);
		return;
	}

	auto & Random = GetRandomProvider();
	m_WanderTicks = 30;
	m_WanderDestination = GetPosition() + Vector3d(Random.RandInt(-3, 3), Random.RandInt(-1, 1), Random.RandInt(-3, 3));
	FlyTowards(*m_WanderDestination);
}
