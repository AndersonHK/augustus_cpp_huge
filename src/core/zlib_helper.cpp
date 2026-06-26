#include "zlib_helper.h"

#include "miniz/miniz.h"

#include <string.h>

int zlib_helper_decompress(void *input_buffer, const int input_length, void *output_buffer, const int output_buffer_length, int *output_length)
{
    if (!input_buffer || input_length <= 0 || !output_buffer || output_buffer_length < 0 || !output_length) {
        return 0;
    }
    if (output_buffer_length == 0) {
        *output_length = 0;
        return 0;
    }

    z_stream strm;
    memset(&strm, 0, sizeof(strm));

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    if (inflateInit(&strm) != Z_OK) {
        return 0;
    }

    strm.avail_in = input_length;
    strm.next_in = static_cast<const unsigned char *>(input_buffer);
    strm.avail_out = output_buffer_length;
    strm.next_out = static_cast<unsigned char *>(output_buffer);
    int result = Z_OK;
    while (result == Z_OK) {
        result = inflate(&strm, Z_NO_FLUSH);
        if (strm.avail_out == 0 && result == Z_OK) {
            break;
        }
    }
    inflateEnd(&strm);
    if (result != Z_STREAM_END || strm.avail_out != 0) {
        return 0;
    }
    *output_length = output_buffer_length - strm.avail_out;
    return 1;
}

int zlib_helper_compress(void *input_buffer, const int input_length, void *output_buffer, const int output_buffer_length, int *output_length)
{
    if (!input_buffer || input_length < 0 || !output_buffer || output_buffer_length <= 0 || !output_length) {
        return 0;
    }

    z_stream strm;
    memset(&strm, 0, sizeof(strm));

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    if (deflateInit(&strm, Z_BEST_SPEED) != Z_OK) {
        return 0;
    }

    strm.avail_in = input_length;
    strm.next_in = static_cast<const unsigned char *>(input_buffer);
    strm.avail_out = output_buffer_length;
    strm.next_out = static_cast<unsigned char *>(output_buffer);
    int result = deflate(&strm, Z_FINISH);
    deflateEnd(&strm);
    if (result != Z_STREAM_END || strm.avail_in != 0) {
        return 0;
    }

    *output_length = output_buffer_length - strm.avail_out;
    return 1;
}
