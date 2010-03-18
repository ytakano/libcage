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

#include "udphandler.hpp"

#include "cagetypes.hpp"

#include <stdio.h>
#include <string.h>
#include <memory.h>

#ifndef WIN32
#include <netdb.h>
#include <unistd.h>
#endif

#include <iterator>

namespace libcage {
#ifndef WIN32
        int
        closesocket(SOCKET fd)
        {
                return close(fd);
        }
#endif // WIN32

        void
        udp_callback(int fd, short event, void *arg)
        {
                udphandler           &udp  = *(udphandler*)arg;
                udphandler::callback &func = *udp.m_callback;
                sockaddr_storage      from;
                packetbuf_ptr         pbuf = packetbuf::construct();

#ifndef WIN32
                ssize_t   len;
                socklen_t fromlen;
#else
                int len;
                int fromlen;
#endif // WIN32

                if (event == EV_TIMEOUT) {
                        func(udp, pbuf, NULL, 0, true);
                        return;
                }


                memset(&from, 0, sizeof(from));
                fromlen = sizeof(from);

                pbuf->use_whole();

                len = recvfrom(fd, pbuf->get_data(), pbuf->get_len(), 0,
                               (sockaddr*)&from, &fromlen);

                pbuf->set_len(len);

#ifndef WIN32
                if (len < 0) {
                        perror("recvfrom");
                        return;
                }
#else
                if (len == SOCKET_ERROR) {
                        perror("recvfrom");
                        return;
                }
#endif // WIN32

                if (len == 0) {
                        return;
                }

                func(udp, pbuf, (sockaddr*)&from, (int)fromlen, false);
        }

        udphandler::udphandler() : m_callback(NULL), m_opened(false)
        {

        }

        udphandler::~udphandler()
        {
                if (m_opened)
                        closesocket(m_socket);

                if (m_callback != NULL) {
                        unset_callback();
                        event_del(&m_event);
                }
        }

        void
        udphandler::init()
        {
#ifdef WIN32
                WSADATA wsaData;
                WSAStartup(MAKEWORD(2,0), &wsaData);
#endif // WIN32
        }

        void
        udphandler::clean_up()
        {
#ifdef WIN32
                WSACleanup();
#endif // WIN32
        }

        bool
        udphandler::get_sockaddr(sockaddr_storage *saddr, std::string host,
                                 int port)
        {
                addrinfo  hints;
                addrinfo* res = NULL;
                int       err;
                char      str[20];

                memset(&hints, 0, sizeof(hints));

                hints.ai_family   = m_domain;
                hints.ai_flags    = AI_PASSIVE;
                hints.ai_protocol = IPPROTO_UDP;
                hints.ai_socktype = SOCK_DGRAM;

                snprintf(str, sizeof(str), "%d", port);

                err = getaddrinfo(host.c_str(), str, &hints, &res);
                if (err != 0) {
                        perror("getaddrinfo");
                        return false;
                }

                memcpy(saddr, res->ai_addr, res->ai_addrlen);

                freeaddrinfo(res);

                return true;
        }

        void
        udphandler::sendto(const void *msg, int len, const sockaddr* to,
                           int tolen)
        {
#ifndef WIN32
                socklen_t slen = tolen;
                ssize_t sendlen;
                size_t  l = len;
#else
                int slen = tolen;
                int sendlen;
                int l = len;
#endif // WIN32

                sendlen = ::sendto(m_socket, msg, l, 0, to, slen);

#ifndef WIN32
                if (sendlen < 0) {
                        perror("sendto");
                }
#else
                if (sendlen == SOCKET_ERROR) {
                        perror("sendto");
                }
#endif // WIN32
        }

        void
        udphandler::sendto(const void *msg, int len, std::string host, int port)
        {
                sockaddr_storage saddr;
                if (! get_sockaddr(&saddr, host, port))
                        return;

#ifndef WIN32
                ssize_t sendlen = 0;
                size_t  l = len;
#else
                int sendlen = 0;
                int l = len;
#endif // WIN32

                if (m_domain == PF_INET) {
                        sendlen = ::sendto(m_socket, msg, l, 0,
                                           (sockaddr*)&saddr,
                                           sizeof(sockaddr_in));
                } else if (m_domain == PF_INET6) {
                        sendlen = ::sendto(m_socket, msg, l, 0,
                                           (sockaddr*)&saddr,
                                           sizeof(sockaddr_in6));
                }

#ifndef WIN32
                if (sendlen < 0) {
                        perror("sendto");
                }
#else
                if (sendlen == SOCKET_ERROR) {
                        perror("sendto");
                }
#endif // WIN32
        }

        void
        udphandler::set_callback(udphandler::callback *func, timeval *tout)
        {
                m_callback = func;
#ifdef WIN32
                event_set(&m_event, (int)m_socket, EV_READ | EV_PERSIST,
                          udp_callback, this);
#else
                event_set(&m_event, m_socket, EV_READ | EV_PERSIST,
                          udp_callback, this);
#endif // WIN32

                event_add(&m_event, tout);
        }

        void
        udphandler::set_callback(udphandler::callback *func)
        {
                set_callback(func, NULL);
        }

        void
        udphandler::unset_callback()
        {
                m_callback = NULL;

                event_del(&m_event);
        }

        bool
        udphandler::open(int domain, int port)
        {
                if (m_opened)
                        return false;

                m_socket = socket(domain, SOCK_DGRAM, 0);

#ifndef WIN32
                if (m_socket < 0) {
                        perror("socket");
                        return false;
                }
#else
                if (m_socket == INVALID_SOCKET) {
                        perror("socket");
                        return false;
                }
#endif // WIN32

                // bind port
                addrinfo  hints;
                addrinfo* res = NULL;
                int       err;
                char      str[20];

                memset(&hints, 0, sizeof(hints));

                hints.ai_family   = domain;
                hints.ai_flags    = AI_PASSIVE;
                hints.ai_protocol = IPPROTO_UDP;
                hints.ai_socktype = SOCK_DGRAM;

                snprintf(str, sizeof(str), "%d", port);

                err = getaddrinfo(NULL, str, &hints, &res);
                if (err != 0) {
                        perror("getaddrinfo");
                        closesocket(m_socket);
                        return false;
                }

                if (bind(m_socket, res->ai_addr, res->ai_addrlen) < 0) {
                        perror("bind");
                        closesocket(m_socket);
                        return false;
                }

                freeaddrinfo(res);

                // set SO_REUSEADDR
                int             optval;
                optval = 1;
                setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR,
                           &optval, sizeof (optval));


                m_opened = true;
                m_domain = domain;

                return true;
        }

        void
        udphandler::close()
        {
                unset_callback();

                closesocket(m_socket);
                m_opened = false;
        }

        uint16_t
        udphandler::get_domain()
        {
                if (m_domain == PF_INET)
                        return domain_inet;
                else if (m_domain == PF_INET6)
                        return domain_inet6;

                return 0;
        }

        uint16_t
        udphandler::get_port()
        {
                if (!m_opened)
                        return 0;


                sockaddr_storage saddr;

#ifndef WIN32
                socklen_t slen;
                slen = sizeof(saddr);

                if (getsockname(m_socket, (sockaddr*)&saddr, &slen) < 0) {
                        perror("getsockname");
                        return 0;
                }
#else
                int slen;
                slen = sizeof(saddr);

                if (getsockname(m_socket, (sockaddr*)&saddr, &slen) ==
                    SOCKET_ERROR) {
                        perror("getsockname");
                        return 0;
                }
#endif // WIN32

                if (saddr.ss_family == PF_INET) {
                        sockaddr_in *in = (sockaddr_in*)&saddr;
                        return in->sin_port;
                } else if (saddr.ss_family == PF_INET6) {
                        sockaddr_in6 *in6 = (sockaddr_in6*)&saddr;
                        return in6->sin6_port;
                }

                return 0;
        }


#ifdef DEBUG
        class udp_func : public udphandler::callback {
        public:
                virtual void operator() (udphandler &udp, void *buf,
                                         int len, sockaddr *from,
                                         int fromlen, bool is_timeout) {
                        sockaddr_in *in = (sockaddr_in*)from;
                        
                        int n = *(int*)buf;
                        printf("%d: from = %d\n", n, ntohs(in->sin_port));

                        sleep(1);

                        n++;
                        udp.sendto(&n, sizeof(n), "localhost",
                                   ntohs(in->sin_port));
                }
        };

        void
        udphandler::test_udp()
        {
                udphandler *udp1, *udp2;
                udp_func   *f1, *f2;

                udp1 = new udphandler();
                udp2 = new udphandler();

                f1 = new udp_func();
                f2 = new udp_func();

                udphandler::init();

                udp1->open(PF_INET, 0);
                udp1->set_callback(f1);

                udp2->open(PF_INET, 0);
                udp2->set_callback(f2);

                int n = 0;
                udp2->sendto(&n, sizeof(n), "localhost",
                             ntohs(udp1->get_port()));
        }
#endif // DEBUG
}
