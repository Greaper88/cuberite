#pragma once

#include "PassiveMonster.h"

#include <optional>





class cBee final :
	public cPassiveMonster
{
	using Super = cPassiveMonster;

public:
	cBee();

	CLASS_PROTODEF(cBee)

	bool HasNectar() const { return m_HasNectar; }
	void SetHasNectar(bool a_HasNectar);
	bool HasStung() const { return m_HasStung; }
	void SetHasStung(bool a_HasStung);
	bool IsRolling() const { return m_IsRolling; }
	bool IsAngry() const { return m_AngerTicks > 0; }
	void AngerAt(cPawn & a_Target);

	std::optional<Vector3i> GetHivePos() const { return m_HivePos; }
	void SetHivePos(std::optional<Vector3i> a_HivePos) { m_HivePos = a_HivePos; }
	std::optional<Vector3i> GetFlowerPos() const { return m_FlowerPos; }
	void SetFlowerPos(std::optional<Vector3i> a_FlowerPos) { m_FlowerPos = a_FlowerPos; }
	int GetCannotEnterHiveTicks() const { return m_CannotEnterHiveTicks; }
	void SetCannotEnterHiveTicks(int a_Ticks) { m_CannotEnterHiveTicks = std::max(a_Ticks, 0); }
	int GetTicksSincePollination() const { return m_TicksSincePollination; }
	void SetTicksSincePollination(int a_Ticks) { m_TicksSincePollination = std::max(a_Ticks, 0); }
	int GetCropsGrownSincePollination() const { return m_CropsGrownSincePollination; }
	void SetCropsGrownSincePollination(int a_Count) { m_CropsGrownSincePollination = std::max(a_Count, 0); }
	int GetTicksSinceSting() const { return m_TicksSinceSting; }
	void SetTicksSinceSting(int a_Ticks) { m_TicksSinceSting = std::max(a_Ticks, 0); }
	int GetAngerTicks() const { return m_AngerTicks; }
	void SetAngerTicks(int a_Ticks);

	void Tick(std::chrono::milliseconds a_Dt, cChunk & a_Chunk) override;
	void MoveToPosition(const Vector3d & a_Position) override;
	bool DoTakeDamage(TakeDamageInfo & a_TDI) override;

private:
	void HandlePhysics(std::chrono::milliseconds a_Dt, cChunk & a_Chunk) override;
	void HandleFalling() override {}
	void InStateIdle(std::chrono::milliseconds, cChunk &) override {}
	void InStateChasing(std::chrono::milliseconds, cChunk &) override {}
	void InStateEscaping(std::chrono::milliseconds, cChunk &) override {}
	void GetFollowedItems(cItems & a_Items) override;

	void BroadcastFlags();
	void SetRolling(bool a_IsRolling);
	bool PursueTarget();
	bool ShouldSeekHive() const;
	bool SeekHive();
	bool SeekFlower();
	void GrowCropsBelow();
	std::optional<Vector3d> GetHiveEntrance(Vector3i a_Pos) const;
	bool IsFlowerAt(Vector3i a_Pos) const;
	std::optional<Vector3i> FindNearbyBlock(bool a_FindHive) const;
	bool CanFlyThrough(Vector3i a_Pos) const;
	void FlyTowards(Vector3d a_Destination);
	void Wander();

	std::optional<Vector3i> m_HivePos;
	std::optional<Vector3i> m_FlowerPos;
	std::optional<Vector3d> m_RequestedDestination;
	std::optional<Vector3d> m_WanderDestination;
	int m_CannotEnterHiveTicks = 0;
	int m_TicksSincePollination = 0;
	int m_CropsGrownSincePollination = 0;
	int m_TicksSinceSting = 0;
	int m_AngerTicks = 0;
	int m_HiveSearchCooldown = 0;
	int m_FlowerSearchCooldown = 0;
	int m_PollinationTicks = 0;
	int m_WanderTicks = 0;
	bool m_HasNectar = false;
	bool m_HasStung = false;
	bool m_IsRolling = false;
};
