
// WorldStorage.cpp

// Implements the cWorldStorage class representing the chunk loading / saving thread

#include "Globals.h"
#include "WorldStorage.h"
#include "WSSAnvil.h"
#include "WSSPalettedAnvil.h"
#include "../World.h"





////////////////////////////////////////////////////////////////////////////////
// cWorldStorage:

cWorldStorage::cWorldStorage(void) :
	Super("World Storage Executor"),
	m_World(nullptr)
{
}





cWorldStorage::~cWorldStorage() = default;





void cWorldStorage::Initialize(
	cWorld & a_World,
	const cWorldStorageFormat & a_Format,
	const AString & a_StorageSchemaName,
	int a_StorageCompressionFactor
)
{
	m_World = &a_World;
	m_DiscardSaves = (NoCaseCompare(a_StorageSchemaName, "forgetful") == 0);
	if (!m_DiscardSaves && (NoCaseCompare(a_StorageSchemaName, "default") != 0) && (NoCaseCompare(a_StorageSchemaName, "anvil") != 0))
	{
		LOGWARNING("Unknown storage schema '%s' for world '%s'. Using the detected format.", a_StorageSchemaName, a_World.GetName());
	}

	switch (a_Format.Format)
	{
		case eWorldStorageFormat::LegacyAnvil:
		{
			m_Schema = std::make_unique<cWSSAnvil>(&a_World, a_StorageCompressionFactor, a_Format);
			break;
		}
		case eWorldStorageFormat::PalettedAnvil:
		{
			m_Schema = std::make_unique<cWSSPalettedAnvil>(&a_World, a_StorageCompressionFactor, a_Format);
			break;
		}
	}
}





void cWorldStorage::Stop(void)
{
	WaitForFinish();
}





void cWorldStorage::WaitForFinish(void)
{
	LOGD("Waiting for the world storage to finish saving");

	{
		m_LoadQueue.Clear();
	}

	// Wait for the saving to finish:
	WaitForSaveQueueEmpty();

	// Wait for the thread to finish:
	m_ShouldTerminate = true;
	m_Event.Set();  // Wake up the thread if waiting
	Super::Stop();
	LOGD("World storage thread finished");
}





void cWorldStorage::WaitForLoadQueueEmpty(void)
{
	m_LoadQueue.BlockTillEmpty();
}





void cWorldStorage::WaitForSaveQueueEmpty(void)
{
	m_SaveQueue.BlockTillEmpty();
}





size_t cWorldStorage::GetLoadQueueLength(void)
{
	return m_LoadQueue.Size();
}





size_t cWorldStorage::GetSaveQueueLength(void)
{
	return m_SaveQueue.Size();
}





void cWorldStorage::QueueLoadChunk(int a_ChunkX, int a_ChunkZ)
{
	ASSERT((a_ChunkX > -0x08000000) && (a_ChunkX < 0x08000000));
	ASSERT((a_ChunkZ > -0x08000000) && (a_ChunkZ < 0x08000000));
	ASSERT(m_World->IsChunkQueued(a_ChunkX, a_ChunkZ));

	m_LoadQueue.EnqueueItem({ a_ChunkX, a_ChunkZ });
	m_Event.Set();
}





void cWorldStorage::QueueSaveChunk(int a_ChunkX, int a_ChunkZ)
{
	ASSERT(m_World->IsChunkValid(a_ChunkX, a_ChunkZ));

	m_SaveQueue.EnqueueItem({ a_ChunkX, a_ChunkZ });
	m_Event.Set();
}





void cWorldStorage::Execute(void)
{
	while (!m_ShouldTerminate)
	{
		m_Event.Wait();
		// Process both queues until they are empty again:
		bool Success;
		do
		{
			if (m_ShouldTerminate)
			{
				return;
			}

			Success = LoadOneChunk();
			Success |= SaveOneChunk();
		} while (Success);
	}
}





bool cWorldStorage::LoadOneChunk(void)
{
	// Dequeue an item, bail out if there's none left:
	cChunkCoords ToLoad(0, 0);
	bool ShouldLoad = m_LoadQueue.TryDequeueItem(ToLoad);
	if (!ShouldLoad)
	{
		return false;
	}

	// Load the chunk:
	LoadChunk(ToLoad.m_ChunkX, ToLoad.m_ChunkZ);

	return true;
}





bool cWorldStorage::SaveOneChunk(void)
{
	// Dequeue one chunk to save:
	cChunkCoords ToSave(0, 0);
	bool ShouldSave = m_SaveQueue.TryDequeueItem(ToSave);
	if (!ShouldSave)
	{
		return false;
	}

	// Save the chunk, if it's valid:
	if (m_World->IsChunkValid(ToSave.m_ChunkX, ToSave.m_ChunkZ))
	{
		m_World->MarkChunkSaving(ToSave.m_ChunkX, ToSave.m_ChunkZ);
		if (m_DiscardSaves || m_Schema->SaveChunk(ToSave))
		{
			m_World->MarkChunkSaved(ToSave.m_ChunkX, ToSave.m_ChunkZ);
		}
	}

	return true;
}





bool cWorldStorage::LoadChunk(int a_ChunkX, int a_ChunkZ)
{
	ASSERT(m_World->IsChunkQueued(a_ChunkX, a_ChunkZ));

	if (m_Schema->LoadChunk({ a_ChunkX, a_ChunkZ }))
	{
		return true;
	}

	// Notify the chunk owner that the chunk failed to load (sets cChunk::m_HasLoadFailed to true):
	m_World->ChunkLoadFailed(a_ChunkX, a_ChunkZ);

	return false;
}





