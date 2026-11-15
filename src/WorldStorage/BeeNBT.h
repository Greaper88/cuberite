#pragma once

#include "../Vector3.h"

#include <memory>





class cBee;
class cBeehiveEntity;
class cFastNBTWriter;
class cParsedNBT;

namespace BeeNBT
{
	void ReadFields(cBee & a_Bee, const cParsedNBT & a_NBT, int a_Tag);
	void WriteFields(cFastNBTWriter & a_Writer, const cBee & a_Bee);

	void ReadHive(cBeehiveEntity & a_Hive, const cParsedNBT & a_NBT, int a_Tag);
	void WriteHive(cFastNBTWriter & a_Writer, const cBeehiveEntity & a_Hive);

	ContiguousByteBuffer SaveOccupant(cBee & a_Bee);
	std::unique_ptr<cBee> LoadOccupant(ContiguousByteBufferView a_Data, Vector3d a_Position);
}
