#pragma once

#include "ChunkDef.h"
#include "NamedBlockState.h"

#include <optional>





namespace NamedBlockPalette
{
	/** A legacy block and metadata for simulation and for clients without the named state.
	The original named state must still be retained for saving and for clients that know it. */
	std::pair<BLOCKTYPE, NIBBLETYPE> ToLegacy(const cNamedBlockState & a_State);

	/** A valid named state in the target Java release for terrain changed by the legacy engine.
	Throws if the runtime block has no representation in that release. */
	const cNamedBlockState & FromLegacy(int a_DataVersion, BLOCKTYPE a_Type, NIBBLETYPE a_Meta);

	/** The wire registry ID for an exact named state in the target Java protocol. */
	std::optional<UInt32> ToProtocol(UInt32 a_Protocol, const cNamedBlockState & a_State);
}
