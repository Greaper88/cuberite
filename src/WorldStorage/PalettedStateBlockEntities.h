#pragma once

#include "../BlockEntities/BlockEntity.h"
#include "../NamedBlockState.h"

#include <optional>





namespace PalettedStateBlockEntities
{
	/** Creates the legacy engine's object for a state that has no Java block entity record. */
	OwnedBlockEntity FromState(const cNamedBlockState & a_State, BLOCKTYPE a_Block, NIBBLETYPE a_Meta, Vector3i a_Pos, cWorld * a_World);

	/** Loads fields that Java stores in a block state alongside the block entity record. */
	void ApplyState(cBlockEntity & a_Entity, const cNamedBlockState & a_State);

	/** Encodes live fields that belong in the Java block state instead of the block entity record.
	Uses the retained state for properties that the engine does not simulate. */
	std::optional<cNamedBlockState> ToState(cBlockEntity & a_Entity, const cNamedBlockState * a_Original, int a_DataVersion);
}
