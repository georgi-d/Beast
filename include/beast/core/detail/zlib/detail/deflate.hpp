//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// This is a derivative work based on Zlib, copyright below:
/*
    Copyright (C) 1995-2013 Jean-loup Gailly and Mark Adler

    This software is provided 'as-is', without any express or implied
    warranty.  In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
       claim that you wrote the original software. If you use this software
       in a product, an acknowledgment in the product documentation would be
       appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
       misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.

    Jean-loup Gailly        Mark Adler
    jloup@gzip.org          madler@alumni.caltech.edu

    The data format used by the zlib library is described by RFCs (Request for
    Comments) 1950 to 1952 in the files http://tools.ietf.org/html/rfc1950
    (zlib format), rfc1951 (deflate format) and rfc1952 (gzip format).
*/

#ifndef BEAST_CORE_DETAIL_ZLIB_DETAIL_DEFLATE_HPP
#define BEAST_CORE_DETAIL_ZLIB_DETAIL_DEFLATE_HPP

#include <cassert>
#include <cstdint>

namespace beast {

struct limits
{
    // Upper limit on code length
    static std::uint8_t constexpr maxBits = 15;

    // Number of length codes, not counting the special END_BLOCK code
    static std::uint16_t constexpr lengthCodes = 29;

    // Number of literal bytes 0..255
    static std::uint16_t constexpr literals = 256;
    
    // Number of Literal or Length codes, including the END_BLOCK code
    static std::uint16_t constexpr lCodes = literals + 1 + lengthCodes;

    // Number of distance code lengths
    static std::uint16_t constexpr dCodes = 30;

    // Number of codes used to transfer the bit lengths
    static std::uint16_t constexpr blCodes = 19;

    // Number of distance codes
    static std::uint16_t constexpr distCodeLen = 512;

    static std::uint16_t constexpr minMatch = 3;
    static std::uint16_t constexpr maxMatch = 258;

    // Can't change minMatch without also changing code, see original zlib
    static_assert(minMatch==3, "");
};

std::uint8_t constexpr MAX_BITS = 15;

std::uint16_t constexpr LENGTH_CODES = 29;

std::uint16_t constexpr LITERALS = 256;

std::uint16_t constexpr L_CODES = LITERALS + 1 + LENGTH_CODES;

std::uint16_t constexpr D_CODES = 30;

std::uint16_t constexpr BL_CODES = 19;

std::uint16_t constexpr DIST_CODE_LEN = 512;

std::uint16_t constexpr MIN_MATCH = 3;

// Can't change MIN_MATCH without also changing code, see original zlib
static_assert(MIN_MATCH==3, "");

std::uint16_t constexpr MAX_MATCH = 258;



namespace detail {

/* Data structure describing a single value and its code string. */
struct ct_data
{
    std::uint16_t fc; // frequency count or bit string    
    std::uint16_t dl; // parent node in tree or length of bit string

    bool
    operator==(ct_data const& rhs) const
    {
        return fc == rhs.fc && dl == rhs.dl;
    }
};

struct deflate_tables
{
    // Number of extra bits for each length code
    std::uint8_t extra_lbits[LENGTH_CODES] = {
        0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
    };

    // Number of extra bits for each distance code
    std::uint8_t extra_dbits[D_CODES] = {
        0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
    };

    // Number of extra bits for each bit length code
    std::uint8_t extra_blbits[BL_CODES] = {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,3,7
    };

    // The lengths of the bit length codes are sent in order
    // of decreasing probability, to avoid transmitting the
    // lengths for unused bit length codes.
    std::uint8_t bl_order[BL_CODES] = {
        16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
    };

    ct_data ltree[L_CODES + 2];

    ct_data dtree[D_CODES];

    // Distance codes. The first 256 values correspond to the distances
    // 3 .. 258, the last 256 values correspond to the top 8 bits of
    // the 15 bit distances.
    std::uint8_t dist_code[DIST_CODE_LEN];

    std::uint8_t length_code[MAX_MATCH-MIN_MATCH+1];

    std::uint8_t base_length[LENGTH_CODES];

    std::uint16_t base_dist[D_CODES];
};

template<class = void>
unsigned
bi_reverse(unsigned code, int len)
{
    unsigned res = 0;
    do
    {
        res |= code & 1;
        code >>= 1;
        res <<= 1;
    }
    while(--len > 0);
    return res >> 1;
}

/* Generate the codes for a given tree and bit counts (which need not be optimal).
   IN assertion: the array bl_count contains the bit length statistics for
   the given tree and the field len is set for all tree elements.
   OUT assertion: the field code is set for all tree elements of non
       zero code length.
*/
template<class = void>
void
gen_codes(ct_data *tree, int max_code, std::uint16_t *bl_count)
{
    std::uint16_t next_code[MAX_BITS+1]; /* next code value for each bit length */
    std::uint16_t code = 0;              /* running code value */
    int bits;                  /* bit index */
    int n;                     /* code index */

    // The distribution counts are first used to
    // generate the code values without bit reversal.
    for (bits = 1; bits <= MAX_BITS; bits++) {
        next_code[bits] = code = (code + bl_count[bits-1]) << 1;
    }
    // Check that the bit counts in bl_count are consistent.
    // The last code must be all ones.
    assert(code + bl_count[MAX_BITS]-1 == (1<<MAX_BITS)-1);
    for (n = 0;  n <= max_code; n++)
    {
        int len = tree[n].dl;
        if (len == 0)
            continue;
        tree[n].fc = bi_reverse(next_code[len]++, len);
    }
}

template<class = void>
deflate_tables const&
get_deflate_tables()
{
    struct init
    {
        deflate_tables tables;

        init()
        {
            // number of codes at each bit length for an optimal tree
            //std::uint16_t bl_count[MAX_BITS+1];

            // Initialize the mapping length (0..255) -> length code (0..28)
            std::uint16_t length = 0;
            for(std::uint8_t code = 0; code < LENGTH_CODES-1; ++code)
            {
                tables.base_length[code] = length;
                for(std::size_t n = 0; n < (1U<<tables.extra_lbits[code]); ++n)
                    tables.length_code[length++] = code;
            }
            assert(length == 256);
            // Note that the length 255 (match length 258) can be represented
            // in two different ways: code 284 + 5 bits or code 285, so we
            // overwrite length_code[255] to use the best encoding:
            tables.length_code[length-1] = LENGTH_CODES-1;

            // Initialize the mapping dist (0..32K) -> dist code (0..29)
            {
                std::uint8_t code;
                std::uint16_t dist = 0;
                for(code = 0; code < 16; code++)
                {
                    tables.base_dist[code] = dist;
                    for(std::size_t n = 0; n < (1U<<extra_dbits[code]); ++n)
                        tables.dist_code[dist++] = code;
                }
                assert(dist == 256);
                // from now on, all distances are divided by 128
                dist >>= 7;
                for(; code < D_CODES; ++code)
                {
                    tables.base_dist[code] = dist << 7;
                    for(std::size_t n = 0; n < (1U<<(extra_dbits[code]-7)); ++n)
                        tables.dist_code[256 + dist++] = code;
                }
                assert(dist == 256);
            }

            // Construct the codes of the static literal tree
            std::uint16_t bl_count[MAX_BITS+1];
            std::memset(bl_count, 0, sizeof(bl_count));
            std::size_t n = 0;
            while (n <= 143) tables.ltree[n++].dl = 8, bl_count[8]++;
            while (n <= 255) tables.ltree[n++].dl = 9, bl_count[9]++;
            while (n <= 279) tables.ltree[n++].dl = 7, bl_count[7]++;
            while (n <= 287) tables.ltree[n++].dl = 8, bl_count[8]++;
            // Codes 286 and 287 do not exist, but we must include them in the tree
            // construction to get a canonical Huffman tree (longest code all ones)
            gen_codes(tables.ltree, L_CODES+1, bl_count);

            for(n = 0; n < D_CODES; ++n)
            {
                tables.dtree[n].dl = 5;
                tables.dtree[n].fc = bi_reverse(n, 5);
            }
        }
    };
    static init const data;
    return data.tables;
}

} // detail
} // beast

#endif
