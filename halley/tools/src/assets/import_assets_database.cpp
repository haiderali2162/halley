#include "halley/tools/assets/import_assets_database.h"
#include "halley/file/byte_serializer.h"
#include "halley/resources/resource_data.h"
#include "halley/file/filesystem.h"

using namespace Halley;

void ImportAssetsDatabase::FileEntry::serialize(Serializer& s) const
{
	s << asset.inputFile;
	s << asset.srcDir;
	s << asset.fileTime;
	s << asset.metaTime;
	s << asset.outputFiles;
}

void ImportAssetsDatabase::FileEntry::deserialize(Deserializer& s)
{
	s >> asset.inputFile;
	s >> asset.srcDir;
	s >> asset.fileTime;
	s >> asset.metaTime;
	s >> asset.outputFiles;
}

ImportAssetsDatabase::ImportAssetsDatabase(Project& project, Path dbFile)
	: project(project)
	, dbFile(dbFile)
{
	load();
}

void ImportAssetsDatabase::load()
{
	std::lock_guard<std::mutex> lock(mutex);
	try {
		auto data = FileSystem::readFile(dbFile);
		auto s = Deserializer(data);
		deserialize(s);
	} catch (...) {
		// No database found, just ignore it
	}
}

void ImportAssetsDatabase::save() const
{
	std::lock_guard<std::mutex> lock(mutex);
	FileSystem::writeFile(dbFile, Serializer::toBytes(*this));
}

bool ImportAssetsDatabase::needsImporting(const ImportAssetsDatabaseEntry& asset) const
{
	std::lock_guard<std::mutex> lock(mutex);
	auto iter = filesImported.find(asset.inputFile);
	if (iter == filesImported.end()) {
		return true;
	} else {
		auto& oldAsset = iter->second.asset;
		return asset.srcDir != oldAsset.srcDir || asset.fileTime != oldAsset.fileTime || asset.metaTime != oldAsset.metaTime;
	}
}

void ImportAssetsDatabase::markAsImported(const ImportAssetsDatabaseEntry& asset)
{
	std::lock_guard<std::mutex> lock(mutex);
	FileEntry entry;
	entry.asset = asset;
	entry.present = true;
	filesImported[asset.inputFile] = entry;
}

void ImportAssetsDatabase::markDeleted(const ImportAssetsDatabaseEntry& asset)
{
	std::lock_guard<std::mutex> lock(mutex);
	filesImported.erase(asset.inputFile);
}

void ImportAssetsDatabase::markAllAsMissing()
{
	std::lock_guard<std::mutex> lock(mutex);
	for (auto& e: filesImported) {
		e.second.present = false;
	}
}

void ImportAssetsDatabase::markAsPresent(Path file)
{
	std::lock_guard<std::mutex> lock(mutex);
	auto iter = filesImported.find(file);
	if (iter != filesImported.end()) {
		iter->second.present = true;
	}
}

std::vector<ImportAssetsDatabaseEntry> ImportAssetsDatabase::getAllMissing() const
{
	std::lock_guard<std::mutex> lock(mutex);
	std::vector<ImportAssetsDatabaseEntry> result;
	for (auto& e : filesImported) {
		if (!e.second.present) {
			result.push_back(e.second.asset);
		}
	}
	return result;
}

void ImportAssetsDatabase::serialize(Serializer& s) const
{
	int version = 1;
	s << version;
	s << filesImported;
}

void ImportAssetsDatabase::deserialize(Deserializer& s)
{
	int version;
	s >> version;
	if (version == 1) {
		s >> filesImported;
	} else {
		save();
	}
}