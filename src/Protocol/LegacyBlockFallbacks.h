#pragma once

#include "Protocol.h"





class cFastNBTWriter;
class cParsedNBT;

namespace LegacyBlockFallbacks
{
	struct cState
	{
		BLOCKTYPE Type;
		NIBBLETYPE Meta;

		/** Legacy chunk and block-change wire encoding. */
		UInt16 Packed() const { return static_cast<UInt16>((Type << 4) | Meta); }
	};

	/** Returns the state to present to a client. Only packet data is translated. */
	cState ForBlock(cProtocol::Version a_Version, BLOCKTYPE a_Type, NIBBLETYPE a_Meta);

	/** Translates legacy block ID and metadata as used by falling blocks and break effects. */
	Int32 ForEffect(cProtocol::Version a_Version, Int32 a_Data);

	/** Returns a valid visible block item for a legacy client, if the canonical item is missing there. */
	std::pair<short, short> ForItem(cProtocol::Version a_Version, short a_Type, short a_Damage);

	/** Attaches the original block item's identity to its substitute for inventory round trips. */
	void WriteItemIdentity(cFastNBTWriter & a_Writer, const cItem & a_Item);

	/** Restores the original item only if it translates to the same wire item. */
	void RestoreItemIdentity(cProtocol::Version a_Version, cItem & a_Item, const cParsedNBT & a_Nbt);
}
