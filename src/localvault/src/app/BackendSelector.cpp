#include "BackendSelector.h"

#include "../storage/FileBackend.h"
#include "../storage/SystemKeychainBackend.h"
#include "../vault/Vault.h"

#include <utility>

namespace localvault {

namespace {

	constexpr char MARKER_KEY[] = "storage-backend";

} // namespace

BackendSelector::BackendSelector(std::filesystem::path const &configDir, std::string const &schema, std::string const &emkKey, Factory const &systemFactory)
	: configDir_(configDir)
	, schema_(schema)
	, emkKey_(emkKey)
	, systemFactory_(systemFactory)
{
	if (!systemFactory_) {
		systemFactory_ = [this] {
			auto ret = std::make_unique<SystemKeychainBackend>();
#if defined(__linux__)
			ret->setSchemaName(schema_);
#elif defined(__APPLE__)
			ret->setServiceName(schema_);
#elif defined(_WIN32)
			// nop
#else
			// cannot use secure storage
#endif
			return ret;
		};
	}
}

const char *BackendSelector::name(BackendKind kind)
{
	return kind == BackendKind::File ? "file" : "system";
}

std::unique_ptr<ISecretStorageBackend> BackendSelector::create(BackendKind kind) const
{
	if (kind == BackendKind::File) {
		return std::make_unique<FileBackend>(configDir_ / "emk");
	}
	return systemFactory_();
}

bool BackendSelector::hasVault(ISecretStorageBackend *backend) const
{
	if (!backend) return false;
	return Vault(std::move(backend), emkKey_).state() == VaultState::Locked;
}

StorageStatus BackendSelector::record(BackendKind kind) const
{
	const std::string value = std::string(name(kind)) + "\n";
	return FileBackend(configDir_).store(MARKER_KEY, Blob(value.begin(), value.end()));
}

StorageStatus BackendSelector::forget() const
{
	return FileBackend(configDir_).remove(MARKER_KEY);
}

BackendSelection BackendSelector::select(bool force_file_backend) const
{
	BackendSelection result;
	
	if (force_file_backend) {
		auto file = create(BackendKind::File);
		result.kind = BackendKind::File;
		result.backend = std::move(file);
	} else {
		Blob marker;
		const StorageStatus markerStatus = FileBackend(configDir_).load(MARKER_KEY, &marker);
		if (markerStatus == StorageStatus::Ok) {
			std::string value(marker.begin(), marker.end());
			while (!value.empty() && (value.back() == '\n' || value.back() == '\r'))
				value.pop_back();
			if (value == name(BackendKind::System)) {
				result.kind = BackendKind::System;
			} else if (value == name(BackendKind::File)) {
				result.kind = BackendKind::File;
			} else {
				return result; // ConfigError
			}
			result.recorded = true;
			auto backend = create(result.kind);
			if (!backend->isAvailable()) {
				result.status = BackendSelection::Status::RecordedBackendUnavailable;
				return result;
			}
			result.backend = std::move(backend);
			result.status = BackendSelection::Status::Ok;
			return result;
		}
		if (markerStatus != StorageStatus::NotFound) {
			return result; // ConfigError
		}
		
		// 記録なし: 既存 Vault を探す（旧バージョンからの移行を含む）
		auto system = create(BackendKind::System);
		const bool systemAvailable = system->isAvailable();
		if (systemAvailable && hasVault(system.get())) {
			result.kind = BackendKind::System;
			result.backend = std::move(system);
		} else {
			auto file = create(BackendKind::File);
			if (hasVault(file.get())) {
				result.kind = BackendKind::File;
				result.backend = std::move(file);
			} else if (systemAvailable) {
				result.kind = BackendKind::System;
				result.backend = std::move(system);
				result.status = BackendSelection::Status::Ok;
				return result;
			} else {
				result.kind = BackendKind::File;
				result.backend = std::move(file);
				result.status = BackendSelection::Status::NeedsFileConsent;
				return result;
			}
		}
	}
	
	// 既存 Vault を見つけた保存先を記録する。失敗しても次回同じ探索で見つかる
	result.recorded = record(result.kind) == StorageStatus::Ok;
	result.status = BackendSelection::Status::Ok;
	return result;
}

} // namespace localvault
