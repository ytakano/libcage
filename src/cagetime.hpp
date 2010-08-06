/*
 * Copyright (c) 2010, Yuuki Takano (ytakanoster@gmail.com).
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

#ifndef CAGETIME_HPP
#define CAGETIME_HPP

#include "common.hpp"

#ifdef WIN32
  #include <time.h>
  #include <windows.h>
  #if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
    #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
  #else
    #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
  #endif
#else
  #include <stddef.h>
  #include <sys/time.h>
#endif // WIN32

namespace libcage {
#ifdef WIN32
        struct timezone 
        {
                int  tz_minuteswest; /* minutes W of Greenwich */
                int  tz_dsttime;     /* type of dst correction */
        };
 
        int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif // WIN32

        class cagetime {
        public:
                cagetime()
                {
                        update();
                }

                void update()
                {
                        gettimeofday(&m_tval, NULL);
                }

                double operator- (cagetime& rhs)
                {
                        double sec1, sec2;

                        sec1 = (double)(m_tval.tv_sec +
                                        m_tval.tv_usec / 1000000.0);
                        sec2 = (double)(rhs.m_tval.tv_sec +
                                        rhs.m_tval.tv_usec / 1000000.0);

                        return sec1 - sec2;
                }

        private:
                timeval m_tval;
        };
}

#endif // CAGETIME_HPP
