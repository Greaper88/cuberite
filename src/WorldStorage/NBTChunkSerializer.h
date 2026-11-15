// NBTChunkSerializer.h

#pragma once

#include "ChunkDef.h"



// fwd:
class cBlockEntity;
class cEntity;
class cFastNBTWriter;
class cWorld;





/** Saves the chunk data into a NBT format, used by the Anvil storage.
The Writer is expected to be set up so that the serializer can write the chunk's top level "Level" NBT tag immediately.
Provides a single static entry point that does all the work, through a hidden worker class in the CPP file. */
class NBTChunkSerializer
{
public:

	/** Serializes the chunk into the specified writer. The chunk must be present. */
	static void Serialize(const cWorld & aWorld, cChunkCoords aCoords, cFastNBTWriter & aWriter);

	/** Encodes one native Java entity compound. Empty output means the entity has no representation in the target version. */
	static ContiguousByteBuffer SerializeEntity(cEntity & a_Entity, int a_DataVersion);

	/** Encodes one native Java block entity compound. Empty output means the block stores its state outside block-entity NBT. */
	static ContiguousByteBuffer SerializeBlockEntity(cBlockEntity & a_Entity, int a_DataVersion);
};
