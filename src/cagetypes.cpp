#include "cagetypes.hpp"

namespace libcage {
        cageaddr
        new_cageaddr(msg_hdr *hdr, sockaddr *saddr)
        {
                cageaddr addr;
                id_ptr   id(new uint160_t);

                id->from_binary(hdr->src, sizeof(hdr->src));

                addr.id = id;

                if (saddr->sa_family == PF_INET) {
                        in_ptr in(new sockaddr_in);
                        memcpy(in.get(), saddr, sizeof(sockaddr_in));
                        addr.domain = domain_inet;
                        addr.saddr  = in;
                } else if (saddr->sa_family == PF_INET6) {
                        in6_ptr in6(new sockaddr_in6);
                        memcpy(in6.get(), saddr, sizeof(sockaddr_in6));
                        addr.domain = domain_inet6;
                        addr.saddr  = in6;
                }
                
                return addr;
        }
}
