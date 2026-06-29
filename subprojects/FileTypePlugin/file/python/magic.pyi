from typing import NamedTuple
from io import IOBase

MAGIC_NONE: int
NONE: int
MAGIC_DEBUG: int
DEBUG: int
MAGIC_SYMLINK: int
SYMLINK: int
MAGIC_COMPRESS: int
COMPRESS: int
MAGIC_DEVICES: int
DEVICES: int
MAGIC_MIME_TYPE: int
MIME_TYPE: int
MAGIC_CONTINUE: int
CONTINUE: int
MAGIC_CHECK: int
CHECK: int
MAGIC_PRESERVE_ATIME: int
PRESERVE_ATIME: int
MAGIC_RAW: int
RAW: int
MAGIC_ERROR: int
ERROR: int
MAGIC_MIME_ENCODING: int
MIME_ENCODING: int
MAGIC_MIME: int
MIME: int
MAGIC_APPLE: int
APPLE: int
MAGIC_NO_CHECK_COMPRESS: int
NO_CHECK_COMPRESS: int
MAGIC_NO_CHECK_TAR: int
NO_CHECK_TAR: int
MAGIC_NO_CHECK_SOFT: int
NO_CHECK_SOFT: int
MAGIC_NO_CHECK_APPTYPE: int
NO_CHECK_APPTYPE: int
MAGIC_NO_CHECK_ELF: int
NO_CHECK_ELF: int
MAGIC_NO_CHECK_TEXT: int
NO_CHECK_TEXT: int
MAGIC_NO_CHECK_CDF: int
NO_CHECK_CDF: int
MAGIC_NO_CHECK_TOKENS: int
NO_CHECK_TOKENS: int
MAGIC_NO_CHECK_ENCODING: int
NO_CHECK_ENCODING: int
MAGIC_NO_CHECK_BUILTIN: int
NO_CHECK_BUILTIN: int
MAGIC_PARAM_INDIR_MAX: int
PARAM_INDIR_MAX: int
MAGIC_PARAM_NAME_MAX: int
PARAM_NAME_MAX: int
MAGIC_PARAM_ELF_PHNUM_MAX: int
PARAM_ELF_PHNUM_MAX: int
MAGIC_PARAM_ELF_SHNUM_MAX: int
PARAM_ELF_SHNUM_MAX: int
MAGIC_PARAM_ELF_NOTES_MAX: int
PARAM_ELF_NOTES_MAX: int
MAGIC_PARAM_REGEX_MAX: int
PARAM_REGEX_MAX: int
MAGIC_PARAM_BYTES_MAX: int
PARAM_BYTES_MAX: int

class FileMagic(NamedTuple):
    mime_type: str
    encoding: str
    name: str

class Magic:
    def close(self) -> None: ...
    def file(self, filename: str | bytes) -> str | None: ...
    def descriptor(self, fd: int) -> str | None: ...
    def buffer(self, buf: str | bytes) -> str | None: ...
    def error(self) -> str | None: ...
    def setflags(self, flags: int) -> int: ...
    def load(self, filename: str | bytes | None = ...) -> int: ...
    def compile(self, dbs: str | bytes) -> int: ...
    def check(self, dbs: str | bytes) -> int: ...
    def list(self, dbs: str | bytes) -> int: ...
    def errno(self) -> int: ...
    def getparam(self, param: int) -> int: ...
    def setparam(self, param: int, value: int) -> int: ...

def open(flags: int) -> Magic | None: ...

class error(Exception): ...

def detect_from_filename(filename: str | bytes) -> FileMagic: ...
def detect_from_fobj(fobj: IOBase) -> FileMagic: ...
def detect_from_content(byte_content: str | bytes) -> FileMagic: ...
