
// WorldStorage.h

// Interfaces to the cWorldStorage class representing the chunk loading / saving thread
// Selects a single storage schema to read and write the world's on-disk format
// Also declares the base class for all storage schemas, cWSSchema





#pragma once

#include "../OSSupport/IsThread.h"
#include "../OSSupport/Queue.h"
#include "ChunkDef.h"
#include "WorldFormat.h"




// fwd:
class cWorld;





/** Interface that all the world storage schemas need to implement */
class cWSSchema abstract
{
public:
	cWSSchema(cWorld * a_World) : m_World(a_World) {}
	virtual ~cWSSchema() {}  // Force the descendants' destructors to be virtual

	virtual bool LoadChunk(const cChunkCoords & a_Chunk) = 0;
	virtual bool SaveChunk(const cChunkCoords & a_Chunk) = 0;

protected:

	cWorld * m_World;
} ;





/** The actual world storage class */
class cWorldStorage:
	public cIsThread
{
	using Super = cIsThread;

public:

	cWorldStorage();
	virtual ~cWorldStorage() override;

	/** Queues a chunk to be loaded, asynchronously. */
	void QueueLoadChunk(int a_ChunkX, int a_ChunkZ);

	/** Queues a chunk to be saved, asynchronously. */
	void QueueSaveChunk(int a_ChunkX, int a_ChunkZ);

	/** Initializes the selected storage schema, ready to be started. */
	void Initialize(cWorld & a_World, const cWorldStorageFormat & a_Format, const AString & a_StorageSchemaName, int a_StorageCompressionFactor);
	void Stop(void);  // Hide the cIsThread's Stop() method, we need to signal the event
	void WaitForFinish(void);
	void WaitForLoadQueueEmpty(void);
	void WaitForSaveQueueEmpty(void);

	size_t GetLoadQueueLength(void);
	size_t GetSaveQueueLength(void);

protected:

	cWorld * m_World;
	bool m_DiscardSaves = false;

	cQueue<cChunkCoords> m_LoadQueue;
	cQueue<cChunkCoords> m_SaveQueue;

	/** The schema chosen for this world, used for both loading and saving. */
	std::unique_ptr<cWSSchema> m_Schema;

	/** Set when there's any addition to the queues */
	cEvent m_Event;


	/** Loads the chunk specified; returns true on success, false on failure */
	bool LoadChunk(int a_ChunkX, int a_ChunkZ);

	virtual void Execute(void) override;

	/** Loads one chunk from the queue (if any queued); returns true if there was a chunk in the queue to load */
	bool LoadOneChunk(void);

	/** Saves one chunk from the queue (if any queued); returns true if there was a chunk in the queue to save */
	bool SaveOneChunk(void);
} ;




