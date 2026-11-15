#pragma once

#include "Vector3.h"

#include <optional>





enum class eWorldStorageFormat
{
	LegacyAnvil,
	PalettedAnvil,
};

struct cWorldStorageFormat
{
	eWorldStorageFormat Format;
	int DataVersion = 0;
	bool HasLevelMetadata = false;
	std::optional<Vector3i> Spawn;
	std::optional<Int64> Seed;
};

/** Java 1.15.2, the default Anvil data version for new worlds. */
constexpr int CurrentJavaWorldDataVersion = 2230;

/** Java 1.15 was the first release to store bees and their homes. */
constexpr int JavaBeeDataVersion = 2225;

/** Last Java release data version that uses numeric block IDs in Anvil chunks. */
constexpr int LegacyAnvilMaxDataVersion = 1343;

/** Selects a reader and writer from the world on disk, or the current format for a new world.
Throws before the world is opened if a recognized format cannot be written by this server. */
cWorldStorageFormat SelectWorldStorageFormat(const AString & a_WorldPath);
