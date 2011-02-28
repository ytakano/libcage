/*
 * Copyright (c) 2006, Yuuki Takano (ytakanoster@gmail.com).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the writers nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BN_HPP
#define BN_HPP


#include "common.hpp"

#include <stdio.h>

#include <arpa/inet.h>

#include <exception>
#include <iostream>
#include <string>

#include <boost/functional/hash.hpp>

namespace libcage {
// this is a sizeof(T) * N bytes integer class
// T should be unsigned
// T should not be long long
        template <typename T, int N>
        class bn
        {
        public:
                bn();
                bn(const bn<T, N> &rhs);
                bn(T rhs);

                ~bn(){};

                // for assign
                bn<T, N>        operator =(const bn<T, N> &rhs);
                bn<T, N>        operator =(T rhs);

                // for addition
                bn<T, N>        operator +(const bn<T, N> &rhs) const;
                bn<T, N>        operator +(T rhs) const;
                bn<T, N>        operator +=(const bn<T, N> &rhs);
                bn<T, N>        operator +=(T rhs);
                // friend bn<T, N> operator +<T, N>(T lhs, const bn<T, N> &rhs);

                // for substraction
                bn<T, N>        operator -(const bn<T, N> &rhs) const;
                bn<T, N>        operator -(T rhs) const;
                bn<T, N>        operator -=(const bn<T, N> &rhs);
                bn<T, N>        operator -=(T rhs);
                // friend bn<T, N> operator -<T, N>(T lhs, const bn<T, N> &rhs);

                // for multiplication
                bn<T, N>        operator *(const bn<T, N> &rhs) const;
                bn<T, N>        operator *(T rhs) const;
                bn<T, N>        operator *=(const bn<T, N> &rhs);
                bn<T, N>        operator *=(T rhs);
                // friend bn<T, N> operator *<T, N>(T lhs, const bn<T, N> &rhs);

                // for exclusive or
                bn<T, N>        operator ^(const bn<T, N> &rhs) const;
                bn<T, N>        operator ^(T rhs) const;
                bn<T, N>        operator ^=(const bn<T, N> &rhs);
                bn<T, N>        operator ^=(T rhs);

                // for and
                bn<T, N>        operator &(const bn<T, N> &rhs) const;
                bn<T, N>        operator &(T rhs) const;
                bn<T, N>        operator &=(const bn<T, N> &rhs);
                bn<T, N>        operator &=(T rhs);

                // for not
                bn<T, N>        operator ~();

                // for multiplication with double
                bn<T, N>        operator *(double rhs) const;
                bn<T, N>        operator *=(double rhs);
                // friend bn<T, N> operator *<T, N>(double lhs, const bn<T, N> &rhs);

                // for shift
                bn<T, N>        operator <<(int rhs) const;
                bn<T, N>        operator >>(int rhs) const;
                bn<T, N>        operator <<=(int rhs);
                bn<T, N>        operator >>=(int rhs);

                // for comparison
                bool            operator ==(const bn<T, N> &rhs) const;
                bool            operator !=(const bn<T, N> &rhs) const;
                bool            operator <=(const bn<T, N> &rhs) const;
                bool            operator >=(const bn<T, N> &rhs) const;
                bool            operator <(const bn<T, N> &rhs) const;
                bool            operator >(const bn<T, N> &rhs) const;

                                operator T() const { return m_num[N - 1]; }
                                operator T&() { return m_num[N - 1]; }


                void            fill_zero();
                void            fill_max();
                void            to_binary(void *buf, int len) const;
                void            from_binary(const void *buf, int len);
                bool            is_zero() const;

                std::string     to_string() const;
                void            from_string(std::string str);
                void            from_string(const char *str);

                size_t          hash_value() const;

//  operator -(const bn &rhs);
//  operator *(const bn &rhs);

#ifdef DEBUG
                void            dump() const;
#endif

        private:
                T               m_num[N];

                static uint64_t m_exp_mask;

                void            shift_right(int bits, T arr[], int size) const;
                void            shift_left(int bits, T arr[], int size) const;

        };



// using namespace std;

        template <typename T, int N>
        uint64_t
        bn<T, N>::m_exp_mask = (((uint64_t)1 << 11) - 1) << 52;

        template <typename T, int N>
        bn<T, N>::bn()
        {

        }

        template <typename T, int N>
        bn<T, N>::bn(const bn<T, N> &rhs)
        {
                for (int i = 0; i < N; i++)
                        m_num[i] = rhs.m_num[i];
        }

        template <typename T, int N>
        bn<T, N>::bn(T rhs)
        {
                *this = rhs;
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator =(const bn<T, N> &rhs)
        {
                for (int i = 0; i < N; i++)
                        m_num[i] = rhs.m_num[i];

                return *this;
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator =(T rhs)
        {
                for (int i = 0; i < N - 1; i++)
                        m_num[i] = 0;

                m_num[N - 1] = rhs;

                return *this;
        }

#define PLUS_BN()                                       \
        do {                                            \
                T               carry = 0;              \
                T               tmp;                    \
                                                        \
                for (int i = N - 1; i >= 0; i--) {      \
                        tmp = m_num[i] + rhs.m_num[i];  \
                        if (tmp < m_num[i]) {           \
                                tmp = tmp + carry;      \
                                carry = 1;              \
                        } else {                        \
                                tmp = tmp + carry;      \
                                if (tmp < carry)        \
                                        carry = 1;      \
                                else                    \
                                        carry = 0;      \
                        }                               \
                        n.m_num[i] = tmp;               \
                }                                       \
                                                        \
                return n;                               \
        } while (0);

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator +(const bn<T, N> &rhs) const
        {
                bn<T, N>        n;

                PLUS_BN();
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator +=(const bn<T, N> &rhs)
        {
                bn<T, N>       &n = *this;

                PLUS_BN();
        }

#undef PLUS_BN

#define PLUS_T()                                        \
        do{                                             \
                T               carry;                  \
                T               tmp;                    \
                                                        \
                tmp = m_num[N - 1] + rhs;               \
                if (tmp < m_num[N - 1])                 \
                        carry = 1;                      \
                else                                    \
                        carry = 0;                      \
                                                        \
                n.m_num[N - 1] = tmp;                   \
                                                        \
                for (int i = N - 2; i >= 0; i--) {      \
                        tmp = m_num[i] + carry;         \
                        if (tmp < m_num[i])             \
                                carry = 1;              \
                        else                            \
                                carry = 0;              \
                        n.m_num[i] = tmp;               \
                }                                       \
                                                        \
                return n;                               \
        } while (0);

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator +(T rhs) const
        {
                bn<T, N>        n;

                PLUS_T();
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator +=(T rhs)
        {
                bn<T, N>       &n = *this;

                PLUS_T();
        }

#undef PLUS_T

/*
  template <typename T, int N>
  bn<T, N>
  operator +(T lhs, const bn<T, N> &rhs)
  {
  return rhs + lhs;
  }
*/

#define MINUS_BN()                                      \
        do {                                            \
                T               borrow = 0;             \
                T               tmp;                    \
                                                        \
                for (int i = N - 1; i >= 0; i--) {      \
                        tmp = m_num[i] - rhs.m_num[i];  \
                        if (tmp > m_num[i]) {           \
                                tmp = tmp - borrow;     \
                                borrow = 1;             \
                        } else {                        \
                                tmp = tmp - borrow;     \
                                if (tmp == (T)-1)       \
                                        borrow = 1;     \
                                else                    \
                                        borrow = 0;     \
                        }                               \
                        n.m_num[i] = tmp;               \
                }                                       \
                                                        \
                return n;                               \
        } while (0);

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator -(const bn<T, N> &rhs) const
        {
                bn<T, N>        n;

                MINUS_BN();
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator -=(const bn<T, N> &rhs)
        {
                bn<T, N>       &n = *this;

                MINUS_BN();
        }

#undef MINUS_BN

#define MINUS_T()                                       \
        do {                                            \
                T               borrow;                 \
                T               tmp;                    \
                                                        \
                tmp = m_num[N - 1] - rhs;               \
                if (tmp > m_num[N - 1])                 \
                        borrow = 1;                     \
                else                                    \
                        borrow = 0;                     \
                                                        \
                n.m_num[N - 1] = tmp;                   \
                                                        \
                for (int i = N - 2; i >= 0; i--) {      \
                        tmp = m_num[i] - borrow;        \
                        if (tmp > m_num[i])             \
                                borrow = 1;             \
                        else                            \
                                borrow = 0;             \
                        n.m_num[i] = tmp;               \
                }                                       \
                                                        \
                return n;                               \
                                                        \
        } while (0);

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator -(T rhs) const
        {
                bn<T, N>        n;

                MINUS_T();
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator -=(T rhs)
        {
                bn<T, N>       &n = *this;

                MINUS_T();
        }

#undef MINUS_T

/*
  template <typename T, int N>
  bn<T, N>
  operator -(T lhs, const bn<T, N> &rhs)
  {
  return rhs - lhs;
  }
*/

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator *(const bn<T, N> &rhs) const
        {
                bn<T, N>        n;
                T               carry;
                uint64_t        u;
                int             i, j;

                for (i = 0; i < N; i++)
                        n.m_num[i] = 0;

                for (i = N - 1; i >= 0; i--) {
                        carry = 0;
                        for (j = N - 1; j >= 0; j--) {
                                int ij = N - 1 - (N - 1 - i + N - 1 - j);
                                if (ij >= 0) {
                                        u = (uint64_t)n.m_num[ij] +
                                                (uint64_t)rhs.m_num[j] *
                                                (uint64_t)m_num[i] +
                                                (uint64_t)carry;
                                        n.m_num[ij] = u &
                                                (((uint64_t)1 <<
                                                  sizeof(m_num[0]) * 8) - 1);
                                        carry = u >> sizeof(m_num[0]) * 8;
                                }
                        }
                }

                return n;
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator *=(const bn<T, N> &rhs)
        {
                *this = *this * rhs;

                return *this;
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator *(T rhs) const
        {
                bn<T, N>        n;
                T               carry;
                uint64_t        u;
                int             i;

                for (i = 0; i < N; i++)
                        n.m_num[i] = 0;

                for (i = N - 1; i >= 0; i--) {
                        carry = 0;
                        u = (uint64_t)n.m_num[i] +
                                (uint64_t)rhs *
                                (uint64_t)m_num[i] +
                                (uint64_t)carry;
                        n.m_num[i] = u & (((uint64_t)1 <<
                                           sizeof(m_num[0]) * 8) - 1);
                        carry = u >> sizeof(m_num[0]) * 8;
                }

                return n;
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator *=(T rhs)
        {
                *this = *this * rhs;

                return *this;
        }

/*
  template <typename T, int N>
  bn<T, N>
  operator *(T lhs, const bn<T, N> &rhs)
  {
  bn<T, N>        n;

  n = rhs * lhs;

  return n;
  }
*/
        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator ^(const bn<T, N> &rhs) const
        {
                bn<T, N>        n;
                int             i;

                for (i = 0; i < N; i++) {
                        n.m_num[i] = m_num[i] ^ rhs.m_num[i];
                }

                return n;
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator ^=(const bn<T, N> &rhs)
        {
                int i;
                for (i = 0; i < N; i++) {
                        m_num[i] ^= rhs.m_num[i];
                }

                return *this;
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator ^(T rhs) const
        {
                bn<T, N>        n;

                n = *this;
                n.m_rhs[0] ^= rhs;

                return n;
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator ^=(T rhs)
        {
                m_num[0] ^= rhs;

                return *this;
        }


        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator &(const bn<T, N> &rhs) const
        {
                bn<T, N>        n;
                int             i;

                for (i = 0; i < N; i++) {
                        n.m_num[i] = m_num[i] & rhs.m_num[i];
                }

                return n;
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator &=(const bn<T, N> &rhs)
        {
                int i;
                for (i = 0; i < N; i++) {
                        m_num[i] &= rhs.m_num[i];
                }

                return *this;
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator &(T rhs) const
        {
                bn<T, N>        n;

                n = *this;
                n.m_rhs[0] &= rhs;

                return n;
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator &=(T rhs)
        {
                m_num[0] &= rhs;

                return *this;
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator ~()
        {
                bn<T, N>        n;

                for (int i = 0; i < N; i++)
                        n.m_num[i] = ~m_num[i];

                return n;
        }


        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator *(double rhs) const
        {
                bn<T, N>        n;
                T               flo[2 * N];
                T               result[2 * N];
                T               carry;
                uint64_t      bits;
                uint64_t      decimal;
                uint64_t      mask;
                uint64_t      u;
                long long       exp_part;
                int             shift;
                int             i, j;

                memcpy(&bits, &rhs, sizeof(bits));

                // bits = *((uint64_t*)&rhs);

                decimal = bits << 12;
                exp_part = bits & m_exp_mask;
                exp_part >>= 52;
                exp_part -= 1023;

                memset(flo, 0, sizeof(flo));

                flo[N - 1] = 1;

                shift = sizeof(decimal) / sizeof(flo[0]);
                shift = (shift - 1) * 8 * sizeof(flo[0]);

                mask = ~(T)0;

                for (i = N; i < 2 * N && shift >= 0;
                     shift -= sizeof(flo[0]) * 8, i++)
                        flo[i] = (T)((decimal >> shift) & mask);

                shift_left((int)exp_part, flo, sizeof(flo) / sizeof(flo[0]));

                for (i = 0; i < N; i++)
                        result[i] = 0;

                for (i = N - 1; i >= 0; i--) {
                        carry = 0;
                        for (j = 2 * N - 1; j >= 0; j--) {
                                int ij = 2 * N - 1 -
                                        (N - 1 - i + 2 * N - 1 - j);
                                if (ij >= 0) {
                                        u = (uint64_t)result[ij] +
                                                (uint64_t)flo[j] *
                                                (uint64_t)m_num[i] +
                                                (uint64_t)carry;
                                        result[ij] = (T)(u &
                                                         (((uint64_t)1 <<
                                                           sizeof(result[0]) *
                                                           8) - 1));
                                        carry = (T)(u >> sizeof(result[0]) * 8);
                                }
                        }
                }

                for (int i = 0; i < N; i++)
                        n.m_num[i] = result[i];

                return n;
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator *=(double rhs)
        {
                *this = *this * rhs;

                return *this;
        }

/*
  template <typename T, int N>
  bn<T, N>
  operator *(double lhs, const bn<T, N> &rhs)
  {
  bn<T, N>        n;

  n = rhs * lhs;

  return n;
  }
*/

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator <<(int rhs) const
        {
                bn<T, N>        n(*this);

                shift_left(rhs, n.m_num, N);

                return n;
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator >>(int rhs) const
        {
                bn<T, N>        n(*this);

                shift_right(rhs, n.m_num, N);

                return n;
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator <<=(int rhs)
        {
                shift_left(rhs, m_num, N);

                return *this;
        }

        template <typename T, int N>
        bn<T, N>
        bn<T, N>::operator >>=(int rhs)
        {
                shift_right(rhs, m_num, N);

                return *this;
        }

        template <typename T, int N>
        bool
        bn<T, N>::operator ==(const bn<T, N> &rhs) const
        {
                for (int i = 0; i < N; i++)
                        if (m_num[i] != rhs.m_num[i])
                                return false;

                return true;
        }

        template <typename T, int N>
        bool
        bn<T, N>::operator !=(const bn<T, N> &rhs) const
        {
                return !(*this == rhs);
        }

        template <typename T, int N>
        bool
        bn<T, N>::operator <=(const bn<T, N> &rhs) const
        {
                return !(*this > rhs);
        }

        template <typename T, int N>
        bool
        bn<T, N>::operator >=(const bn<T, N> &rhs) const
        {
                return !(*this < rhs);
        }

        template <typename T, int N>
        bool
        bn<T, N>::operator <(const bn<T, N> &rhs) const
        {
                for (int i = 0; i < N; i++)
                        if (m_num[i] < rhs.m_num[i])
                                return true;
                        else if (m_num[i] > rhs.m_num[i])
                                return false;

                return false;
        }

        template <typename T, int N>
        bool
        bn<T, N>::operator >(const bn<T, N> &rhs) const
        {
                for (int i = 0; i < N; i++)
                        if (m_num[i] > rhs.m_num[i])
                                return true;
                        else if (m_num[i] < rhs.m_num[i])
                                return false;

                return false;
        }

        template <typename T, int N>
        void
        bn<T, N>::shift_right(int bits, T arr[], int size) const
        {
                T               mask;
                int             n, m;
                int             i;

                if (bits < 0) {
                        shift_left(abs(bits), arr, size);
                        return;
                }

                n = bits / (sizeof(arr[0]) * 8);
                m = bits % (sizeof(arr[0]) * 8);

                for (i = size - 1; i >= n; i--)
                        arr[i] = arr[i - n];

                for (i = 0; i < n; i++)
                        arr[i] = 0;

                if (m == 0 || n >= size)
                        return;

                for (i = size - 1; i >= n + 1; i--) {
                        arr[i] >>= m;

                        mask = 1;
                        mask <<= m;
                        mask--;

                        arr[i] |= (mask & arr[i - 1]) <<
                                (sizeof(arr[0]) * 8 - m);
                }

                arr[n] >>= m;
        }

        template <typename T, int N>
        void
        bn<T, N>::shift_left(int bits, T arr[], int size) const
        {
                int             n, m;
                int             i;

                if (bits < 0) {
                        shift_right(abs(bits), arr, size);
                        return;
                }

                n = bits / (sizeof(arr[0]) * 8);
                m = bits % (sizeof(arr[0]) * 8);

                for (i = 0; i < size - n; i++)
                        arr[i] = arr[i + n];

                for (i = size - n; i < size; i++)
                        arr[i] = 0;

                if (m == 0 || n >= size)
                        return;

                for (i = 0; i < size - n - 1; i++) {
                        T msb;

                        msb = arr[i + 1];
                        msb >>= sizeof(T) * 8 - m;

                        arr[i] <<= m;
                        arr[i] |= msb;
                }

                arr[i] <<= m;
        }

        template <typename T, int N>
        void
        bn<T, N>::fill_zero()
        {
                memset(m_num, 0, N * sizeof(T));
        }

        template <typename T, int N>
        void
        bn<T, N>::fill_max()
        {
                for (int i = 0; i < N; i++)
                        m_num[i] = ~(T)0;
        }

        template <typename T, int N>
        void
        bn<T, N>::to_binary(void *buf, int len) const
        {
                T              *num;
                int             i;

                num = (T*)buf;

                for (i = 0; len >= (int)sizeof(T); i++, len -= (int)sizeof(T)) {
                        if (sizeof(T) == 4) {
                                uint32_t n = (uint32_t)m_num[i];
                                num[i] = htonl(n);
                        } else if (sizeof(T) == 2) {
                                uint16_t n = (uint16_t)m_num[i];
                                num[i] = htons(n);
                        } else if (sizeof(T) == 1) {
                                num[i] = m_num[i];
                        }
                }
        }

        template <typename T, int N>
        void
        bn<T, N>::from_binary(const void *buf, int len)
        {
                const T        *num;
                int             i;

                num = (T*)buf;

                memset(m_num, 0, sizeof(m_num));

                for (i = 0; i < N && len >= (int)sizeof(T);
                     i++, len -= (int)sizeof(T))
                {
                        if (sizeof(T) == 4) {
                                uint32_t n = (uint32_t)num[i];
                                m_num[i] = ntohl(n);
                        } else if (sizeof(T) == 2) {
                                uint16_t n = (uint16_t)num[i];
                                m_num[i] = ntohs(n);
                        }else if (sizeof(T) == 1) {
                                m_num[i] = num[i];
                        }
                }
        }

        template <typename T, int N>
        bool
        bn<T, N>::is_zero() const
        {
                T               zero = 0;

                for (int i = 0; i < N; i++)
                        if (m_num[i] != zero)
                                return false;

                return true;
        }

        static const char *hexstr[16] = {"0", "1", "2", "3",
                                         "4", "5", "6", "7",
                                         "8", "9", "a", "b",
                                         "c", "d", "e", "f"};

        template <typename T, int N>
        std::string
        bn<T, N>::to_string() const
        {
                T               num[N];
                unsigned int    i;
                char           *p;
                std::string     str;

                to_binary((char*)num, sizeof(num));

                try {
                        str = "";
                        p = (char*)num;

                        for (i = 0; i < sizeof(T) * N; i++) {
                                uint8_t t1, t2;
                                t1 = (0x00f0 & (p[i])) >> 4;
                                t2 = 0x000f & (p[i]);
                                str += hexstr[t1];
                                str += hexstr[t2];
                        }
                } catch(std::exception &e) {
                        std::cerr << "get_string(): " << e.what() << std::endl;
                        return str;
                }

                return str;
        }

        template <typename T, int N>
        void
        bn<T, N>::from_string(std::string str)
        {
                from_string(str.c_str());
        }

        template <typename T, int N>
        void
        bn<T, N>::from_string(const char *str)
        {
                fill_zero();

                while (str[0]) {
                        char c = str[0];
                        *this <<= 4;
                        if (c >= '0' && c <= '9') {
                                T n = (T)(str[0] - '0');
                                *this += n;
                        } else if (c >= 'A' && c <= 'F') {
                                T n = (T)(str[0] - 'A' + 10);
                                *this += n;
                        } else if (c >= 'a' && c <= 'f') {
                                T n = (T)(str[0] - 'a' + 10);
                                *this += n;
                        } else {
                                break;
                        }
                        str++;
                }
        }

        template <typename T, int N>
        size_t
        bn<T, N>::hash_value() const
        {
                size_t h = 0;

                for (int i = 0; i < N; i++) {
                        boost::hash_combine(h, m_num[i]);
                }

                return h;

        }

#ifdef DEBUG
        template <typename T, int N>
        void
        bn<T, N>::dump() const
        {
                std::string     str;
                int             i;
                char           *p;

                str = "";
                p = (char*)m_num;

                std::cout << "0x";
                for (i = 0; i < sizeof(T) * N; i++) {
                        uint8_t t1, t2;
                        t1 = (0x00f0 & (p[i])) >> 4;
                        t2 = 0x000f & (p[i]);
                        str += hexstr[t1];
                        str += hexstr[t2];
                }
                std::cout << str << std::endl;
        }
#endif

        typedef bn<uint32_t, 4> uint128_t;
        typedef bn<uint32_t, 5> uint160_t;

        inline
        size_t
        hash_value(const uint128_t &num)
        {
                return num.hash_value();
        }

        inline
        size_t
        hash_value(const uint160_t &num)
        {
                return num.hash_value();
        }
}

#endif // BN_HPP
