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

#ifndef ADVERTISE_HPP
#define ADVERTISE_HPP

#include "common.hpp"

#include "bn.hpp"
#include "cagetypes.hpp"
#include "dtun.hpp"
#include "timer.hpp"
#include "peers.hpp"
#include "udphandler.hpp"

#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

namespace libcage {
        class advertise {
        private:
                static const time_t     advertise_ttl;
                static const time_t     advertise_timeout;
                static const time_t     refresh_interval;

        public:
                advertise(rand_uint &rnd, rand_real &drnd, const uint160_t &id,
                          timer &tm, udphandler &udp, peers &p, dtun &d);
                virtual ~advertise();

                void            recv_advertise(void *msg, sockaddr *from);
                void            recv_advertise_reply(void *msg, sockaddr *from);

                void            advertise_to(uint160_t &id, uint16_t domain,
                                             uint16_t port, void *addr);

                void            refresh();

        private:
                class timer_advertise : public timer::callback {
                public:
                        virtual void operator() ();

                        advertise      *p_advertise;
                        uint32_t        nonce;
                };

                typedef boost::shared_ptr<timer_advertise> timer_ptr;

                class timer_refresh : public timer::callback {
                public:
                        virtual void operator() ()
                        {
                                m_advertise.refresh();

                                timeval tval;
                                time_t  t;

                                t  = (time_t)((double)advertise::refresh_interval * m_advertise.m_drnd());
                                t += advertise::refresh_interval;

                                tval.tv_sec  = t;
                                tval.tv_usec = 0;

                                m_advertise.m_timer.set_timer(this, &tval);
                        }

                        timer_refresh(advertise &adv) : m_advertise(adv)
                        {
                                timeval tval;
                                time_t  t;

                                t  = (time_t)((double)advertise::refresh_interval * m_advertise.m_drnd());
                                t += advertise::refresh_interval;

                                tval.tv_sec  = t;
                                tval.tv_usec = 0;

                                m_advertise.m_timer.set_timer(this, &tval);
                        }

                        ~timer_refresh()
                        {
                                m_advertise.m_timer.unset_timer(this);
                        }

                        advertise      &m_advertise;
                };

                rand_uint      &m_rnd;
                rand_real      &m_drnd;

                const uint160_t        &m_id;
                timer          &m_timer;
                udphandler     &m_udp;
                peers          &m_peers;
                dtun           &m_dtun;
                timer_refresh   m_timer_refresh;
                boost::unordered_map<uint32_t, timer_ptr>       m_advertising;
                boost::unordered_map<uint160_t, time_t>         m_advertised;
        };
}

#endif // ADVERTISE_HPP
