/**
 * Snappy-fox -- Firefox Morgue Cache de-compressor
 * Copyright (C) 2021 Davide Berardi <berardi.dav@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#define _POSIX_C_SOURCE 1

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#define MAX_COMPRESSED_DATA_SIZE   16777211
#define MAX_UNCOMPRESSED_DATA_SIZE 65536

#ifdef DEBUG
#define prdebug(f...) fprintf(stderr, "[ DEBUG ]"), fprintf(stderr, f)
#define prinfo(f...)  fprintf(stderr, "[ INFO  ]"), fprintf(stderr, f)
#else
#define prdebug(f...)
#define prinfo(f...)
#endif
#define prbanner(f...) fprintf(stderr, f)
#define prerror(f...)  fprintf(stderr, "[ ERROR ]"), fprintf(stderr, f)

#ifndef VERSION
#define VERSION "unknown"
#endif

/* Flags */
/* Unframed file stream, by default assume framed file */
static uint32_t unframed_stream = 0;
/* Ignore offset errors, by default consider them as fatal errors */
static uint32_t ignore_offset_errors = 0;
/* Byte to substitute offset corrupted values with */
static uint8_t offset_dummy_byte = 0xff;
/* Ignore altered magic bytes (sNaPpY) */
static uint32_t ignore_magic = 0;
/* Read offset */
static uint32_t read_offset = 0;
/* Consider CRC Errors */
static uint32_t consider_crc_errors = 0;
/* Use Firefox CRC32 implementation */
static uint32_t firefox_crc = 0;

/* CRC related functions */
static const uint32_t crc32c_table[] = {
	0x00000000, 0xf26b8303, 0xe13b70f7, 0x1350f3f4, 0xc79a971f,
	0x35f1141c, 0x26a1e7e8, 0xd4ca64eb, 0x8ad958cf, 0x78b2dbcc,
	0x6be22838, 0x9989ab3b, 0x4d43cfd0, 0xbf284cd3, 0xac78bf27,
	0x5e133c24, 0x105ec76f, 0xe235446c, 0xf165b798, 0x030e349b,
	0xd7c45070, 0x25afd373, 0x36ff2087, 0xc494a384, 0x9a879fa0,
	0x68ec1ca3, 0x7bbcef57, 0x89d76c54, 0x5d1d08bf, 0xaf768bbc,
	0xbc267848, 0x4e4dfb4b, 0x20bd8ede, 0xd2d60ddd, 0xc186fe29,
	0x33ed7d2a, 0xe72719c1, 0x154c9ac2, 0x061c6936, 0xf477ea35,
	0xaa64d611, 0x580f5512, 0x4b5fa6e6, 0xb93425e5, 0x6dfe410e,
	0x9f95c20d, 0x8cc531f9, 0x7eaeb2fa, 0x30e349b1, 0xc288cab2,
	0xd1d83946, 0x23b3ba45, 0xf779deae, 0x05125dad, 0x1642ae59,
	0xe4292d5a, 0xba3a117e, 0x4851927d, 0x5b016189, 0xa96ae28a,
	0x7da08661, 0x8fcb0562, 0x9c9bf696, 0x6ef07595, 0x417b1dbc,
	0xb3109ebf, 0xa0406d4b, 0x522bee48, 0x86e18aa3, 0x748a09a0,
	0x67dafa54, 0x95b17957, 0xcba24573, 0x39c9c670, 0x2a993584,
	0xd8f2b687, 0x0c38d26c, 0xfe53516f, 0xed03a29b, 0x1f682198,
	0x5125dad3, 0xa34e59d0, 0xb01eaa24, 0x42752927, 0x96bf4dcc,
	0x64d4cecf, 0x77843d3b, 0x85efbe38, 0xdbfc821c, 0x2997011f,
	0x3ac7f2eb, 0xc8ac71e8, 0x1c661503, 0xee0d9600, 0xfd5d65f4,
	0x0f36e6f7, 0x61c69362, 0x93ad1061, 0x80fde395, 0x72966096,
	0xa65c047d, 0x5437877e, 0x4767748a, 0xb50cf789, 0xeb1fcbad,
	0x197448ae, 0x0a24bb5a, 0xf84f3859, 0x2c855cb2, 0xdeeedfb1,
	0xcdbe2c45, 0x3fd5af46, 0x7198540d, 0x83f3d70e, 0x90a324fa,
	0x62c8a7f9, 0xb602c312, 0x44694011, 0x5739b3e5, 0xa55230e6,
	0xfb410cc2, 0x092a8fc1, 0x1a7a7c35, 0xe811ff36, 0x3cdb9bdd,
	0xceb018de, 0xdde0eb2a, 0x2f8b6829, 0x82f63b78, 0x709db87b,
	0x63cd4b8f, 0x91a6c88c, 0x456cac67, 0xb7072f64, 0xa457dc90,
	0x563c5f93, 0x082f63b7, 0xfa44e0b4, 0xe9141340, 0x1b7f9043,
	0xcfb5f4a8, 0x3dde77ab, 0x2e8e845f, 0xdce5075c, 0x92a8fc17,
	0x60c37f14, 0x73938ce0, 0x81f80fe3, 0x55326b08, 0xa759e80b,
	0xb4091bff, 0x466298fc, 0x1871a4d8, 0xea1a27db, 0xf94ad42f,
	0x0b21572c, 0xdfeb33c7, 0x2d80b0c4, 0x3ed04330, 0xccbbc033,
	0xa24bb5a6, 0x502036a5, 0x4370c551, 0xb11b4652, 0x65d122b9,
	0x97baa1ba, 0x84ea524e, 0x7681d14d, 0x2892ed69, 0xdaf96e6a,
	0xc9a99d9e, 0x3bc21e9d, 0xef087a76, 0x1d63f975, 0x0e330a81,
	0xfc588982, 0xb21572c9, 0x407ef1ca, 0x532e023e, 0xa145813d,
	0x758fe5d6, 0x87e466d5, 0x94b49521, 0x66df1622, 0x38cc2a06,
	0xcaa7a905, 0xd9f75af1, 0x2b9cd9f2, 0xff56bd19, 0x0d3d3e1a,
	0x1e6dcdee, 0xec064eed, 0xc38d26c4, 0x31e6a5c7, 0x22b65633,
	0xd0ddd530, 0x0417b1db, 0xf67c32d8, 0xe52cc12c, 0x1747422f,
	0x49547e0b, 0xbb3ffd08, 0xa86f0efc, 0x5a048dff, 0x8ecee914,
	0x7ca56a17, 0x6ff599e3, 0x9d9e1ae0, 0xd3d3e1ab, 0x21b862a8,
	0x32e8915c, 0xc083125f, 0x144976b4, 0xe622f5b7, 0xf5720643,
	0x07198540, 0x590ab964, 0xab613a67, 0xb831c993, 0x4a5a4a90,
	0x9e902e7b, 0x6cfbad78, 0x7fab5e8c, 0x8dc0dd8f, 0xe330a81a,
	0x115b2b19, 0x020bd8ed, 0xf0605bee, 0x24aa3f05, 0xd6c1bc06,
	0xc5914ff2, 0x37faccf1, 0x69e9f0d5, 0x9b8273d6, 0x88d28022,
	0x7ab90321, 0xae7367ca, 0x5c18e4c9, 0x4f48173d, 0xbd23943e,
	0xf36e6f75, 0x0105ec76, 0x12551f82, 0xe03e9c81, 0x34f4f86a,
	0xc69f7b69, 0xd5cf889d, 0x27a40b9e, 0x79b737ba, 0x8bdcb4b9,
	0x988c474d, 0x6ae7c44e, 0xbe2da0a5, 0x4c4623a6, 0x5f16d052,
	0xad7d5351
};

static uint32_t crc32c_lookup(uint8_t index) {
	return crc32c_table[index & 0xff];
}

static void crc32c(uint32_t *crc, const uint8_t *data, size_t len)
{
	int i = 0;
	for (i = 0 ; i < len; ++i) {
		uint32_t tabval = crc32c_lookup(*crc ^ data[i]);
		*crc = tabval ^ (*crc >> 8);
	}
}

static void crc32c_init(uint32_t *crc) {
	/* Initial value of CRC */
	*crc = 0xffffffff;
}

static void crc32c_fini(uint32_t *crc) {
	/* Firefox uses unreversed CRCs */
	if (!firefox_crc) {
		/* Final step is to reverse the CRC Value */
		*crc ^= 0xffffffff;
	}
	/* Mask the CRC */
	*crc = ((*crc >> 15) | (*crc << 17)) + 0xa282ead8;
}

/* Logarithm base two of the number */
static uint32_t log2_32(uint32_t n) {
    int32_t i = 0;
    for (i = 31; i >= 0; --i) {
        if (n & (1ul << i))
            return i + 1;
    }
    return 0;
}

static int check_overflow_shift(uint8_t c, uint32_t shift, uint32_t length) {
    /* Trivial check */
    if (c == 0 || shift == 0)
        return 0;

    /* The right value will overflow the number */
    if (7*shift + log2_32(c) > 31)
        return 1;

    return 0;
}

static uint32_t get_length(uint8_t *data, uint32_t length, uint32_t *bytes) {
    uint32_t l = 0;
    uint32_t shift = 0;
    uint8_t c = 0;
    uint8_t cbit = 1;
    while (cbit != 0) {
        c = *data;

        /* Return error */
        if (check_overflow_shift(c, shift, length))
            return MAX_UNCOMPRESSED_DATA_SIZE + 1;

        cbit = c & 0x80;

        c = c & ~0x80;

        l |= c << (7*shift);

        data++;
        shift++;
        (*bytes)++;
    }
    return l;
}

static int32_t parse_literal(uint8_t *cdata, uint32_t cidx, uint32_t clength,
             uint8_t *data,  uint32_t *idx, uint32_t length) {
    int32_t  lenval          = 0;
    uint32_t bytes_to_read   = 0;
    uint32_t offsetval       = 0;
    uint32_t lenval_u        = 0;
    uint32_t clen            = (uint32_t)(cdata[cidx] & 0xfc) >> 2;

    if (clen < 60) {
        bytes_to_read = 0;
    } else {
        bytes_to_read = clen - 59;
        clen = 0;
        memcpy(&clen, &cdata[cidx + 1], bytes_to_read);
    }
    clen += 1;

    offsetval = cidx + bytes_to_read + 1;
    if (offsetval > clength)
        return -1;

    /* Check integer overflow */
    lenval_u = clen + bytes_to_read +1;
    if (lenval_u > (uint32_t)(UINT32_MAX / 2))
        return -1;
    lenval = (int32_t)lenval_u;

    if (*idx > length || clen > length)
        return -1;

    if (*idx + clen > length)
        return -1;

    prdebug("Copying literal %d bytes at (u:%d c:%d (%u))\n",
            clen, *idx, offsetval, offsetval);

    memcpy(&data[*idx], &cdata[offsetval], clen);
    *idx += clen;

    return lenval;
}

static int offsetread(uint8_t *data, uint32_t *idx, uint32_t length,
              uint32_t clen, uint32_t coff) {
    int ret = 0;
    uint32_t i;
    prdebug("Copying %d bytes offset %d (pos: %d)\n",
            clen, coff, *idx);

    /* Ignore invalid offset */
    if (*idx < coff || coff == 0)
        ret = -1;

    if (*idx + clen > length)
        ret = -1;

    /* Check if we can ignore errors */
    if (ret != 0 && !ignore_offset_errors) {
        prerror("Offset error\n");
    } else if (ret != 0 && ignore_offset_errors) {
        prinfo("Ignoring offset errors\n");
        for (i = 0; i < clen; ++i)
            data[*idx+i] = offset_dummy_byte;
        *idx = *idx + clen;
        ret = 0;
    } else if (coff >= clen) {
        memcpy(&data[*idx], &data[*idx - coff], clen);
        *idx += clen;
    } else {
        for (i = 0; i < clen / coff ; ++i) {
            memcpy(&data[*idx], &data[*idx - coff], coff);
            *idx += coff;
        }
        memcpy(&data[*idx], &data[*idx - coff], clen % coff);
        *idx += clen % coff;
    }
    return ret;
}


static int32_t parse_copy1(uint8_t *cdata, uint32_t cidx, uint32_t clength,
                           uint8_t *data,  uint32_t *idx, uint32_t length) {
    int ret = 0;
    uint32_t clen  = (uint32_t)((cdata[cidx] & 0x1c) >> 2) + 4;
    uint32_t coff  = (uint32_t)((cdata[cidx] & 0xe0)) << 3;

    coff |= cdata[cidx+1];

    if ((ret = offsetread(data, idx, length, clen, coff)) != 0)
        return ret;

    return 2;
}

static int32_t parse_copy2(uint8_t *cdata, uint32_t cidx, uint32_t clength,
                           uint8_t *data,  uint32_t *idx, uint32_t length) {
    int ret = 0;
    uint32_t clen  = (uint32_t)((cdata[cidx] & 0xfc) >> 2) + 1;
    uint32_t coff  = 0;

    memcpy(&coff, &cdata[cidx+1], 2);

    if ((ret = offsetread(data, idx, length, clen, coff)) != 0)
        return ret;

    return 3;
}


static int32_t parse_copy4(uint8_t *cdata, uint32_t cidx, uint32_t clength,
                           uint8_t *data,  uint32_t *idx, uint32_t length) {
    int ret = 0;
    uint32_t clen  = (uint32_t)((cdata[cidx] & 0xfc) >> 2) + 1;
    uint32_t coff  = 0;

    memcpy(&coff, &cdata[cidx+1], 4);

    if ((ret = offsetread(data, idx, length, clen, coff)) != 0)
        return ret;

    return 5;
}

static int32_t parse_compressed_type(uint8_t compressed_type,
        uint8_t *cdata, uint32_t cidx, uint32_t clen,
        uint8_t *data,  uint32_t *idx, uint32_t len) {
    switch (compressed_type) {
        case 0:
            /* Literal stream */
            prdebug("Found Literal stream\n");
            return parse_literal(cdata, cidx, clen, data, idx, len);
        case 1:
            /* 1 byte offset */
            prdebug("Found single byte offset stream\n");
            return parse_copy1(cdata, cidx, clen, data, idx, len);
        case 2:
            /* 2 byte offset */
            prdebug("Found two bytes offset stream\n");
            return parse_copy2(cdata, cidx, clen, data, idx, len);
        case 3:
            /* 4 byte offset */
            prdebug("Found four bytes offset stream\n");
            return parse_copy4(cdata, cidx, clen, data, idx, len);
        default:
            prerror("Impossible compressed type!\n");
            return -1;
    }
}

static int snappy_uncompress(FILE *out, uint8_t *cdata, size_t clength,
        uint8_t *data, size_t length, uint32_t *idx, uint32_t *crc) {
    int32_t  off = 0;
    uint32_t cidx  = 0;
    uint32_t bytes = 0;
    uint32_t len = 0;
    uint8_t  ctype = 0;

    crc32c_init(crc);

    *idx = 0;

    prdebug("Decompressing %ld bytes\n", clength);

    len = get_length(cdata, clength, &bytes);
    prdebug("Uncompressed Length %d\n", len);
    if (len > MAX_UNCOMPRESSED_DATA_SIZE)
        return -1;

    cidx = bytes;

    while (cidx < clength && *idx < length) {
        ctype = cdata[cidx] & 0x03;

        off = parse_compressed_type(ctype, cdata, cidx, clength,
                                    data, idx,  len);
        if (off < 0) {
            /* Calculate CRC */
            crc32c(crc, data, *idx);
            crc32c_fini(crc);
            if (fwrite(data, 1, *idx, out) !=  *idx)
                return off;
            return off;
        }


        cidx += off;
    }

    crc32c(crc, data, *idx);
    crc32c_fini(crc);

    return 0;
}

static FILE *open_read_file(const char *file) {
    FILE *in = stdin;
    if (strcmp(file, "-") != 0)
        in = fopen(file, "rb");
    prdebug("Opening IN file: %s\n", file);
    if (in == NULL)
        return in;

    if (read_offset != 0) {
        prinfo("Seeking to offset %d\n", read_offset);
        fseek(in, read_offset, SEEK_SET);
    }
    return in;
}

static FILE *open_write_file(const char *file) {
    FILE *out = stdout;
    if (strcmp(file, "-") != 0)
        out = fopen(file, "wb");
    prdebug("Opening OUT file: %s\n", file);
    return out;
}

static int close_file(FILE *f) {
    if (f == stdin || f == stdout)
        return 0;

    return fclose(f);
}

static uint8_t get_chunktype(FILE *in) {
    uint8_t chunktype;
    if (fread(&chunktype, 1, 1, in) != 1)
        return 0x27;
    return chunktype;
}

static int parse_stream_identifier(FILE *in) {
    uint8_t stream_identifier[9];
    uint8_t reference_identifier[] = {
        0x06, 0x00, 0x00, 0x73,
        0x4e, 0x61, 0x50, 0x70,
        0x59
    };
    if (fread(stream_identifier, 9, 1, in) < 1)
        return -1;

    if (memcmp(reference_identifier, stream_identifier, 9) != 0 && !ignore_magic)
        return -1;

    return 0;
}

static int parse_compressed_data_chunk(FILE *in, FILE *out) {
    int ret = 0;
    size_t r = 0;
    uint8_t *c_data = NULL;
    uint8_t *data   = NULL;
    /* Compressed data */
    uint32_t c_length = 0;
    uint32_t c_read_length = 0;
    uint32_t crc = 0;
    uint32_t idx = 0;
    uint32_t uncompressed_crc = 0;

    c_data = malloc(MAX_COMPRESSED_DATA_SIZE);
    if (c_data == NULL) {
        ret = -1;
        goto exit_point;
    }

    data = malloc(MAX_UNCOMPRESSED_DATA_SIZE);
    if (data == NULL) {
        ret = -1;
        goto free_c_data;
    }

    r = fread(&c_length, 1, 3, in);
    if (r ==  0) {
        ret = 0;
        goto return_point;
    } else if (r < 3) {
        ret = -1;
        goto return_point;
    }

    r = fread(&crc, 1, 4, in);
    if (r == 0) {
        ret = 0;
        goto return_point;
    } else if (r < 4) {
        ret = -1;
        goto return_point;
    }

    if (c_length > MAX_COMPRESSED_DATA_SIZE) {
        ret = -1;
        goto return_point;
    }

    c_length--;

    prdebug("Compressed data chunk, len %d\n", c_length);

    if (c_length > MAX_COMPRESSED_DATA_SIZE) {
        ret = -1;
        goto return_point;
    }

    c_read_length = fread(c_data, 1, c_length - 3, in);

    if ((ret = snappy_uncompress(out, c_data, c_read_length,
                         data, MAX_UNCOMPRESSED_DATA_SIZE, &idx,
                         &uncompressed_crc)) != 0) {
        goto return_point;
    }


    prinfo("End of decompression %lx\n", ftell(in));
    if (crc != uncompressed_crc) {
        prinfo("Corrupted File! Expected CRC: %08x Calculated CRC: %08x\n", crc, uncompressed_crc);
        if (consider_crc_errors) {
            ret = -1;
            goto return_point;
	}
    }

    if (fwrite(data, 1, idx, out) < idx) {
        perror("fwrite");
        ret = -1;
        goto return_point;
    }

return_point:
    free(data);
free_c_data:
    free(c_data);
exit_point:
    return ret;
}

static int parse_unknown_chunktype(uint8_t chunktype) {
    if (chunktype == 0x27) {
        return 0;
    } else if (chunktype > 0x27 && chunktype <= 0x7f) {
        prerror("[frame] Unskippable chunk encountered %02hhx\n",
            (unsigned char) chunktype);
        return 1;
    } else {
        prerror("[frame] Skipping chunk %02hhx\n",
            (unsigned char) chunktype);
        return 0;
    }
}

static int parse_chunk(FILE *in, FILE *out, uint8_t chunktype) {
    prinfo("Got chunk %d\n", chunktype);
    switch (chunktype) {
        case 0xff:
            return parse_stream_identifier(in);
        case 0x00:
            return parse_compressed_data_chunk(in, out);
        case 0x01:
            /* TODO */
            // return parse_uncompressed_data_chunk(in, out);
            return -1;
        case 0xfe:
            /* TODO */
            // return parse_padding(in, out);
            return -1;
        default:
            return parse_unknown_chunktype(chunktype);
    }
}

static int snappy_decompress_unframed(FILE *in, FILE *out) {
    int ret = 0;
    int32_t r = 0;
    uint32_t read_head = 0;
    uint32_t write_head = 0;

    uint8_t *inbuf, *outbuf;

    int32_t read_size = MAX_COMPRESSED_DATA_SIZE;
    int32_t write_size = MAX_COMPRESSED_DATA_SIZE;

    inbuf = malloc(read_size);
    if (inbuf == NULL) {
        ret = -1;
        goto return_point;
    }

    outbuf = malloc(write_size);
    if (outbuf == NULL) {
        ret = -1;
        goto free_in;
    }

    read_size = fread(inbuf, 1, read_size, in);
    if (read_size <= 0) {
        ret = read_size;
        goto free_out;
    }

    while (read_head < read_size) {
        /* Skip unvalid compressed types, sledge */
        uint8_t ctype = inbuf[read_head] & 0x03;
        r = parse_compressed_type(ctype, inbuf, read_head, read_size,
                                  outbuf, &write_head, write_size);
        if (r < 0) {
            prerror("parse_compressed_type: %d\n", r);
            return r;
        }

        read_head += r;
        prinfo("offset: %u\n", read_head);
    }

    r = fwrite(outbuf, 1, write_head, out);
    if (r < 0)
        ret = r;

free_out:
    free(outbuf);
free_in:
    free(inbuf);
return_point:
    return ret;
}

static int snappy_decompress_framed(FILE *in, FILE *out) {
    int ret = 0;
    uint8_t chunktype;

    while (feof(in) == 0 && ferror(in) == 0 && ret == 0) {
        chunktype = get_chunktype(in);
        ret = parse_chunk(in, out, chunktype);
        prdebug("New run %ld %d %d\n", ftell(in), feof(in), ferror(in));
    }

    return ret;
}

static void version(const char *progname) {
    fprintf(stderr, "%s Version: %s\n", progname, VERSION);
}

static void usage(const char *progname) {
    fprintf(stderr, "Usage %s [options] <input file> <output file>\n",
		    progname);
    fprintf(stderr, "  files can be specified as - for stdin or stdout\n");
    fprintf(stderr, "  Options:\n");
    fprintf(stderr, "    -C --consider_crc_errors                      Consider CRC errors as fatal\n");
    fprintf(stderr, "    -E --ignore_offset_errors [substitution byte] Ignore any offset errors that occurs\n");
    fprintf(stderr, "    -M --ignore_magic                             Ignore altered magic bytes (sNaPpY)\n");
    fprintf(stderr, "    -O --read_offset [offset]                     Start reading file from offset\n");
    fprintf(stderr, "    -f --firefox                                  Use firefox's CRC algorithm\n");
    fprintf(stderr, "    -u --unframed                                 Assume Unframed stream in input file\n");
    fprintf(stderr, "    -h --help                                     This Help\n");
    fprintf(stderr, "    -v --version                                  Print Version and exit\n");
}

int main(int argc, char **argv) {
    int c = 0;
    int ret = 0;
    FILE *in, *out;

    int option_idx = 0;
    static struct option flags[] = {
        {"consider_crc_errors",  no_argument,       0, 'C'},
        {"ignore_offset_errors", optional_argument, 0, 'E'},
        {"ignore_magic",         no_argument,       0, 'M'},
        {"read_offset",          required_argument, 0, 'O'},
        {"firefox",              no_argument,       0, 'f'},
        {"unframed",             no_argument,       0, 'u'},
        {"version",              no_argument,       0, 'v'},
        {"help",                 no_argument,       0, 'h'},
        {0,                      0,                 0, 0}
    };

    while (c != -1) {
        c = getopt_long(argc, argv, "CO:E::fuhv", flags, &option_idx);
        switch (c) {
            case 'C':
                consider_crc_errors = 1;
                break;
            case 'E':
                ignore_offset_errors = 1;
                /* Set the dummy byte to the passed value */
                if (optarg != NULL)
                    offset_dummy_byte = (strtol(optarg, NULL, 0) & 0xff);
                break;
            case 'M':
                ignore_magic = 1;
                break;
            case 'O':
                if (optarg != NULL)
                    read_offset = strtol(optarg, NULL, 0);
                break;
            case 'f':
                firefox_crc = 1;
                break;
            case 'u':
                unframed_stream = 1;
                break;
            case 'h':
                usage(argv[0]);
                return 0;
            case 'v':
                version(argv[0]);
                return 0;
            default:
                prerror("Unknown Option: %c\n", c);
                continue;
            case -1:
                break;
        }
    }

    prdebug("Starting snappy-fox\n");

    if (argc - optind < 2) {
        usage(argv[0]);
        return 1;
    }

#ifdef __AFL_LOOP
    while (__AFL_LOOP(UINT32_MAX)) {
#endif
    in = open_read_file(argv[optind]);
    if (in == NULL) {
        perror("fopen read");
        ret = 1;
        goto exit_point;
    }

#ifdef __AFL_LOOP
    ftell(in);
#endif

    out = open_write_file(argv[optind + 1]);
    if (out == NULL) {
        perror("fopen write");
        ret = 1;
        goto close_in;
    }

    if (unframed_stream == 0)
        ret = snappy_decompress_framed(in, out);
    else
        ret = snappy_decompress_unframed(in, out);

    if (ret != 0) {
        prerror("decompress %d\n", ret);
        goto return_point;
    }

return_point:
    if (close_file(out) != 0)
        perror("close");
close_in:
    if (close_file(in) != 0)
        perror("close");
exit_point:
    prdebug("Exiting %d\n", ret);
#ifdef __AFL_LOOP
    }
#endif
    return ret;
}


