#pragma once

#include "WSSAnvil.h"





/** Uses the shared Anvil region container with Java 1.13 through 1.15.2 chunk NBT. */
class cWSSPalettedAnvil final :
	public cWSSAnvil
{
public:
	cWSSPalettedAnvil(cWorld * a_World, int a_CompressionFactor, const cWorldStorageFormat & a_Format);

private:
	bool LoadChunkFromData(const cChunkCoords & a_Chunk, ContiguousByteBufferView a_Data) override;
	std::optional<Compression::Result> SaveChunkToData(const cChunkCoords & a_Chunk) override;

	const int m_DataVersion;
};
