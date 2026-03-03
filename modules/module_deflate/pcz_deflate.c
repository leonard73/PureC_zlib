#include "pczlib_internal.h"

#include <stdlib.h>
#include <string.h>

#define PCZ_MIN_MATCH 3
#define PCZ_MAX_MATCH 258
#define PCZ_WINDOW_SIZE 32768
#define PCZ_HASH_BITS 15
#define PCZ_HASH_SIZE (1u << PCZ_HASH_BITS)

typedef struct {
    uint8_t is_match;
    uint16_t lit;
    uint16_t len;
    uint16_t dist;
} pcz_token_t;

typedef struct {
    pcz_token_t *data;
    size_t size;
    size_t cap;
} pcz_tokens_t;

typedef struct {
    pcz_buffer_t *out;
    uint64_t bitbuf;
    unsigned bitcount;
} pcz_bit_writer_t;

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t pos;
    uint64_t bitbuf;
    unsigned bitcount;
} pcz_bit_reader_t;

typedef struct {
    uint16_t sym;
    uint8_t bits;
} pcz_huff_entry_t;

typedef struct {
    pcz_huff_entry_t *table;
    uint16_t *codes;
    uint8_t *lengths;
    int max_bits;
    uint32_t mask;
} pcz_huff_table_t;

static const uint16_t g_len_base[29] = {
    3,   4,   5,   6,   7,   8,   9,   10,  11,  13,
    15,  17,  19,  23,  27,  31,  35,  43,  51,  59,
    67,  83,  99,  115, 131, 163, 195, 227, 258
};

static const uint8_t g_len_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
    1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
    4, 4, 4, 4, 5, 5, 5, 5, 0
};

static const uint16_t g_dist_base[30] = {
    1,     2,     3,     4,     5,     7,     9,     13,
    17,    25,    33,    49,    65,    97,    129,   193,
    257,   385,   513,   769,   1025,  1537,  2049,  3073,
    4097,  6145,  8193,  12289, 16385, 24577
};

static const uint8_t g_dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2,
    3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10,
    11, 11, 12, 12, 13, 13
};

static uint8_t g_fixed_ll_len[288];
static uint16_t g_fixed_ll_rev[288];
static uint8_t g_fixed_dist_len[32];
static uint16_t g_fixed_dist_rev[32];
static int g_fixed_ready = 0;

static uint32_t pcz_hash3(const uint8_t *p) {
    return (uint32_t)(((uint32_t)p[0] * 251u + (uint32_t)p[1] * 509u + (uint32_t)p[2] * 1021u) & (PCZ_HASH_SIZE - 1u));
}

static uint16_t pcz_reverse_bits(uint16_t v, unsigned n) {
    uint16_t r = 0;
    for (unsigned i = 0; i < n; ++i) {
        r = (uint16_t)((r << 1) | (v & 1u));
        v >>= 1;
    }
    return r;
}

static pcz_status_t pcz_build_canonical_codes(const uint8_t *lengths, size_t count, uint16_t *codes, int *max_bits_out) {
    uint16_t bl_count[16] = {0};
    uint16_t next_code[16] = {0};
    uint16_t code = 0;
    int max_bits = 0;

    for (size_t i = 0; i < count; ++i) {
        if (lengths[i] > 15) {
            return PCZ_ERR_FORMAT;
        }
        if (lengths[i] > 0) {
            bl_count[lengths[i]]++;
            if ((int)lengths[i] > max_bits) {
                max_bits = lengths[i];
            }
        }
    }

    for (int bits = 1; bits <= 15; ++bits) {
        code = (uint16_t)((code + bl_count[bits - 1]) << 1);
        next_code[bits] = code;
    }

    for (size_t i = 0; i < count; ++i) {
        uint8_t len = lengths[i];
        if (len == 0) {
            codes[i] = 0;
        } else {
            codes[i] = next_code[len]++;
        }
    }

    if (max_bits_out) {
        *max_bits_out = max_bits;
    }
    return PCZ_OK;
}

static void pcz_init_fixed_tables(void) {
    uint16_t raw_ll[288];
    uint16_t raw_d[32];

    if (g_fixed_ready) {
        return;
    }

    for (int i = 0; i <= 143; ++i) {
        g_fixed_ll_len[i] = 8;
    }
    for (int i = 144; i <= 255; ++i) {
        g_fixed_ll_len[i] = 9;
    }
    for (int i = 256; i <= 279; ++i) {
        g_fixed_ll_len[i] = 7;
    }
    for (int i = 280; i <= 287; ++i) {
        g_fixed_ll_len[i] = 8;
    }

    for (int i = 0; i < 32; ++i) {
        g_fixed_dist_len[i] = 5;
    }

    (void)pcz_build_canonical_codes(g_fixed_ll_len, 288, raw_ll, NULL);
    (void)pcz_build_canonical_codes(g_fixed_dist_len, 32, raw_d, NULL);

    for (int i = 0; i < 288; ++i) {
        g_fixed_ll_rev[i] = pcz_reverse_bits(raw_ll[i], g_fixed_ll_len[i]);
    }
    for (int i = 0; i < 32; ++i) {
        g_fixed_dist_rev[i] = pcz_reverse_bits(raw_d[i], g_fixed_dist_len[i]);
    }

    g_fixed_ready = 1;
}

static pcz_status_t pcz_tokens_push(pcz_tokens_t *ts, const pcz_token_t *t) {
    if (ts->size == ts->cap) {
        size_t new_cap = ts->cap == 0 ? 1024 : ts->cap * 2;
        pcz_token_t *new_data = (pcz_token_t *)realloc(ts->data, new_cap * sizeof(*new_data));
        if (!new_data) {
            return PCZ_ERR_OOM;
        }
        ts->data = new_data;
        ts->cap = new_cap;
    }
    ts->data[ts->size++] = *t;
    return PCZ_OK;
}

static void pcz_tokens_free(pcz_tokens_t *ts) {
    free(ts->data);
    ts->data = NULL;
    ts->size = 0;
    ts->cap = 0;
}

static pcz_status_t pcz_bw_write_bits(pcz_bit_writer_t *bw, uint32_t bits, unsigned nbits) {
    while (nbits > 0) {
        bw->bitbuf |= ((uint64_t)(bits & 1u) << bw->bitcount);
        bw->bitcount++;
        bits >>= 1;
        nbits--;

        if (bw->bitcount >= 8) {
            uint8_t byte = (uint8_t)(bw->bitbuf & 0xFFu);
            pcz_status_t st = pcz_buffer_append_byte(bw->out, byte);
            if (st != PCZ_OK) {
                return st;
            }
            bw->bitbuf >>= 8;
            bw->bitcount -= 8;
        }
    }
    return PCZ_OK;
}

static pcz_status_t pcz_bw_flush(pcz_bit_writer_t *bw) {
    while (bw->bitcount > 0) {
        uint8_t byte = (uint8_t)(bw->bitbuf & 0xFFu);
        pcz_status_t st = pcz_buffer_append_byte(bw->out, byte);
        if (st != PCZ_OK) {
            return st;
        }
        if (bw->bitcount >= 8) {
            bw->bitbuf >>= 8;
            bw->bitcount -= 8;
        } else {
            bw->bitbuf = 0;
            bw->bitcount = 0;
        }
    }
    return PCZ_OK;
}

static int pcz_br_fill(pcz_bit_reader_t *br, unsigned want_bits) {
    while (br->bitcount < want_bits && br->pos < br->size) {
        br->bitbuf |= ((uint64_t)br->data[br->pos++] << br->bitcount);
        br->bitcount += 8;
    }
    return br->bitcount >= want_bits;
}

static uint32_t pcz_br_peek(pcz_bit_reader_t *br, unsigned nbits) {
    if (nbits == 0) {
        return 0;
    }
    (void)pcz_br_fill(br, nbits);
    return (uint32_t)(br->bitbuf & ((1ULL << nbits) - 1ULL));
}

static int pcz_br_drop(pcz_bit_reader_t *br, unsigned nbits) {
    if (!pcz_br_fill(br, nbits)) {
        return 0;
    }
    br->bitbuf >>= nbits;
    br->bitcount -= nbits;
    return 1;
}

static int pcz_br_read_bits(pcz_bit_reader_t *br, unsigned nbits, uint32_t *out) {
    if (!pcz_br_fill(br, nbits)) {
        return 0;
    }
    *out = (uint32_t)(br->bitbuf & ((1ULL << nbits) - 1ULL));
    br->bitbuf >>= nbits;
    br->bitcount -= nbits;
    return 1;
}

static void pcz_br_align_byte(pcz_bit_reader_t *br) {
    unsigned drop = br->bitcount & 7u;
    br->bitbuf >>= drop;
    br->bitcount -= drop;
}

static int pcz_length_to_code(uint16_t length, uint16_t *sym, uint16_t *extra_bits, uint16_t *extra_val) {
    for (int i = 0; i < 29; ++i) {
        uint16_t base = g_len_base[i];
        uint16_t span = (uint16_t)((1u << g_len_extra[i]) - 1u);
        if (length >= base && length <= (uint16_t)(base + span)) {
            *sym = (uint16_t)(257 + i);
            *extra_bits = g_len_extra[i];
            *extra_val = (uint16_t)(length - base);
            return 1;
        }
    }
    return 0;
}

static int pcz_dist_to_code(uint16_t dist, uint16_t *sym, uint16_t *extra_bits, uint16_t *extra_val) {
    for (int i = 0; i < 30; ++i) {
        uint16_t base = g_dist_base[i];
        uint16_t span = (uint16_t)((1u << g_dist_extra[i]) - 1u);
        if (dist >= base && dist <= (uint16_t)(base + span)) {
            *sym = (uint16_t)i;
            *extra_bits = g_dist_extra[i];
            *extra_val = (uint16_t)(dist - base);
            return 1;
        }
    }
    return 0;
}

static pcz_status_t pcz_lz77_tokenize(
    const uint8_t *input,
    size_t input_size,
    const pcz_compute_ops_t *ops,
    int level,
    pcz_tokens_t *tokens) {
    int *head = NULL;
    int *prev = NULL;
    size_t pos = 0;
    int max_chain;

    if (!input || !ops || !tokens) {
        return PCZ_ERR_ARG;
    }

    memset(tokens, 0, sizeof(*tokens));
    if (input_size == 0) {
        return PCZ_OK;
    }

    head = (int *)malloc(PCZ_HASH_SIZE * sizeof(int));
    prev = (int *)malloc(input_size * sizeof(int));
    if (!head || !prev) {
        free(head);
        free(prev);
        return PCZ_ERR_OOM;
    }

    for (size_t i = 0; i < PCZ_HASH_SIZE; ++i) {
        head[i] = -1;
    }
    for (size_t i = 0; i < input_size; ++i) {
        prev[i] = -1;
    }

    if (level <= 1) {
        max_chain = 8;
    } else if (level <= 4) {
        max_chain = 24;
    } else if (level <= 7) {
        max_chain = 64;
    } else {
        max_chain = 128;
    }

    while (pos < input_size) {
        pcz_token_t tok;
        uint16_t best_len = 0;
        uint16_t best_dist = 0;

        if (pos + PCZ_MIN_MATCH <= input_size) {
            uint32_t h = pcz_hash3(input + pos);
            int candidate = head[h];
            int chain_left = max_chain;

            prev[pos] = head[h];
            head[h] = (int)pos;

            while (candidate >= 0 && chain_left-- > 0) {
                size_t dist = pos - (size_t)candidate;
                if (dist > PCZ_WINDOW_SIZE) {
                    break;
                }

                if (input[candidate] == input[pos]) {
                    size_t max_len = input_size - pos;
                    if (max_len > PCZ_MAX_MATCH) {
                        max_len = PCZ_MAX_MATCH;
                    }
                    if (best_len > 0 && max_len > best_len) {
                        max_len = PCZ_MAX_MATCH;
                    }
                    {
                        size_t m = ops->match_len(input + candidate, input + pos, max_len);
                        if (m >= PCZ_MIN_MATCH && m > best_len) {
                            best_len = (uint16_t)m;
                            best_dist = (uint16_t)dist;
                            if (m == PCZ_MAX_MATCH) {
                                break;
                            }
                        }
                    }
                }

                candidate = prev[candidate];
            }
        }

        if (best_len >= PCZ_MIN_MATCH) {
            tok.is_match = 1;
            tok.len = best_len;
            tok.dist = best_dist;
            tok.lit = 0;
            {
                pcz_status_t st = pcz_tokens_push(tokens, &tok);
                if (st != PCZ_OK) {
                    free(head);
                    free(prev);
                    pcz_tokens_free(tokens);
                    return st;
                }
            }

            for (uint16_t i = 1; i < best_len; ++i) {
                size_t p = pos + i;
                if (p + PCZ_MIN_MATCH > input_size) {
                    break;
                }
                {
                    uint32_t h = pcz_hash3(input + p);
                    prev[p] = head[h];
                    head[h] = (int)p;
                }
            }
            pos += best_len;
        } else {
            tok.is_match = 0;
            tok.lit = input[pos];
            tok.len = 0;
            tok.dist = 0;
            {
                pcz_status_t st = pcz_tokens_push(tokens, &tok);
                if (st != PCZ_OK) {
                    free(head);
                    free(prev);
                    pcz_tokens_free(tokens);
                    return st;
                }
            }
            pos += 1;
        }
    }

    free(head);
    free(prev);
    return PCZ_OK;
}

static pcz_status_t pcz_encode_fixed_block(const pcz_tokens_t *tokens, pcz_buffer_t *out) {
    pcz_bit_writer_t bw;

    pcz_init_fixed_tables();

    bw.out = out;
    bw.bitbuf = 0;
    bw.bitcount = 0;

    {
        pcz_status_t st;
        st = pcz_bw_write_bits(&bw, 1u, 1u); /* BFINAL */
        if (st != PCZ_OK) {
            return st;
        }
        st = pcz_bw_write_bits(&bw, 1u, 2u); /* BTYPE=01 fixed */
        if (st != PCZ_OK) {
            return st;
        }
    }

    for (size_t i = 0; i < tokens->size; ++i) {
        const pcz_token_t *t = &tokens->data[i];
        if (!t->is_match) {
            uint16_t sym = t->lit;
            pcz_status_t st = pcz_bw_write_bits(&bw, g_fixed_ll_rev[sym], g_fixed_ll_len[sym]);
            if (st != PCZ_OK) {
                return st;
            }
        } else {
            uint16_t lsym, lextra_bits, lextra_val;
            uint16_t dsym, dextra_bits, dextra_val;
            pcz_status_t st;

            if (!pcz_length_to_code(t->len, &lsym, &lextra_bits, &lextra_val) ||
                !pcz_dist_to_code(t->dist, &dsym, &dextra_bits, &dextra_val)) {
                return PCZ_ERR_INTERNAL;
            }

            st = pcz_bw_write_bits(&bw, g_fixed_ll_rev[lsym], g_fixed_ll_len[lsym]);
            if (st != PCZ_OK) {
                return st;
            }
            if (lextra_bits > 0) {
                st = pcz_bw_write_bits(&bw, lextra_val, lextra_bits);
                if (st != PCZ_OK) {
                    return st;
                }
            }

            st = pcz_bw_write_bits(&bw, g_fixed_dist_rev[dsym], g_fixed_dist_len[dsym]);
            if (st != PCZ_OK) {
                return st;
            }
            if (dextra_bits > 0) {
                st = pcz_bw_write_bits(&bw, dextra_val, dextra_bits);
                if (st != PCZ_OK) {
                    return st;
                }
            }
        }
    }

    {
        pcz_status_t st = pcz_bw_write_bits(&bw, g_fixed_ll_rev[256], g_fixed_ll_len[256]);
        if (st != PCZ_OK) {
            return st;
        }
    }

    return pcz_bw_flush(&bw);
}

static void pcz_huff_table_free(pcz_huff_table_t *t) {
    if (!t) {
        return;
    }
    free(t->table);
    free(t->codes);
    free(t->lengths);
    t->table = NULL;
    t->codes = NULL;
    t->lengths = NULL;
    t->max_bits = 0;
    t->mask = 0;
}

static pcz_status_t pcz_huff_table_build(const uint8_t *lengths, size_t count, pcz_huff_table_t *out) {
    pcz_status_t st;
    int max_bits = 0;

    memset(out, 0, sizeof(*out));

    out->lengths = (uint8_t *)malloc(count);
    out->codes = (uint16_t *)malloc(count * sizeof(uint16_t));
    if (!out->lengths || !out->codes) {
        pcz_huff_table_free(out);
        return PCZ_ERR_OOM;
    }

    memcpy(out->lengths, lengths, count);

    st = pcz_build_canonical_codes(out->lengths, count, out->codes, &max_bits);
    if (st != PCZ_OK) {
        pcz_huff_table_free(out);
        return st;
    }

    if (max_bits == 0) {
        out->max_bits = 0;
        out->mask = 0;
        out->table = NULL;
        return PCZ_OK;
    }

    out->max_bits = max_bits;
    out->mask = (1u << max_bits) - 1u;
    out->table = (pcz_huff_entry_t *)calloc((size_t)1u << max_bits, sizeof(pcz_huff_entry_t));
    if (!out->table) {
        pcz_huff_table_free(out);
        return PCZ_ERR_OOM;
    }

    for (size_t sym = 0; sym < count; ++sym) {
        uint8_t len = out->lengths[sym];
        if (len == 0) {
            continue;
        }
        {
            uint16_t rev = pcz_reverse_bits(out->codes[sym], len);
            uint32_t step = 1u << len;
            uint32_t size = 1u << max_bits;
            for (uint32_t i = rev; i < size; i += step) {
                out->table[i].sym = (uint16_t)sym;
                out->table[i].bits = len;
            }
        }
    }

    return PCZ_OK;
}

static pcz_status_t pcz_huff_decode_symbol(pcz_bit_reader_t *br, const pcz_huff_table_t *table, uint16_t *sym_out) {
    uint32_t key;
    pcz_huff_entry_t e;

    if (!table || !sym_out || table->max_bits <= 0 || !table->table) {
        return PCZ_ERR_FORMAT;
    }

    (void)pcz_br_fill(br, (unsigned)table->max_bits);
    if (br->bitcount == 0) {
        return PCZ_ERR_FORMAT;
    }

    key = pcz_br_peek(br, (unsigned)table->max_bits) & table->mask;
    e = table->table[key];
    if (e.bits == 0) {
        return PCZ_ERR_FORMAT;
    }
    if (!pcz_br_drop(br, e.bits)) {
        return PCZ_ERR_FORMAT;
    }

    *sym_out = e.sym;
    return PCZ_OK;
}

static pcz_status_t pcz_decode_huffman_block(
    pcz_bit_reader_t *br,
    pcz_buffer_t *out,
    const pcz_huff_table_t *litlen,
    const pcz_huff_table_t *dist) {
    while (1) {
        uint16_t sym;
        pcz_status_t st = pcz_huff_decode_symbol(br, litlen, &sym);
        if (st != PCZ_OK) {
            return st;
        }

        if (sym < 256) {
            st = pcz_buffer_append_byte(out, (uint8_t)sym);
            if (st != PCZ_OK) {
                return st;
            }
        } else if (sym == 256) {
            return PCZ_OK;
        } else if (sym <= 285) {
            uint32_t extra = 0;
            uint16_t len;
            uint16_t dsym;
            uint16_t dist_val;

            {
                int idx = (int)sym - 257;
                len = g_len_base[idx];
                if (g_len_extra[idx] > 0) {
                    if (!pcz_br_read_bits(br, g_len_extra[idx], &extra)) {
                        return PCZ_ERR_FORMAT;
                    }
                    len = (uint16_t)(len + extra);
                }
            }

            st = pcz_huff_decode_symbol(br, dist, &dsym);
            if (st != PCZ_OK || dsym > 29) {
                return PCZ_ERR_FORMAT;
            }

            dist_val = g_dist_base[dsym];
            if (g_dist_extra[dsym] > 0) {
                if (!pcz_br_read_bits(br, g_dist_extra[dsym], &extra)) {
                    return PCZ_ERR_FORMAT;
                }
                dist_val = (uint16_t)(dist_val + extra);
            }

            if (dist_val == 0 || dist_val > out->size) {
                return PCZ_ERR_FORMAT;
            }

            for (uint16_t i = 0; i < len; ++i) {
                uint8_t b = out->data[out->size - dist_val];
                st = pcz_buffer_append_byte(out, b);
                if (st != PCZ_OK) {
                    return st;
                }
            }
        } else {
            return PCZ_ERR_FORMAT;
        }
    }
}

static pcz_status_t pcz_decode_stored_block(pcz_bit_reader_t *br, pcz_buffer_t *out) {
    uint32_t len, nlen;

    pcz_br_align_byte(br);

    if (!pcz_br_read_bits(br, 16, &len) || !pcz_br_read_bits(br, 16, &nlen)) {
        return PCZ_ERR_FORMAT;
    }
    if (((uint16_t)len ^ 0xFFFFu) != (uint16_t)nlen) {
        return PCZ_ERR_FORMAT;
    }

    for (uint32_t i = 0; i < len; ++i) {
        uint32_t v;
        if (!pcz_br_read_bits(br, 8, &v)) {
            return PCZ_ERR_FORMAT;
        }
        {
            pcz_status_t st = pcz_buffer_append_byte(out, (uint8_t)v);
            if (st != PCZ_OK) {
                return st;
            }
        }
    }
    return PCZ_OK;
}

static pcz_status_t pcz_decode_fixed_block(pcz_bit_reader_t *br, pcz_buffer_t *out) {
    pcz_status_t st;
    pcz_huff_table_t litlen;
    pcz_huff_table_t dist;

    pcz_init_fixed_tables();

    st = pcz_huff_table_build(g_fixed_ll_len, 288, &litlen);
    if (st != PCZ_OK) {
        return st;
    }
    st = pcz_huff_table_build(g_fixed_dist_len, 32, &dist);
    if (st != PCZ_OK) {
        pcz_huff_table_free(&litlen);
        return st;
    }

    st = pcz_decode_huffman_block(br, out, &litlen, &dist);

    pcz_huff_table_free(&litlen);
    pcz_huff_table_free(&dist);
    return st;
}

static pcz_status_t pcz_decode_dynamic_block(pcz_bit_reader_t *br, pcz_buffer_t *out) {
    static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    uint32_t hlit, hdist, hclen;
    uint8_t cl_lengths[19] = {0};
    uint8_t ll_lengths[286] = {0};
    uint8_t d_lengths[32] = {0};
    uint8_t combined[286 + 32] = {0};
    pcz_huff_table_t cl_table;
    pcz_huff_table_t litlen;
    pcz_huff_table_t dist;
    pcz_status_t st;

    memset(&cl_table, 0, sizeof(cl_table));
    memset(&litlen, 0, sizeof(litlen));
    memset(&dist, 0, sizeof(dist));

    if (!pcz_br_read_bits(br, 5, &hlit) || !pcz_br_read_bits(br, 5, &hdist) || !pcz_br_read_bits(br, 4, &hclen)) {
        return PCZ_ERR_FORMAT;
    }

    hlit += 257;
    hdist += 1;
    hclen += 4;

    if (hlit > 286 || hdist > 32) {
        return PCZ_ERR_FORMAT;
    }

    for (uint32_t i = 0; i < hclen; ++i) {
        uint32_t v;
        if (!pcz_br_read_bits(br, 3, &v)) {
            return PCZ_ERR_FORMAT;
        }
        cl_lengths[order[i]] = (uint8_t)v;
    }

    st = pcz_huff_table_build(cl_lengths, 19, &cl_table);
    if (st != PCZ_OK) {
        return st;
    }

    {
        uint32_t total = hlit + hdist;
        uint32_t i = 0;
        while (i < total) {
            uint16_t sym;
            st = pcz_huff_decode_symbol(br, &cl_table, &sym);
            if (st != PCZ_OK) {
                pcz_huff_table_free(&cl_table);
                return st;
            }

            if (sym <= 15) {
                combined[i++] = (uint8_t)sym;
            } else if (sym == 16) {
                uint32_t rep;
                uint8_t prev;
                if (i == 0 || !pcz_br_read_bits(br, 2, &rep)) {
                    pcz_huff_table_free(&cl_table);
                    return PCZ_ERR_FORMAT;
                }
                prev = combined[i - 1];
                rep += 3;
                if (i + rep > total) {
                    pcz_huff_table_free(&cl_table);
                    return PCZ_ERR_FORMAT;
                }
                while (rep--) {
                    combined[i++] = prev;
                }
            } else if (sym == 17) {
                uint32_t rep;
                if (!pcz_br_read_bits(br, 3, &rep)) {
                    pcz_huff_table_free(&cl_table);
                    return PCZ_ERR_FORMAT;
                }
                rep += 3;
                if (i + rep > total) {
                    pcz_huff_table_free(&cl_table);
                    return PCZ_ERR_FORMAT;
                }
                while (rep--) {
                    combined[i++] = 0;
                }
            } else if (sym == 18) {
                uint32_t rep;
                if (!pcz_br_read_bits(br, 7, &rep)) {
                    pcz_huff_table_free(&cl_table);
                    return PCZ_ERR_FORMAT;
                }
                rep += 11;
                if (i + rep > total) {
                    pcz_huff_table_free(&cl_table);
                    return PCZ_ERR_FORMAT;
                }
                while (rep--) {
                    combined[i++] = 0;
                }
            } else {
                pcz_huff_table_free(&cl_table);
                return PCZ_ERR_FORMAT;
            }
        }
    }

    pcz_huff_table_free(&cl_table);

    memcpy(ll_lengths, combined, hlit);
    memcpy(d_lengths, combined + hlit, hdist);

    if (ll_lengths[256] == 0) {
        return PCZ_ERR_FORMAT;
    }

    st = pcz_huff_table_build(ll_lengths, hlit, &litlen);
    if (st != PCZ_OK) {
        return st;
    }
    st = pcz_huff_table_build(d_lengths, hdist, &dist);
    if (st != PCZ_OK) {
        pcz_huff_table_free(&litlen);
        return st;
    }

    st = pcz_decode_huffman_block(br, out, &litlen, &dist);

    pcz_huff_table_free(&litlen);
    pcz_huff_table_free(&dist);
    return st;
}

pcz_status_t pcz_deflate_compress_fixed(
    const uint8_t *input,
    size_t input_size,
    pcz_buffer_t *out_deflate,
    const pcz_compute_ops_t *ops,
    int level) {
    pcz_tokens_t tokens;
    pcz_status_t st;

    if (!out_deflate || !ops || level < 0 || level > 9) {
        return PCZ_ERR_ARG;
    }

    pcz_buffer_init(out_deflate);
    memset(&tokens, 0, sizeof(tokens));

    st = pcz_lz77_tokenize(input, input_size, ops, level, &tokens);
    if (st != PCZ_OK) {
        return st;
    }

    st = pcz_encode_fixed_block(&tokens, out_deflate);
    pcz_tokens_free(&tokens);

    if (st != PCZ_OK) {
        pcz_buffer_free(out_deflate);
    }

    return st;
}

pcz_status_t pcz_deflate_decompress(
    const uint8_t *deflate_data,
    size_t deflate_size,
    pcz_buffer_t *output,
    const pcz_compute_ops_t *ops) {
    pcz_bit_reader_t br;
    uint32_t final = 0;

    (void)ops;

    if (!deflate_data || !output) {
        return PCZ_ERR_ARG;
    }

    pcz_buffer_init(output);

    br.data = deflate_data;
    br.size = deflate_size;
    br.pos = 0;
    br.bitbuf = 0;
    br.bitcount = 0;

    do {
        uint32_t btype;
        pcz_status_t st;

        if (!pcz_br_read_bits(&br, 1, &final) || !pcz_br_read_bits(&br, 2, &btype)) {
            pcz_buffer_free(output);
            return PCZ_ERR_FORMAT;
        }

        if (btype == 0) {
            st = pcz_decode_stored_block(&br, output);
        } else if (btype == 1) {
            st = pcz_decode_fixed_block(&br, output);
        } else if (btype == 2) {
            st = pcz_decode_dynamic_block(&br, output);
        } else {
            st = PCZ_ERR_FORMAT;
        }

        if (st != PCZ_OK) {
            pcz_buffer_free(output);
            return st;
        }
    } while (!final);

    return PCZ_OK;
}
