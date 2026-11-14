/*
* Bitset.cpp is a modified version of the Boost dynamic bitset class,
 * and adheres to the the Boost Software License, Version 1.0.

 * Modifications were made to suit specific needs and design preferences.
 * Any changes beyond this point do not necessarily reflect the views of Boost.
 *
 * License information for Boost Software License, Version 1.0 can be found here:
 * https://www.boost.org/LICENSE_1_0.txt
 */

#include "bitset.h"

Bitset operator&(const Bitset& x, const Bitset& y)
{
    Bitset b(x);
    return b &= y;
}

Bitset operator|(const Bitset& x, const Bitset& y)
{
    Bitset b(x);
    return b |= y;
}

bool operator!=(Bitset& a, Bitset& b)
{
    return !(a == b);
}

bool operator==(const Bitset& a, const Bitset& b)
{
    return (a.m_num_bits == b.m_num_bits)
           and std::equal(a.m_bitset.get(), a.m_bitset.get() + a.m_num_words, b.m_bitset.get());
}

bool is_subset(const Bitset& a, const Bitset& b)
{
    assert(a.size() == b.size());

    for (size_t i = 0; i < a.m_num_words; ++i) {
        if ((a.m_bitset[i] & b.m_bitset[i]) != a.m_bitset[i])
            return false;
    }
    return true;
}

bool intersects(const Bitset& a, const Bitset& b)
{
    assert(a.size() == b.size());

    for (size_t i = 0; i < a.m_num_words; ++i) {
        if (a.m_bitset[i] & b.m_bitset[i])
            return true;
    }
    return false;
}

bool intersects_except(const Bitset& a, const Bitset& b, size_t pos)
{
    assert(a.size() == b.size());
    assert(pos < a.m_num_bits);

    const size_t skip_block = Bitset::block_index(pos);
    const uint64_t skip_mask = ~Bitset::bit_mask(pos);

    for (size_t i = 0; i < a.m_num_words; ++i) {
        uint64_t common = a.m_bitset[i] & b.m_bitset[i];
        if (i == skip_block)
            common &= skip_mask;
        if (common)
            return true;
    }
    return false;
}