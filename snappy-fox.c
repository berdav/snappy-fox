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

    prdebug("Copying literal %d bytes at (u:%d c:%d (%lu))\n",
            clen, *idx, offsetval, offsetval);

    memcpy(&data[*idx], &cdata[offsetval], clen);
    *idx += clen;

    return lenval;
}

static int offsetread(uint8_t *data, uint32_t *idx, uint32_t length,
              uint32_t clen, uint32_t coff) {
    uint32_t i;
    prdebug("Copying %d bytes offset %d (pos: %d)\n",
            clen, coff, *idx);

    /* Ignore invalid offset */
    if (*idx < coff || coff == 0)
        return -1;

    if (*idx + clen > length)
        return -1;

    if (coff >= clen) {
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
    return 0;
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
        uint8_t *data, size_t length, uint32_t *idx) {
    int32_t  off = 0;
    uint32_t cidx  = 0;
    uint32_t bytes = 0;
    uint32_t len = 0;
    uint8_t  ctype = 0;

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
            if (fwrite(data, 1, *idx, out) !=  *idx)
                return off;
            return off;
        }


        cidx += off;
    }

    return 0;
}

static FILE *open_read_file(const char *file) {
    FILE *in = stdin;
    if (strcmp(file, "-") != 0)
        in = fopen(file, "rb");
    return in;
}

static FILE *open_write_file(const char *file) {
    FILE *out = stdout;
    if (strcmp(file, "-") != 0)
        out = fopen(file, "wb");
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

    if (memcmp(reference_identifier, stream_identifier, 9) != 0)
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
                         data, MAX_UNCOMPRESSED_DATA_SIZE, &idx)) != 0) {
        goto return_point;
    }


    prinfo("End of decompression %lx\n", ftell(in));

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

static int snappy_decompress_frame(FILE *in, FILE *out) {
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
    fprintf(stderr, "    -h --help     This Help\n");
    fprintf(stderr, "    -v --version  Print Version and exit\n");
}

int main(int argc, char **argv) {
    int c;
    int ret = 0;
    FILE *in, *out;

    int option_idx = 0;
    static struct option flags[] = {
        {"version", no_argument, 0, 'v'},
        {"help",    no_argument, 0, 'h'},
        {0,         0,           0, 0}
    };

    while (c != -1) {
        c = getopt_long(argc, argv, "hv", flags, &option_idx);
        switch (c) {
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

    if (option_idx + argc < 3) {
        usage(argv[0]);
        return 1;
    }

#ifdef __AFL_LOOP
    while (__AFL_LOOP(1000)) {
#endif
    in = open_read_file(argv[option_idx+1]);
    if (in == NULL) {
        perror("fopen");
        ret = 1;
        goto exit_point;
    }

    out = open_write_file(argv[option_idx+2]);
    if (out == NULL) {
        perror("fopen");
        ret = 1;
        goto close_in;
    }

    if ((ret = snappy_decompress_frame(in, out)) != 0) {
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


