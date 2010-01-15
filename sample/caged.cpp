#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <event.h>

#include <libcage/cage.hpp>

#include <iostream>
#include <string>

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

#define DEBUG

#ifdef DEBUG
  #define D(X) X
#endif

extern char *optarg;
extern int optind, opterr, optopt;

boost::unordered_map<int, boost::shared_ptr<event> > events;
boost::unordered_map<std::string, boost::shared_ptr<libcage::cage> > cages;

typedef boost::char_separator<char> char_separator;
typedef boost::tokenizer<char_separator> tokenizer;
typedef boost::escaped_list_separator<char> esc_separator;
typedef boost::tokenizer<esc_separator> esc_tokenizer;

bool start_listen(int port);
void callback_accept(int fd, short ev, void *arg);
void callback_read(int fd, short ev, void *arg);
void replace(std::string &str, std::string from, std::string to);
void do_command(int sockfd, std::string command);
void process_new(int sockfd, esc_tokenizer::iterator &it,
		 const esc_tokenizer::iterator &end);

static const char * const SUCCESSED_NEW         = "200";

static const char * const ERR_UNKNOWN_COMMAND   = "400";
static const char * const ERR_INVALID_STATEMENT = "401";
static const char * const ERR_CANNOT_OPEN_PORT  = "402";
static const char * const ERR_ALREADY_EXIST     = "403";

/*
  new,node_name,port_number |
  new,node_name,port_number,global
  -> result,200,new,node_name,port_number |
     result,400 | 401,comment |
     result,402 | 403,new,node_name,port_number,comment |

  delete node_name

  join node_name 192.168.0.1 10000
  put node_name "key" "value" ttl
  get node_name "key"
 */

int
main(int argc, char *argv[])
{
        int  opt;
        int  port = 12080;
        bool is_daemon = false;

	while ((opt = getopt(argc, argv, "dp:")) != -1) {
		switch (opt) {
		case 'd':
                        is_daemon = true;
                        break;
                case 'p':
                        port = atoi(optarg);
                        break;
                }
        }

        event_init();

        if (start_listen(port) == false) {
                std::cerr << "can't listen port: " << port << std::endl;
                return -1;
        }

        event_dispatch();

        return 0;
}

bool
start_listen(int port)
{
        sockaddr_in saddr_in;
        event       *ev = new event;
        int         sockfd;
        int         on = 1;

        if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
                perror("sockfd");

                return false;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                       sizeof(on)) < 0) {
                perror("setsockopt");
        }

        memset(&saddr_in, 0, sizeof(saddr_in));

        saddr_in.sin_port = htons(port);
        saddr_in.sin_family = PF_INET;
        saddr_in.sin_addr.s_addr = htonl(0x7f000001);

        if (bind(sockfd, (sockaddr*)&saddr_in, sizeof(saddr_in)) < 0) {
                close(sockfd);

                perror("bind");

                return false;
        }

        if (listen(sockfd, 10) < 0) {
                close(sockfd);

                perror("listen");

                return false;
        }

        event_set(ev, sockfd, EV_READ | EV_PERSIST, &callback_accept, NULL);
        event_add(ev, NULL);

        return true;
}

void
callback_accept(int sockfd, short ev, void *arg)
{
        if (ev == EV_READ) {
                sockaddr_in saddr_in;
                socklen_t len = sizeof(saddr_in);
                int fd = accept(sockfd, (sockaddr*)&saddr_in, &len);

                boost::shared_ptr<event> readev(new event);

                events[fd] = readev;

                event_set(readev.get(), fd, EV_READ | EV_PERSIST,
                          &callback_read, NULL);
                event_add(readev.get(), NULL);

        }
}

void
callback_read(int sockfd, short ev, void *arg)
{
        ssize_t size;
        char    buf[1024 * 4];

        if (ev == EV_READ) {
        retry:
                size = recv(sockfd, buf, sizeof(buf) - 1, 0);

                if (size <= 0) {
                        if (size == -1) {
                                if (errno == EINTR)
                                        goto retry;

                                perror("recv");
                        }

                        event_del(events[sockfd].get());
                        events.erase(sockfd);

                        shutdown(sockfd, SHUT_RDWR);

                        return;
                }

                send(sockfd, buf, size, 0);

                buf[size - 1] = '\0';

                std::string    str(buf);
                replace(str, "\r\n", "\n");
                replace(str, "\r", "\n");

                char_separator sep("\n", "", boost::drop_empty_tokens);
                tokenizer      tokens(str, sep);

                for (tokenizer::iterator it = tokens.begin();
                     it != tokens.end(); ++it) {
                        do_command(sockfd, *it);
                }
        }
}

void
replace(std::string &str, std::string from, std::string to)
{
        for (std::string::size_type pos = str.find(from);
             pos != std::string::npos;
             pos = str.find(from, from.length() + pos)) {
                str.replace(pos, from.length(), to);
        }
}

void
do_command(int sockfd, std::string command)
{
        esc_separator sep("\\", ",", "\"");
        esc_tokenizer tokens(command, sep);

        esc_tokenizer::iterator it = tokens.begin();

        if (it == tokens.end())
                return;

        if (*it == "new") {
                D(std::cout << "process new" << std::endl);
                process_new(sockfd, ++it, tokens.end());
        } else if (*it == "delete") {
                D(std::cout << "process delete" << std::endl);
        } else {
                D(std::cout << "unknown command: " << *it << std::endl);

                char result[1024];

                // format: result,400,comment
                snprintf(result, sizeof(result), 
                         "result,400,unknown command. cannot recognize '%s'\n",
                         it->c_str());
                send(sockfd, result, strlen(result), 0);
        }
}

void
process_new(int sockfd, esc_tokenizer::iterator &it,
            const esc_tokenizer::iterator &end)
{
        std::string node_name;
        std::string esc_node_name;
        int         port;
        char        result[1024 * 4];
        bool        is_global = false;

        // read node_name
        if (it == end || it->length() == 0) {
                // there is no port number
                // format: result,401,comment
                snprintf(result, sizeof(result),
                         "result,401,node name is required\n");
                send(sockfd, result, strlen(result), 0);
                return;
        }

        node_name = *it;
        esc_node_name = *it;
        replace(esc_node_name, ",", "\\,");
        ++it;


        // read port number
        if (it == end) {
                // there is no port number
                // format: result,401,comment
                snprintf(result, sizeof(result),
                         "result,401,port number is required\n");
                send(sockfd, result, strlen(result), 0);
                return;
        }

        try {
                port = boost::lexical_cast<int>(*it);
        } catch (boost::bad_lexical_cast) {
                // no integer
                // format: result,401,comment
                snprintf(result, sizeof(result),
                         "result,401,'%s' is not a valid number\n",
                         it->c_str());
                send(sockfd, result, strlen(result), 0);
                return;
        }

        ++it;


        // read option
        if (it != end) {
                if (*it == "global")
                        is_global = true;
        }

        D(std::cout << "    node_name: " << node_name
                    << "\n    port:      " << port
                    << "\n    is_global: " << is_global << std::endl);

        // check whether the node_name has been used already or not
        if (cages.find(node_name) != cages.end()) {
                // the node name has been already used
                // format: result,403,new,node_name,port_number,comment |
                snprintf(result, sizeof(result),
                         "result,%s,new,%s,%d,the node name '%s' exists already\n",
                         ERR_ALREADY_EXIST, esc_node_name.c_str(), port,
                         esc_node_name.c_str());
                send(sockfd, result, strlen(result), 0);
                return;
        }

        boost::shared_ptr<libcage::cage> c(new libcage::cage);

        // open port
        if (c->open(PF_INET, port) == false) {
                // cannot open port
                // format: result,402,new,node_name,port_number,comment |
                snprintf(result, sizeof(result),
                         "result,%s,new,%s,%d,cannot open port(%d)\n",
                         ERR_CANNOT_OPEN_PORT, esc_node_name.c_str(),
                         port, port);
                send(sockfd, result, strlen(result), 0);
                return;
        }

        // set as global
        if (is_global) {
                c->set_global();
        }

        cages[node_name] = c;

        // send the result
        // format: result,200,new,node_name,port_number
        snprintf(result, sizeof(result), "result,%s,new,%s,%d\n", 
                 SUCCESSED_NEW, esc_node_name.c_str(), port);

        send(sockfd, result, strlen(result), 0);

        return;
}
