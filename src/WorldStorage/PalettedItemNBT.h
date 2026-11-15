#pragma once





class cFastNBTWriter;
class cParsedNBT;
class cItem;





namespace PalettedItemNBT
{
	/** Reads a native Java item. Returns false if the engine cannot retain its data, leaving a_Item unchanged.
	An empty equipment slot, encoded as an empty compound, is accepted. */
	bool Read(cItem & a_Item, const cParsedNBT & a_NBT, int a_Tag, int a_DataVersion);

	/** Writes a native Java item, including an empty compound for an empty equipment slot.
	Throws if the stack cannot be represented in the target data version. */
	void Write(cFastNBTWriter & a_Writer, const cItem & a_Item, int a_Slot, const AString & a_Name, int a_DataVersion);
}
