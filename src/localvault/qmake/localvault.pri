
INCLUDEPATH += $$LOCALVAULT_SRC/
INCLUDEPATH += $$LOCALVAULT_SRC/gui/

HEADERS += \
	$$LOCALVAULT_SRC/app/BackendSelector.h \
	$$LOCALVAULT_SRC/app/SecretVault.h \
	$$LOCALVAULT_SRC/gui/SecurePinEdit.h \
	$$LOCALVAULT_SRC/vault/ProcessHardening.h \
	$$LOCALVAULT_SRC/vault/SecretStorage.h \
	$$LOCALVAULT_SRC/vault/SecureBuffer.h \
	$$LOCALVAULT_SRC/vault/Vault.h \
	$$LOCALVAULT_SRC/storage/AtomicFile.h \
	$$LOCALVAULT_SRC/storage/FileBackend.h \
	$$LOCALVAULT_SRC/storage/SystemKeychainBackend.h \
	$$LOCALVAULT_SRC/gui/UnlockVaultDialog.h \
	$$LOCALVAULT_SRC/gui/ResetVaultDialog.h \
	$$LOCALVAULT_SRC/gui/ChangePinDialog.h \
	$$LOCALVAULT_SRC/gui/SetupVaultDialog.h

SOURCES += \
	$$LOCALVAULT_SRC/app/BackendSelector.cpp \
	$$LOCALVAULT_SRC/app/SecretVault.cpp \
	$$LOCALVAULT_SRC/gui/SecurePinEdit.cpp \
	$$LOCALVAULT_SRC/vault/ProcessHardening.cpp \
	$$LOCALVAULT_SRC/vault/SecureBuffer.cpp \
	$$LOCALVAULT_SRC/vault/Vault.cpp \
	$$LOCALVAULT_SRC/storage/AtomicFile.cpp \
	$$LOCALVAULT_SRC/storage/FileBackend.cpp \
	$$LOCALVAULT_SRC/storage/SystemKeychainBackend.cpp \
	$$LOCALVAULT_SRC/gui/UnlockVaultDialog.cpp \
	$$LOCALVAULT_SRC/gui/ResetVaultDialog.cpp \
	$$LOCALVAULT_SRC/gui/ChangePinDialog.cpp \
	$$LOCALVAULT_SRC/gui/SetupVaultDialog.cpp

FORMS += \
    $$LOCALVAULT_SRC/gui/UnlockVaultDialog.ui \
	$$LOCALVAULT_SRC/gui/ResetVaultDialog.ui \
	$$LOCALVAULT_SRC/gui/ChangePinDialog.ui \
	$$LOCALVAULT_SRC/gui/SetupVaultDialog.ui

HEADERS += $$LOCALVAULT_SRC/gui/SecureStoreGUI.h
SOURCES += $$LOCALVAULT_SRC/gui/SecureStoreGUI.cpp

linux {
	PKGCONFIG += libsecret-1
	PKGCONFIG += libsodium
}
macx {
	LIBS += -framework Security -framework CoreFoundation
}
win32 {
	LIBS += -lcrypt32 -llibsodium
}

DISTFILES += \
	../AGENTS.md \
	../README.md

