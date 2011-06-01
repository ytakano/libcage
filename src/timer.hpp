/*
 * Copyright (c) 2009, Yuuki Takano (ytakanoster@gmail.com).
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

#ifndef TIMER_HPP
#define TIMER_HPP

#include "common.hpp"

#include <event.h>

#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

namespace libcage {
        class timer {
        public:
                class callback {
                public:
                        virtual void    operator() () = 0;

                        callback() {}
                        virtual ~callback() {}

                        timer  *get_timer() { return m_timer; }

                        friend class    timer;
                        friend void     timer_callback(int fd, short event, void *arg);

                private:
                        timer   *m_timer;
                        timeval  m_scheduled;
                        timeval  m_interval;
                };

                virtual ~timer();


                friend void     timer_callback(int fd, short event, void *arg);

                void            set_timer(callback *func, timeval *t);
                void            unset_timer(callback *func);

        private:
                boost::unordered_map<callback*,
                                     boost::shared_ptr<event> >  m_events;


#ifdef DEBUG
        public:
                static void     test_timer();
#endif // DEBUG
        };
}

#endif // TIMER_HPP
