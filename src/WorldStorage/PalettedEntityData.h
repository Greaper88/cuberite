#pragma once





/** Stores a loaded record alongside the fields represented by the engine at load time. */
struct cPalettedEntityData
{
	ContiguousByteBuffer Original;
	ContiguousByteBuffer Baseline;
	int OriginalVersion = 0;

	// These are assigned once to live entities so their Java identity survives a move or save.
	Int64 UUIDMost = 0;
	Int64 UUIDLeast = 0;
};





/** Holds valid Java records for blocks and entities outside the engine's current capabilities. */
struct cUnloadedEntityRecords
{
	std::vector<ContiguousByteBuffer> Entities;
	std::vector<ContiguousByteBuffer> BlockEntities;
};
