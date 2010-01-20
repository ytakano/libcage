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


typedef boost::char_separator<char> char_separator;
typedef boost::tokenizer<char_separator> tokenizer;
typedef boost::escaped_list_separator<char> esc_separator;
typedef boost::tokenizer<esc_separator> esc_tokenizer;
typedef boost::unordered_map<std::string, boost::shared_ptr<libcage::cage> > name2node_type;
typedef boost::unordered_map<int, boost::shared_ptr<event> > sock2ev_type;


sock2ev_type   sock2ev;
name2node_type name2node;


bool start_listen(int port);
void callback_accept(int fd, short ev, void *arg);
void callback_read(int fd, short ev, void *arg);
void replace(std::string &str, std::string from, std::string to);
void do_command(int sockfd, std::string command);
void process_new(int sockfd, esc_tokenizer::iterator &it,
		 const esc_tokenizer::iterator &end);
void process_delete(int sockfd, esc_tokenizer::iterator &it,
                    const esc_tokenizer::iterator &end);
void process_join(int sockfd, esc_tokenizer::iterator &it,
                  const esc_tokenizer::iterator &end);
void process_put(int sockfd, esc_tokenizer::iterator &it,
                 const esc_tokenizer::iterator &end);

static const char * const SUCCESSED_NEW         = "200";
static const char * const SUCCESSED_DELETE      = "201";
static const char * const SUCCESSED_JOIN        = "202";

static const char * const ERR_UNKNOWN_COMMAND   = "400";
static const char * const ERR_INVALID_STATEMENT = "401";
static const char * const ERR_CANNOT_OPEN_PORT  = "402";
static const char * const ERR_ALREADY_EXIST     = "403";
static const char * const ERR_DEL_NO_SUCH_NODE  = "404";
static const char * const ERR_JOIN_NO_SUCH_NODE = "405";
static const char * const ERR_JOIN_FAILED       = "406";

/*
  new,node_name,port_number |
  new,node_name,port_number,global
  -> result,200,new,node_name,port_number |
     result,400 | 401,comment |
     result,402 | 403,new,node_name,port_number,comment |

  delete,node_name
  -> result,201,delete,node_name |
     result,404,delete,node_name,comment

  join,node_name,host,port
  -> result,202,join,node_name,host,port
     result,400 | 401,comment
     result,405 | 406,join,node_name,host,port,comment

  put,node_name,key,value,ttl


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

                sock2ev[fd] = readev;

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

                        event_del(sock2ev[sockfd].get());
                        sock2ev.erase(sockfd);

                        shutdown(sockfd, SHUT_RDWR);

                        return;
                }

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
                process_delete(sockfd, ++it, tokens.end());
        } else if (*it == "join") {
                D(std::cout << "process join" << std::endl;);
                process_join(sockfd, ++it, tokens.end());
        } else if (*it == "put") {
                D(std::cout << "process put" << std::endl);
                process_put(sockfd, ++it, tokens.end());
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
        if (name2node.find(node_name) != name2node.end()) {
                // the node name has been already used
                // format: result,403,new,node_name,port_number,comment
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
                // format: result,402,new,node_name,port_number,comment
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

        name2node[node_name] = c;

        // send result
        // format: result,200,new,node_name,port_number
        snprintf(result, sizeof(result), "result,%s,new,%s,%d\n", 
                 SUCCESSED_NEW, esc_node_name.c_str(), port);

        send(sockfd, result, strlen(result), 0);

        return;
}

void
process_delete(int sockfd, esc_tokenizer::iterator &it,
               const esc_tokenizer::iterator &end)
{
        name2node_type::iterator it_n2n;
        std::string node_name;
        std::string esc_node_name;
        char        result[1024 * 4];


        if (it == end) {
                // there is no node_name
                // format: result,401,comment
                snprintf(result, sizeof(result),
                         "result,401,node name is required\n");
                send(sockfd, result, strlen(result), 0);
                return;
        }

        node_name = *it;
        esc_node_name = *it;
        replace(esc_node_name, ",", "\\,");

        it_n2n = name2node.find(node_name);
        if (it_n2n == name2node.end()) {
                // invalid node name
                // format: result,404,delete,node_name,comment
                snprintf(result, sizeof(result),
                         "result,%s,delete,%s,no such node named '%s'\n",
                         ERR_DEL_NO_SUCH_NODE, esc_node_name.c_str(),
                         esc_node_name.c_str());
                send(sockfd, result, strlen(result), 0);
                return;
        }

        D(std::cout << "    node_name: " << node_name);

        name2node.erase(it_n2n);

        // send result
        // format: result,201,delete,node_name
        snprintf(result, sizeof(result),
                 "result,%s,delete,%s\n",
                 SUCCESSED_DELETE, esc_node_name.c_str());
        send(sockfd, result, strlen(result), 0);
}

class func_join
{
public:
        std::string esc_node_name;
        std::string esc_host;
        int         port;
        int         sockfd;

        void operator() (bool is_join) {
                sock2ev_type::iterator it;
                char result[1024 * 4];

                it = sock2ev.find(sockfd);
                if (it == sock2ev.end()) {
                        std::cout << "no socket" << std::endl;
                        return;
                }

                if (is_join) {
                        // send result of the success
                        // format: result,202,join,node_name,host,port
                        snprintf(result, sizeof(result),
                                 "result,%s,join,%s,%s,%d",
                                 SUCCESSED_JOIN, esc_node_name.c_str(),
                                 esc_host.c_str(), port);
                        send(sockfd, result, strlen(result), 0);
                } else {
                        // send result of the fail
                        // format: result,406,join,node_name,host,port,comment
                        snprintf(result, sizeof(result),
                                 "result,%s,join,%s,%s,%d,failed to join to '%s:%d'",
                                 ERR_JOIN_FAILED, esc_node_name.c_str(),
                                 esc_host.c_str(), port, esc_host.c_str(),
                                 port);
                        send(sockfd, result, strlen(result), 0);
                }
        }
};

void process_join(int sockfd, esc_tokenizer::iterator &it,
                  const esc_tokenizer::iterator &end)
{
        std::string node_name;
        std::string esc_node_name;
        std::string host;
        std::string esc_host;
        int         port;
        func_join   func;
        char        result[1024 * 4];
        name2node_type::iterator it_n2n;

        if (it == end) {
                // there is no node_name
                // format: result,401,comment
                snprintf(result, sizeof(result),
                         "result,401,node name is required\n");
                send(sockfd, result, strlen(result), 0);
                return;
        }

        node_name = *it;
        esc_node_name = node_name;
        replace(esc_node_name, ",", "\\,");

        ++it;
        if (it == end) {
                // there is no host
                // format: result,401,comment
                snprintf(result, sizeof(result),
                         "result,401,host is required\n");
                send(sockfd, result, strlen(result), 0);
                return;
        }

        host = *it;
        esc_host = host;
        replace(esc_host, ",", "\\,");

        ++it;
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

        it_n2n = name2node.find(node_name);
        if (it_n2n == name2node.end()) {
                // invalid node name
                // format: result,405,join,node_name,host,port,comment
                snprintf(result, sizeof(result),
                         "result,%s,join,%s,%s,%d,no such node named '%s'\n",
                         ERR_JOIN_NO_SUCH_NODE, esc_node_name.c_str(),
                         host.c_str(), port, esc_node_name.c_str());
                send(sockfd, result, strlen(result), 0);
                return;
        }

        D(std::cout << "    node_name: " << node_name
                    << "\n    host: " << host
                    << "\n    port: " << port << std::endl;)

        func.esc_node_name = esc_node_name;
        func.esc_host      = esc_host;
        func.port          = port;
        func.sockfd        = sockfd;

        // join to the host:port
        it_n2n->second->join(host, port, func);
}

void process_put(int sockfd, esc_tokenizer::iterator &it,
                 const esc_tokenizer::iterator &end)
{
        std::string node_name;
        std::string key, value;
        uint16_t    ttl;
}
