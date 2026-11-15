#include "Globals.h"
#include "WorldFormat.h"
#include "FastNBT.h"
#include "PalettedAnvil.h"
#include "OSSupport/File.h"
#include "OSSupport/GZipFile.h"





namespace
{
	constexpr auto CurrentWorldFormat = eWorldStorageFormat::LegacyAnvil;
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
}





eWorldStorageFormat SelectWorldStorageFormat(const AString & a_WorldPath)
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

	auto MetadataPath = Base + "level.dat";
	if (!cFile::Exists(MetadataPath))
	{
		MetadataPath = Base + "level.dat_old";
		if (!cFile::Exists(MetadataPath))
		{
			// Cuberite worlds can also contain just region files, without a level.dat.
			return HasRegionFiles(Base + "region", ".mca") ? eWorldStorageFormat::LegacyAnvil : CurrentWorldFormat;
		}
	}

	int DataVersion = 0;
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
			DataVersion = Nbt.GetInt(DataVersionTag);
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

	if (PalettedAnvil::BiomeCount(DataVersion) != 0)
	{
		RejectFormat(a_WorldPath, fmt::format(FMT_STRING("Java DataVersion {} requires paletted block states in the live world engine"), DataVersion));
	}
	if (DataVersion > LegacyAnvilMaxDataVersion)
	{
		RejectFormat(a_WorldPath, fmt::format(FMT_STRING("Java world DataVersion {} is unsupported"), DataVersion));
	}
	if (SaveVersion == McRegionSaveVersion)
	{
		RejectFormat(a_WorldPath, "Java McRegion worlds are unsupported");
	}
	if (SaveVersion != AnvilSaveVersion)
	{
		RejectFormat(a_WorldPath, fmt::format(FMT_STRING("Java world save version {} is unsupported"), SaveVersion));
	}
	return eWorldStorageFormat::LegacyAnvil;
}
