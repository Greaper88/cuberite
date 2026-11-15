#include "Globals.h"
#include "BeeNBT.h"

#include "FastNBT.h"
#include "../BlockEntities/BeehiveEntity.h"
#include "../Mobs/Bee.h"





namespace
{
	int ReadInt(const cParsedNBT & a_NBT, int a_Tag, std::string_view a_Name, int a_Default = 0)
	{
		const auto Field = a_NBT.FindChildByName(a_Tag, a_Name.data(), a_Name.size());
		return ((Field >= 0) && (a_NBT.GetType(Field) == TAG_Int)) ? a_NBT.GetInt(Field) : a_Default;
	}

	bool ReadFlag(const cParsedNBT & a_NBT, int a_Tag, std::string_view a_Name)
	{
		const auto Field = a_NBT.FindChildByName(a_Tag, a_Name.data(), a_Name.size());
		return (Field >= 0) && (a_NBT.GetType(Field) == TAG_Byte) && (a_NBT.GetByte(Field) != 0);
	}

	std::optional<Vector3i> ReadPosition(const cParsedNBT & a_NBT, int a_Tag, std::string_view a_Name)
	{
		const auto Position = a_NBT.FindChildByName(a_Tag, a_Name.data(), a_Name.size());
		if ((Position < 0) || (a_NBT.GetType(Position) != TAG_Compound))
		{
			return std::nullopt;
		}
		const auto X = a_NBT.FindChildByName(Position, "X");
		const auto Y = a_NBT.FindChildByName(Position, "Y");
		const auto Z = a_NBT.FindChildByName(Position, "Z");
		if (
			(X < 0) || (a_NBT.GetType(X) != TAG_Int) ||
			(Y < 0) || (a_NBT.GetType(Y) != TAG_Int) ||
			(Z < 0) || (a_NBT.GetType(Z) != TAG_Int)
		)
		{
			return std::nullopt;
		}
		const Vector3i Result(a_NBT.GetInt(X), a_NBT.GetInt(Y), a_NBT.GetInt(Z));
		constexpr int WorldLimit = 30000000;
		if (
			(Result.x < -WorldLimit) || (Result.x >= WorldLimit) ||
			(Result.z < -WorldLimit) || (Result.z >= WorldLimit) || !cChunkDef::IsValidHeight(Result)
		)
		{
			return std::nullopt;
		}
		return Result;
	}

	void WritePosition(cFastNBTWriter & a_Writer, const AString & a_Name, const std::optional<Vector3i> & a_Pos)
	{
		if (a_Pos.has_value())
		{
			a_Writer.BeginCompound(a_Name);
			a_Writer.AddInt("X", a_Pos->x);
			a_Writer.AddInt("Y", a_Pos->y);
			a_Writer.AddInt("Z", a_Pos->z);
			a_Writer.EndCompound();
		}
	}
}





void BeeNBT::ReadFields(cBee & a_Bee, const cParsedNBT & a_NBT, int a_Tag)
{
	a_Bee.SetAge(ReadInt(a_NBT, a_Tag, "Age"));
	a_Bee.SetHasNectar(ReadFlag(a_NBT, a_Tag, "HasNectar"));
	a_Bee.SetHasStung(ReadFlag(a_NBT, a_Tag, "HasStung"));
	a_Bee.SetHivePos(ReadPosition(a_NBT, a_Tag, "HivePos"));
	a_Bee.SetFlowerPos(ReadPosition(a_NBT, a_Tag, "FlowerPos"));
	a_Bee.SetTicksSincePollination(ReadInt(a_NBT, a_Tag, "TicksSincePollination"));
	a_Bee.SetCannotEnterHiveTicks(ReadInt(a_NBT, a_Tag, "CannotEnterHiveTicks"));
	a_Bee.SetCropsGrownSincePollination(ReadInt(a_NBT, a_Tag, "CropsGrownSincePollination"));
	a_Bee.SetTicksSinceSting(ReadInt(a_NBT, a_Tag, "CuberiteTicksSinceSting"));
}





void BeeNBT::WriteFields(cFastNBTWriter & a_Writer, const cBee & a_Bee)
{
	a_Writer.AddInt("Age", a_Bee.GetAge());
	a_Writer.AddByte("HasNectar", a_Bee.HasNectar());
	a_Writer.AddByte("HasStung", a_Bee.HasStung());
	WritePosition(a_Writer, "HivePos", a_Bee.GetHivePos());
	WritePosition(a_Writer, "FlowerPos", a_Bee.GetFlowerPos());
	a_Writer.AddInt("TicksSincePollination", a_Bee.GetTicksSincePollination());
	a_Writer.AddInt("CannotEnterHiveTicks", a_Bee.GetCannotEnterHiveTicks());
	a_Writer.AddInt("CropsGrownSincePollination", a_Bee.GetCropsGrownSincePollination());
	if (a_Bee.HasStung())
	{
		a_Writer.AddInt("CuberiteTicksSinceSting", a_Bee.GetTicksSinceSting());
	}
}





void BeeNBT::ReadHive(cBeehiveEntity & a_Hive, const cParsedNBT & a_NBT, int a_Tag)
{
	const auto Bees = a_NBT.FindChildByName(a_Tag, "Bees");
	if (Bees < 0)
	{
		return;
	}
	if (a_NBT.GetType(Bees) != TAG_List)
	{
		throw std::runtime_error("Bee home has a malformed Bees list");
	}

	std::vector<cBeehiveEntity::Occupant> Occupants;
	for (auto Occupant = a_NBT.GetFirstChild(Bees); Occupant >= 0; Occupant = a_NBT.GetNextSibling(Occupant))
	{
		if (a_NBT.GetType(Occupant) != TAG_Compound)
		{
			throw std::runtime_error("Bee home has a malformed occupant");
		}
		const auto Entity = a_NBT.FindChildByName(Occupant, "EntityData");
		if ((Entity < 0) || (a_NBT.GetType(Entity) != TAG_Compound))
		{
			throw std::runtime_error("Bee home occupant has malformed EntityData");
		}
		cFastNBTWriter Writer;
		for (int Field = a_NBT.GetFirstChild(Entity); Field >= 0; Field = a_NBT.GetNextSibling(Field))
		{
			Writer.CopyTag(a_NBT, Field);
		}
		Writer.Finish();

		Occupants.push_back(
			{
				ContiguousByteBuffer(Writer.GetResult()),
				std::max(0, ReadInt(a_NBT, Occupant, "TicksInHive")),
				std::max(0, ReadInt(a_NBT, Occupant, "MinOccupationTicks", ReadFlag(a_NBT, Entity, "HasNectar") ? 2400 : 600))
			}
		);
	}
	a_Hive.SetOccupants(std::move(Occupants));
}





void BeeNBT::WriteHive(cFastNBTWriter & a_Writer, const cBeehiveEntity & a_Hive)
{
	a_Writer.BeginList("Bees", TAG_Compound);
	for (const auto & Occupant : a_Hive.GetOccupants())
	{
		const cParsedNBT Entity(Occupant.EntityData);
		ASSERT(Entity.IsValid() && (Entity.GetType(0) == TAG_Compound));
		a_Writer.BeginCompound("");
		a_Writer.BeginCompound("EntityData");
		for (int Field = Entity.GetFirstChild(0); Field >= 0; Field = Entity.GetNextSibling(Field))
		{
			a_Writer.CopyTag(Entity, Field);
		}
		a_Writer.EndCompound();
		a_Writer.AddInt("TicksInHive", Occupant.TicksInHive);
		a_Writer.AddInt("MinOccupationTicks", Occupant.MinOccupationTicks);
		a_Writer.EndCompound();
	}
	a_Writer.EndList();
}
