#pragma once





enum class eWorldStorageFormat
{
	LegacyAnvil,
};

/** Last Java release data version that uses numeric block IDs in Anvil chunks. */
constexpr int LegacyAnvilMaxDataVersion = 1343;

/** Selects a reader and writer from the world on disk, or the current format for a new world.
Throws before the world is opened if a recognized format cannot be written by this server. */
eWorldStorageFormat SelectWorldStorageFormat(const AString & a_WorldPath);
