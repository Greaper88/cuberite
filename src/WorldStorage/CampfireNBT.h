#pragma once





class cCampfireEntity;
class cFastNBTWriter;
class cParsedNBT;

namespace CampfireNBT
{
	/** Reads the native cooking inventory and timers. Leaves the entity untouched on failure. */
	bool Read(cCampfireEntity & a_Campfire, const cParsedNBT & a_NBT, int a_Tag, int a_DataVersion);

	/** Writes the inventory used in both saves and block entity update packets. */
	void WriteItems(cFastNBTWriter & a_Writer, const cCampfireEntity & a_Campfire, int a_DataVersion);

	/** Writes the cooking inventory and timers used in saves. */
	void Write(cFastNBTWriter & a_Writer, const cCampfireEntity & a_Campfire, int a_DataVersion);
}
