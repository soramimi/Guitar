#include "easycrypto.h"
#include "../common/ChaCha20.h"
#include "../common/crc32.h"
#include <cstdio>
#include <cstring>
#include <cstring>
#include <sodium.h>
#include <string>

std::vector<char> easycrypto::encrypt(std::string const &key, std::string_view plain)
{
	std::vector<char> cipher;
	if (plain.empty()) {
		return cipher;
	}
	
	unsigned char k[crypto_secretbox_KEYBYTES];
	crypto_generichash(k, sizeof(k),
					   reinterpret_cast<unsigned char const *>(key.data()), key.size(),
					   nullptr, 0);
	
	unsigned char nonce[crypto_secretbox_NONCEBYTES];
	randombytes_buf(nonce, sizeof(nonce));
	
	cipher.resize(crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES + plain.size());
	std::memcpy(cipher.data(), nonce, crypto_secretbox_NONCEBYTES);
	
	if (crypto_secretbox_easy(
			reinterpret_cast<unsigned char *>(cipher.data()) + crypto_secretbox_NONCEBYTES,
			reinterpret_cast<unsigned char const *>(plain.data()), plain.size(),
			nonce, k) != 0) {
		cipher.clear();
	}
	
	sodium_memzero(k, sizeof(k));
	return cipher;
}

std::vector<char> easycrypto::decrypt(std::string const &key, std::string_view encrypted)
{
	std::vector<char> plain;
	if (encrypted.size() < crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES) {
		return plain;
	}
	
	unsigned char k[crypto_secretbox_KEYBYTES];
	crypto_generichash(k, sizeof(k),
					   reinterpret_cast<unsigned char const *>(key.data()), key.size(),
					   nullptr, 0);
	
	unsigned char const *nonce = reinterpret_cast<unsigned char const *>(encrypted.data());
	unsigned char const *cipher = reinterpret_cast<unsigned char const *>(encrypted.data()) + crypto_secretbox_NONCEBYTES;
	size_t cipher_len = encrypted.size() - crypto_secretbox_NONCEBYTES;
	
	plain.resize(cipher_len - crypto_secretbox_MACBYTES);
	if (crypto_secretbox_open_easy(
			reinterpret_cast<unsigned char *>(plain.data()),
			cipher, cipher_len,
			nonce, k) != 0) {
		plain.clear();
	}
	
	sodium_memzero(k, sizeof(k));
	return plain;
}
