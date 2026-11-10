#include "zs.h"
#include "../../zstd_stream.h"

bool ZS::decompress(std::function<int(char *, int)> const &input,
                    std::function<int(char const *, int)> const &output, filesize_t maxlen)
{
    return guitar_zstd::decompress(input, output, maxlen, error);
}

bool ZS::compress(std::function<int(char *, int)> const &input,
                  std::function<int(char const *, int)> const &output)
{
    return guitar_zstd::compress(input, output, error);
}
