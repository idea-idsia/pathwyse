/*
* Bitset.h is a modified version of the Boost dynamic bitset class,
 * and adheres to the the Boost Software License, Version 1.0.

 * Modifications were made to suit specific needs and design preferences.
 * Any changes beyond this point do not necessarily reflect the views of Boost.
 *
 * License information for Boost Software License, Version 1.0 can be found here:
 * https://www.boost.org/LICENSE_1_0.txt
 */

#ifndef BITSET_H
#define BITSET_H

#include <algorithm>
#include <memory>
#include <numeric>
#include <cstdint>
#include <assert.h>
#include <limits>
#include <ostream>
#include <bit>

class Bitset {

public:
    static const size_t bits_per_uint64 = std::numeric_limits<uint64_t>::digits;

    // constructors
    Bitset() = default;

    Bitset(size_t num_bits, uint64_t value = 0)
    : m_num_bits((uint32_t) num_bits), m_num_words((uint32_t) calc_num_blocks(num_bits))
    {
        if(m_num_words) {
            m_bitset.reset(new uint64_t[m_num_words]);
            std::fill_n(m_bitset.get(), m_num_words, value);
            m_zero_unused_bits();
        }
    }

    Bitset(const Bitset& b)
    : m_num_bits(b.m_num_bits), m_num_words(b.m_num_words)
    {
        if(m_num_words) {
            m_bitset.reset(new uint64_t[m_num_words]);
            std::copy_n(b.m_bitset.get(), m_num_words, m_bitset.get());
        }
    }

    Bitset(Bitset&& b) noexcept = default;
    Bitset& operator=(Bitset&& b) noexcept = default;

    Bitset& operator=(const Bitset& b) {
        if(this == &b)
            return *this;

        //Reuses the buffer when the size matches (always the case)
        if(m_num_words != b.m_num_words) {
            m_bitset.reset(b.m_num_words ? new uint64_t[b.m_num_words] : nullptr);
            m_num_words = b.m_num_words;
        }

        m_num_bits = b.m_num_bits;
        if(m_num_words)
            std::copy_n(b.m_bitset.get(), m_num_words, m_bitset.get());

        return *this;
    }

    ~Bitset() = default;


    // utils functions
    uint64_t count_extra_bits() const { return bit_index(m_num_bits); }
    static size_t bit_index(size_t pos) {return pos % bits_per_uint64;}
    static size_t block_index(size_t pos) { return pos / bits_per_uint64; }
    static uint64_t bit_mask(size_t pos) { return uint64_t(1) << bit_index(pos); }
    static size_t calc_num_blocks(size_t num_bits) {return num_bits / bits_per_uint64
                                                           + static_cast<std::size_t>(num_bits % bits_per_uint64 != 0);}
    // If size() is not a multiple of bits_per_block
    // then not all the bits in the last block are used.
    // This function resets the unused bits
    void m_zero_unused_bits()
    {
        assert (m_num_words == calc_num_blocks(m_num_bits));

        const uint64_t extra_bits = count_extra_bits();
        if (extra_bits != 0)
            m_bitset[m_num_words - 1] &= (uint64_t(1) << extra_bits) - 1;
    }

    // reset functions
    Bitset& reset(size_t pos) {
        // reset the bit in position pos
        m_bitset[block_index(pos)] &= ~bit_mask(pos);
        return *this;
    }
    Bitset& reset()
    {
        std::fill_n(m_bitset.get(), m_num_words, uint64_t(0));
        return *this;
    }

    size_t size() const {
        return m_num_bits;
    }

    // clear
    void clear() {
        m_bitset.reset();
        m_num_bits = 0;
        m_num_words = 0;
    }

    // set() functions
    Bitset& set(size_t pos, bool val = true){
        assert(pos < m_num_bits);
        if (val)
            m_bitset[block_index(pos)] |= bit_mask(pos);

        else
            reset(pos);
        return *this;
    }

    Bitset& set() {
        std::fill_n(m_bitset.get(), m_num_words, std::numeric_limits<uint64_t>::max());
        m_zero_unused_bits();
        return *this;
    }

    bool get(size_t pos){
        assert (pos < m_num_bits);
        return (this->m_bitset[block_index(pos)] & bit_mask(pos)) != 0;
    }

    size_t count() const {
        size_t num = 0;
        for (size_t i = 0; i < m_num_words; ++i)
            num += std::popcount(m_bitset[i]);
        return num;
    }

    Bitset& operator&=(const Bitset& rhs){
        assert(size() == rhs.size());
        for (size_t i = 0; i < m_num_words; ++i)
            m_bitset[i] &= rhs.m_bitset[i];
        return *this;
    }

    Bitset& operator|=(const Bitset& rhs){
        assert(size() == rhs.size());
        for (size_t i = 0; i < m_num_words; ++i)
            m_bitset[i] |= rhs.m_bitset[i];
        return *this;
    }

    bool empty() const{
        return size() == 0;
    }

    Bitset& flip(){
        for (size_t i = 0; i < m_num_words; ++i)
            m_bitset[i] = ~m_bitset[i];
        m_zero_unused_bits();
        return *this;
    }

    friend bool operator==(const Bitset& a, const Bitset& b);

    bool any() const{
        for (size_t i = 0; i < m_num_words; ++i)
            if (m_bitset[i])
                return true;
        return false;
    }

    bool none() const{
        return not any();
    }

    friend bool is_subset(const Bitset& a, const Bitset& b);
    friend bool intersects(const Bitset& a, const Bitset& b);
    friend bool intersects_except(const Bitset& a, const Bitset& b, size_t pos);

private:
    std::unique_ptr<uint64_t[]> m_bitset;
    uint32_t m_num_bits = 0;
    uint32_t m_num_words = 0;
};

Bitset operator&(const Bitset& x, const Bitset& y);
Bitset operator|(const Bitset& x, const Bitset& y);
bool operator!=(Bitset& a, Bitset& b);
bool operator==(const Bitset& a, const Bitset& b);
bool intersects(const Bitset& a, const Bitset& b);
bool intersects_except(const Bitset& a, const Bitset& b, size_t pos);


template <typename Ch, typename Tr>
std::basic_ostream<Ch, Tr>&
operator<<(std::basic_ostream<Ch, Tr>& os,
           Bitset& b)
{

    using namespace std;

    const ios_base::iostate ok = ios_base::goodbit;
    ios_base::iostate err = ok;

    typename basic_ostream<Ch, Tr>::sentry cerberos(os);
    if (cerberos) {

        const Ch zero = '0';
        const Ch one = '1';

        try {

            typedef uint64_t bitset_size_type;
            typedef basic_streambuf<Ch, Tr> buffer_type;

            buffer_type *buf = os.rdbuf();
            // careful: os.width() is signed (and can be < 0)
            const bitset_size_type width = (os.width() <= 0) ? 0 : static_cast<bitset_size_type>(os.width());
            streamsize npad = (width <= b.size()) ? 0 : width - b.size();

            const Ch fill_char = os.fill();
            const ios_base::fmtflags adjustfield = os.flags() & ios_base::adjustfield;

            // if needed fill at left; pad is decreased along the way
            if (adjustfield != ios_base::left) {
                for (; 0 < npad; --npad)
                    if (Tr::eq_int_type(Tr::eof(), buf->sputc(fill_char))) {
                        err |= ios_base::failbit;
                        break;
                    }
            }

            if (err == ok) {
                // output the bitset
                for (bitset_size_type i = b.size(); 0 < i; --i) {
                    typename buffer_type::int_type
                            ret = buf->sputc(b.get(i - 1) ? one : zero);
                    if (Tr::eq_int_type(Tr::eof(), ret)) {
                        err |= ios_base::failbit;
                        break;
                    }
                }
            }

            if (err == ok) {
                // if needed fill at right
                for (; 0 < npad; --npad) {
                    if (Tr::eq_int_type(Tr::eof(), buf->sputc(fill_char))) {
                        err |= ios_base::failbit;
                        break;
                    }
                }
            }


            os.width(0);

        } catch (...) { // see std 27.6.1.1/4
            bool rethrow = false;
            try { os.setstate(ios_base::failbit); } catch (...) { rethrow = true; }

            if (rethrow)
                throw;
        }
    }

    if(err != ok)
        os.setstate(err); // may throw exception
    return os;

}
#endif
