#include <brutal/codec/deflate/inflate.h>
#include <brutal/codec/errors.h>
#include <brutal/codec/zlib/zlib.h>
#include <brutal/io/mem_view.h>

IoResult zlib_decompress_data(const uint8_t *in, size_t in_len, const uint8_t *out, size_t out_len)
{
    // Input
    MemView in_view;
    mem_view_init(&in_view, in_len, in);
    IoReader reader = mem_view_reader(&in_view);

    // Output
    MemView out_view;
    mem_view_init(&out_view, out_len, out);
    IoWriter writer = mem_view_writer(&out_view);

    return deflate_decompress_stream(writer, reader);
}

IoResult zlib_decompress_stream(IoWriter writer, IoReader reader)
{
    uint8_t cmf, flg;
    // Read CMF (compression method & flags)
    io_read(reader, &cmf, 1);
    // Flags
    io_read(reader, &flg, 1);

    // The FCHECK value must be such that CMF and FLG, when viewed as
    // a 16-bit unsigned integer stored in MSB order (CMF*256 + FLG),
    // is a multiple of 31.
    if ((256 * cmf + flg) % 31)
    {
        return ERR(IoResult, ERR_INVALID_DATA);
    }

    // Check method is deflate
    if ((cmf & 0x0F) != 8)
    {
        return ERR(IoResult, ERR_NOT_IMPLEMENTED);
    }

    // Check window size is valid
    uint8_t window_bits = cmf >> 4;
    if (window_bits > 7)
    {
        return ERR(IoResult, ERR_INVALID_DATA);
    }

    // Check there is no preset dictionary
    if (flg & 0x20)
    {
        return ERR(IoResult, ERR_NOT_IMPLEMENTED);
    }

    // Get Adler-32 checksum of original data
    be_uint32_t value;
    io_read(reader, (uint8_t*)&value, 4);
    uint32_t adler32 = load_be(value);
    UNUSED(adler32);

    size_t decompressed = TRY(IoResult, deflate_decompress_stream(writer, reader));

    return OK(IoResult, decompressed);
}