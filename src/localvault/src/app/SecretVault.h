
#ifndef SECRETVAULT_H
#define SECRETVAULT_H

#include "../vault/Vault.h"

#include "BackendSelector.h"

namespace localvault {

struct VaultWithBackend {
	BackendSelection backend;
	std::unique_ptr<Vault> vault;
	VaultWithBackend() = default;
	VaultWithBackend(BackendSelection &&b, std::unique_ptr<Vault> &&v)
		: backend(std::move(b))
		, vault(std::move(v))
	{
		vault->backend_ = backend.backend.get();
	}
	operator bool () const
	{
		return (bool)vault;
	}
	void reset()
	{
		vault.reset();
		backend = {};
	}
};


class SecretVault {
public:
	SecretVault();
};

} // namespace localvault

#endif // SECRETVAULT_H
