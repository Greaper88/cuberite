#include "Globals.h"
#include "WorldFormat.h"
#include "FastNBT.h"
#include "ChunkDef.h"
#include "PalettedAnvil.h"
#include "OSSupport/File.h"
#include "OSSupport/GZipFile.h"





namespace
{
	constexpr int AnvilSaveVersion = 19133;
	constexpr int McRegionSaveVersion = 19132;

	[[noreturn]] void RejectFormat(const AString & a_WorldPath, const AString & a_Reason)
	{
		throw std::runtime_error(fmt::format(FMT_STRING("Cannot open world '{}': {}."), a_WorldPath, a_Reason));
	}

	bool HasRegionFiles(const AString & a_RegionPath, const char * a_Extension)
	{
		if (!cFile::IsFolder(a_RegionPath))
		{
			return false;
		}
		for (const auto & Name : cFile::GetFolderContents(a_RegionPath))
		{
			if ((Name.size() > 6) && (Name.compare(0, 2, "r.") == 0) && (Name.compare(Name.size() - 4, 4, a_Extension) == 0))
			{
				return true;
			}
		}
		return false;
	}

	AString FindLevelMetadata(const AString & a_Path)
	{
		auto LevelDat = a_Path + cFile::PathSeparator() + "level.dat";
		if (cFile::Exists(LevelDat))
		{
			return LevelDat;
		}
		LevelDat += "_old";
		return cFile::Exists(LevelDat) ? LevelDat : AString{};
	}
}





cWorldStorageFormat SelectWorldStorageFormat(const AString & a_WorldPath)
{
	const auto Base = a_WorldPath + cFile::PathSeparator();
	if (cFile::Exists(Base + "db" + cFile::PathSeparator() + "CURRENT"))
	{
		RejectFormat(a_WorldPath, "Bedrock LevelDB worlds are unsupported");
	}
	if (HasRegionFiles(Base + "region", ".mcr"))
	{
		RejectFormat(a_WorldPath, "Java McRegion worlds are unsupported");
	}

	auto MetadataPath = FindLevelMetadata(a_WorldPath);
	if (MetadataPath.empty())
	{
		// Vanilla stores the Nether and End alongside the overworld's region directory.
		const auto End = a_WorldPath.find_last_not_of("/\\");
		const auto Separator = a_WorldPath.find_last_of("/\\", End);
		if ((End != AString::npos) && (Separator != AString::npos))
		{
			const auto Directory = a_WorldPath.substr(Separator + 1, End - Separator);
			if ((Directory == "DIM-1") || (Directory == "DIM1"))
			{
				MetadataPath = FindLevelMetadata(a_WorldPath.substr(0, Separator));
			}
		}
	}
	if (MetadataPath.empty())
	{
		if (HasRegionFiles(Base + "region", ".mca"))
		{
			// Cuberite used to allow legacy worlds consisting only of region files.
			return {eWorldStorageFormat::LegacyAnvil, 0, false, {}, {}};
		}
		return {eWorldStorageFormat::PalettedAnvil, CurrentJavaWorldDataVersion, false, {}, {}};
	}

	cWorldStorageFormat Format{eWorldStorageFormat::LegacyAnvil, 0, true, {}, {}};
	int SaveVersion = AnvilSaveVersion;
	try
	{
		const auto Metadata = GZipFile::ReadRestOfFile(MetadataPath);
		const cParsedNBT Nbt(Metadata.GetView());
		const int Data = Nbt.IsValid() ? Nbt.FindChildByName(0, "Data") : -1;
		if ((Data < 0) || (Nbt.GetType(Data) != TAG_Compound))
		{
			throw std::runtime_error("missing Java world metadata");
		}

		const int DataVersionTag = Nbt.FindChildByName(Data, "DataVersion");
		if (DataVersionTag >= 0)
		{
			if (Nbt.GetType(DataVersionTag) != TAG_Int)
			{
				throw std::runtime_error("invalid DataVersion tag");
			}
			Format.DataVersion = Nbt.GetInt(DataVersionTag);
		}

		const auto SpawnX = Nbt.FindChildByName(Data, "SpawnX");
		const auto SpawnY = Nbt.FindChildByName(Data, "SpawnY");
		const auto SpawnZ = Nbt.FindChildByName(Data, "SpawnZ");
		if (
			(SpawnX >= 0) && (Nbt.GetType(SpawnX) == TAG_Int) &&
			(SpawnY >= 0) && (Nbt.GetType(SpawnY) == TAG_Int) &&
			(SpawnZ >= 0) && (Nbt.GetType(SpawnZ) == TAG_Int)
		)
		{
			const auto Y = Nbt.GetInt(SpawnY);
			if ((Y >= 0) && (Y < cChunkDef::Height))
			{
				Format.Spawn = Vector3i(Nbt.GetInt(SpawnX), Y, Nbt.GetInt(SpawnZ));
			}
		}
		const auto Seed = Nbt.FindChildByName(Data, "RandomSeed");
		if ((Seed >= 0) && (Nbt.GetType(Seed) == TAG_Long))
		{
			Format.Seed = Nbt.GetLong(Seed);
		}

		const int SaveVersionTag = Nbt.FindChildByName(Data, "version");
		if (SaveVersionTag >= 0)
		{
			if (Nbt.GetType(SaveVersionTag) != TAG_Int)
			{
				throw std::runtime_error("invalid version tag");
			}
			SaveVersion = Nbt.GetInt(SaveVersionTag);
		}
	}
	catch (const std::exception & Error)
	{
		RejectFormat(a_WorldPath, fmt::format(FMT_STRING("cannot identify the format in {} ({})"), MetadataPath, Error.what()));
	}

	if (SaveVersion == McRegionSaveVersion)
	{
		RejectFormat(a_WorldPath, "Java McRegion worlds are unsupported");
	}
	if (SaveVersion != AnvilSaveVersion)
	{
		RejectFormat(a_WorldPath, fmt::format(FMT_STRING("Java world save version {} is unsupported"), SaveVersion));
	}
	if (PalettedAnvil::BiomeCount(Format.DataVersion) != 0)
	{
		Format.Format = eWorldStorageFormat::PalettedAnvil;
	}
	else if (Format.DataVersion > LegacyAnvilMaxDataVersion)
	{
		RejectFormat(a_WorldPath, fmt::format(FMT_STRING("Java world DataVersion {} is unsupported"), Format.DataVersion));
	}
	return Format;
}
