#include "SystemKeychainBackend.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <system_error>

#if defined(__linux__)
#include <libsecret/secret.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#elif defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include "AtomicFile.h"
#include <Windows.h>
#include <dpapi.h>
#include <shlobj.h>
#include <windows.h>
#endif

namespace localvault {

#if defined(__linux__)

namespace {

	std::string base64Encode(Blob const &data)
	{
		static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string out;
		out.reserve(((data.size() + 2) / 3) * 4);
		for (size_t i = 0; i < data.size(); i += 3) {
			uint32_t triple = 0;
			int len = 0;
			for (size_t j = 0; j < 3 && i + j < data.size(); ++j) {
				triple |= static_cast<uint8_t>(data[i + j]) << (16 - j * 8);
				++len;
			}
			out.push_back(alphabet[(triple >> 18) & 0x3f]);
			out.push_back(alphabet[(triple >> 12) & 0x3f]);
			out.push_back(len > 1 ? alphabet[(triple >> 6) & 0x3f] : '=');
			out.push_back(len > 2 ? alphabet[triple & 0x3f] : '=');
		}
		return out;
	}

	Blob base64Decode(std::string const &in)
	{
		static const unsigned char decodeTable[256] = {
			 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
			 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
			 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f,
			 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
			 0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
			 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,
			 0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
			 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff,
		};

		Blob out;
		out.reserve((in.size() / 4) * 3);
		uint32_t triple = 0;
		int bits = 0;
		int pad = 0;
		for (char c : in) {
			if (std::isspace(static_cast<unsigned char>(c))) continue;
			if (c == '=') {
				++pad;
				continue;
			}
			unsigned char val = (c >= 0 && c < 0x80) ? decodeTable[c] : 0xff;
			if (val == 0xff) return { };
			triple = (triple << 6) | val;
			bits += 6;
			if (bits >= 8) {
				bits -= 8;
				out.push_back(static_cast<char>((triple >> bits) & 0xff));
			}
		}
		while (pad-- > 0 && !out.empty()) {
			// パディングは無視
		}
		return out;
	}

} // namespace

class SystemKeychainBackend::Impl {
public:
	Impl()
	{
		schemaName_ = "com.example.localvault.Vault";
		schema_.name = schemaName_.c_str();
		schema_.flags = SECRET_SCHEMA_NONE;
		schema_.attributes[0].name = "key";
		schema_.attributes[0].type = SECRET_SCHEMA_ATTRIBUTE_STRING;
		schema_.attributes[1].name = nullptr;
		schema_.attributes[1].type = SecretSchemaAttributeType(0);
	}

	void setSchemaName(std::string const &schemaName)
	{
		schemaName_ = schemaName;
		schema_.name = schemaName_.c_str();
	}

	void setServiceName(std::string const &) {}

	bool isAvailable() const
	{
		// Secret Service に接続してコレクション一覧を取得するだけで、項目の読み出しは行わない。
		// ダミー項目の lookup と違い、キーリングがロックされていても解除ダイアログは出ない
		GError *error = nullptr;
		SecretService *service = secret_service_get_sync(SECRET_SERVICE_LOAD_COLLECTIONS, nullptr, &error);
		if (service) {
			g_object_unref(service);
		}
		if (error) {
			g_error_free(error);
			return false;
		}
		return service != nullptr;
	}

	StorageStatus store(std::string const &key, Blob const &data)
	{
		// 同じ属性の項目が既にあれば libsecret が置き換える。
		// 事前に remove() すると、保存失敗時に既存の EMK を失うため行わない
		std::string base64 = base64Encode(data);
		std::string label = "localvault vault: " + key;

		GError *error = nullptr;
		const gboolean ok = secret_password_store_sync(
			&schema_,
			SECRET_COLLECTION_DEFAULT,
			label.c_str(),
			base64.c_str(),
			nullptr,
			&error,
			"key", key.c_str(),
			nullptr);

		if (error) {
			g_error_free(error);
			return StorageStatus::Unavailable;
		}
		return ok ? StorageStatus::Ok : StorageStatus::Error;
	}

	StorageStatus load(std::string const &key, Blob *out)
	{
		if (!out) return StorageStatus::Error;
		out->clear();
		// secret_password_lookup_sync は、ロック中の項目の解除ダイアログをユーザーが閉じると
		// エラーなしで NULL を返し「項目なし」と区別できない。それを NotFound と誤認すると
		// 新規セットアップへ進み既存 EMK を上書きしかねないため、SECRET_SEARCH_ALL で
		// ロック中の項目も列挙し、存在するが読めない場合は Unavailable を返す。
		GError *error = nullptr;
		GList *items = secret_password_search_sync(
			&schema_,
			static_cast<SecretSearchFlags>(SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS),
			nullptr,
			&error,
			"key", key.c_str(),
			nullptr);

		if (error) {
			g_list_free_full(items, g_object_unref);
			g_error_free(error);
			return StorageStatus::Unavailable;
		}
		if (!items) {
			return StorageStatus::NotFound;
		}

		StorageStatus status = StorageStatus::Ok;
		for (GList *l = items; l; l = l->next) {
			if (SECRET_IS_ITEM(l->data) && secret_item_get_locked(SECRET_ITEM(l->data))) {
				status = StorageStatus::Unavailable;
			}
		}
		if (status == StorageStatus::Ok) {
			SecretValue *value = secret_retrievable_retrieve_secret_sync(SECRET_RETRIEVABLE(items->data), nullptr, &error);
			if (error) {
				g_error_free(error);
				status = StorageStatus::Unavailable;
			} else if (!value) {
				status = StorageStatus::Unavailable;
			} else {
				gsize length = 0;
				const gchar *text = secret_value_get(value, &length);
				*out = base64Decode(std::string(text, length));
				if (length > 0 && out->empty()) {
					status = StorageStatus::Error;
				}
				secret_value_unref(value);
			}
		}
		g_list_free_full(items, g_object_unref);
		return status;
	}

	StorageStatus remove(std::string const &key)
	{
		GError *error = nullptr;
		const gboolean removed = secret_password_clear_sync(
			&schema_,
			nullptr,
			&error,
			"key", key.c_str(),
			nullptr);

		if (error) {
			g_error_free(error);
			return StorageStatus::Unavailable;
		}
		return removed ? StorageStatus::Ok : StorageStatus::NotFound;
	}

private:
	SecretSchema schema_;
	std::string schemaName_;
};

#elif defined(__APPLE__)

class SystemKeychainBackend::Impl {
public:
	bool isAvailable() const
	{
		return true;
	}

	void setSchemaName(std::string const &) {}

	void setServiceName(std::string const &serviceName)
	{
		serviceName_ = serviceName;
	}

	StorageStatus store(std::string const &key, Blob const &data)
	{
		// 既存項目は SecItemUpdate で置き換える。
		// 事前に削除すると、追加失敗時に既存の EMK を失うため行わない
		CFStringRef service = CFStringCreateWithCString(nullptr, serviceName_.c_str(), kCFStringEncodingUTF8);
		CFStringRef account = CFStringCreateWithCString(nullptr, key.c_str(), kCFStringEncodingUTF8);
		CFDataRef value = CFDataCreate(nullptr, reinterpret_cast<const UInt8 *>(data.data()), static_cast<CFIndex>(data.size()));

		void const *queryKeys[] = { kSecClass, kSecAttrService, kSecAttrAccount };
		void const *queryValues[] = { kSecClassGenericPassword, service, account };
		CFDictionaryRef query = CFDictionaryCreate(nullptr, queryKeys, queryValues, 3, nullptr, nullptr);

		void const *updateKeys[] = { kSecValueData };
		void const *updateValues[] = { value };
		CFDictionaryRef update = CFDictionaryCreate(nullptr, updateKeys, updateValues, 1, nullptr, nullptr);

		OSStatus status = SecItemUpdate(query, update);
		if (status == errSecItemNotFound) {
			void const *addKeys[] = { kSecClass, kSecAttrService, kSecAttrAccount, kSecValueData, kSecAttrAccessible };
			void const *addValues[] = { kSecClassGenericPassword, service, account, value, kSecAttrAccessibleWhenUnlockedThisDeviceOnly };
			CFDictionaryRef add = CFDictionaryCreate(nullptr, addKeys, addValues, 5, nullptr, nullptr);
			status = SecItemAdd(add, nullptr);
			CFRelease(add);
		}

		CFRelease(update);
		CFRelease(query);
		CFRelease(value);
		CFRelease(account);
		CFRelease(service);

		return toStorageStatus(status);
	}

	StorageStatus load(std::string const &key, Blob *out)
	{
		if (!out) return StorageStatus::Error;
		out->clear();
		CFStringRef service = CFStringCreateWithCString(nullptr, serviceName_.c_str(), kCFStringEncodingUTF8);
		CFStringRef account = CFStringCreateWithCString(nullptr, key.c_str(), kCFStringEncodingUTF8);

		void const *keys[] = { kSecClass, kSecAttrService, kSecAttrAccount, kSecReturnData, kSecMatchLimit };
		void const *values[] = { kSecClassGenericPassword, service, account, kCFBooleanTrue, kSecMatchLimitOne };
		CFDictionaryRef query = CFDictionaryCreate(nullptr, keys, values, 5, nullptr, nullptr);

		CFDataRef result = nullptr;
		OSStatus status = SecItemCopyMatching(query, reinterpret_cast<CFTypeRef *>(&result));

		if (status == errSecSuccess && result) {
			out->resize(static_cast<size_t>(CFDataGetLength(result)));
			std::memcpy(out->data(), CFDataGetBytePtr(result), out->size());
		} else if (status == errSecSuccess) {
			status = errSecDecode;
		}
		if (result) {
			CFRelease(result);
		}

		CFRelease(query);
		CFRelease(account);
		CFRelease(service);

		return toStorageStatus(status);
	}

	StorageStatus remove(std::string const &key)
	{
		CFStringRef service = CFStringCreateWithCString(nullptr, serviceName_.c_str(), kCFStringEncodingUTF8);
		CFStringRef account = CFStringCreateWithCString(nullptr, key.c_str(), kCFStringEncodingUTF8);

		void const *keys[] = { kSecClass, kSecAttrService, kSecAttrAccount };
		void const *values[] = { kSecClassGenericPassword, service, account };
		CFDictionaryRef query = CFDictionaryCreate(nullptr, keys, values, 3, nullptr, nullptr);

		OSStatus status = SecItemDelete(query);

		CFRelease(query);
		CFRelease(account);
		CFRelease(service);

		return toStorageStatus(status);
	}

private:
	std::string serviceName_ = "localvault-vault";

	static StorageStatus toStorageStatus(OSStatus status)
	{
		switch (status) {
		case errSecSuccess:
			return StorageStatus::Ok;
		case errSecItemNotFound:
			return StorageStatus::NotFound;
		case errSecInteractionNotAllowed:
		case errSecNotAvailable:
		case errSecAuthFailed:
		case errSecUserCanceled:
			return StorageStatus::Unavailable;
		default:
			return StorageStatus::Error;
		}
	}
};

#elif defined(_WIN32)

namespace {

	std::filesystem::path windowsAppDataPath()
	{
		wchar_t *path = nullptr;
		if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path)) && path) {
			std::filesystem::path result(path);
			CoTaskMemFree(path);
			return result;
		}
		const char *appdata = std::getenv("APPDATA");
		if (appdata) {
			return std::filesystem::path(appdata);
		}
		return std::filesystem::path(std::getenv("USERPROFILE") ? std::getenv("USERPROFILE") : ".");
	}

	std::string sanitizeKey(std::string const &key)
	{
		std::string safe = key;
		for (char &c : safe) {
			if (c == '/' || c == '\\' || c == ':') {
				c = '_';
			}
		}
		return safe;
	}

} // namespace

class SystemKeychainBackend::Impl {
public:
	Impl()
	{
		directory_ = windowsAppDataPath() / "localvault" / "emk";
	}

	bool isAvailable() const
	{
		return true;
	}

	void setSchemaName(std::string const &) {}

	void setServiceName(std::string const &) {}

	StorageStatus store(std::string const &key, Blob const &data)
	{
		DATA_BLOB in;
		in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(data.data()));
		in.cbData = static_cast<DWORD>(data.size());

		DATA_BLOB out;
		if (!CryptProtectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
			return StorageStatus::Error;
		}

		Blob encrypted(reinterpret_cast<char *>(out.pbData), reinterpret_cast<char *>(out.pbData) + out.cbData);
		LocalFree(out.pbData);

		std::error_code ec;
		std::filesystem::create_directories(directory_, ec);
		if (ec) {
			return StorageStatus::Error;
		}
		return AtomicFile::write(directory_ / sanitizeKey(key), encrypted);
	}

	StorageStatus load(std::string const &key, Blob *result)
	{
		if (!result) return StorageStatus::Error;
		result->clear();
		Blob encrypted;
		const StorageStatus status = AtomicFile::read(directory_ / sanitizeKey(key), &encrypted);
		if (status != StorageStatus::Ok) {
			return status;
		}

		DATA_BLOB in;
		in.pbData = reinterpret_cast<BYTE *>(encrypted.data());
		in.cbData = static_cast<DWORD>(encrypted.size());

		DATA_BLOB out;
		if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
			return StorageStatus::Error;
		}

		result->assign(reinterpret_cast<char *>(out.pbData), reinterpret_cast<char *>(out.pbData) + out.cbData);
		SecureZeroMemory(out.pbData, out.cbData);
		LocalFree(out.pbData);
		return StorageStatus::Ok;
	}

	StorageStatus remove(std::string const &key)
	{
		return AtomicFile::remove(directory_ / sanitizeKey(key));
	}

private:
	std::filesystem::path directory_;
};

#else
class SystemKeychainBackend::Impl {
public:
	bool isAvailable() const
	{
		return false;
	}

	void setSchemaName(std::string const &) {}

	void setServiceName(std::string const &) {}

	StorageStatus store(std::string const &, Blob const &)
	{
		return StorageStatus::Unavailable;
	}

	StorageStatus load(std::string const &, Blob *out)
	{
		if (!out) return StorageStatus::Error;
		out->clear();
		return StorageStatus::Unavailable;
	}

	StorageStatus remove(std::string const &)
	{
		return StorageStatus::Unavailable;
	}
};
#endif

SystemKeychainBackend::SystemKeychainBackend()
	: impl_(new Impl())
{
}

SystemKeychainBackend::~SystemKeychainBackend()
{
	delete impl_;
}

bool SystemKeychainBackend::isAvailable() const
{
	return impl_->isAvailable();
}

std::string SystemKeychainBackend::name() const
{
#if defined(__linux__)
	return "system-keychain-libsecret";
#elif defined(__APPLE__)
	return "system-keychain-macos";
#elif defined(_WIN32)
	return "system-keychain-dpapi";
#else
	return "system-keychain-unavailable";
#endif
}

StorageStatus SystemKeychainBackend::store(std::string const &key, Blob const &data)
{
	return impl_->store(key, data);
}

StorageStatus SystemKeychainBackend::load(std::string const &key, Blob *out)
{
	return impl_->load(key, out);
}

StorageStatus SystemKeychainBackend::remove(std::string const &key)
{
	return impl_->remove(key);
}

void SystemKeychainBackend::setSchemaName(std::string const &schemaName)
{
	impl_->setSchemaName(schemaName);
}

void SystemKeychainBackend::setServiceName(std::string const &serviceName)
{
	impl_->setServiceName(serviceName);
}

} // namespace localvault
