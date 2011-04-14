#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <event.h>

#include <libcage/cage.hpp>

#include <iostream>
#include <string>

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

// #define DEBUG

#ifdef DEBUG
  #define D(X) X
#else
  #define D(X)
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

void usage(char *cmd);
bool start_listen(int port);
void callback_accept(int fd, short ev, void *arg);
void callback_read(int fd, short ev, void *arg);
void replace(std::string &str, std::string from, std::string to);
void do_command(int sockfd, std::string command);
void process_set_id(int sockfd, esc_tokenizer::iterator &it,
                    const esc_tokenizer::iterator &end);
void process_new(int sockfd, esc_tokenizer::iterator &it,
		 const esc_tokenizer::iterator &end);
void process_delete(int sockfd, esc_tokenizer::iterator &it,
                    const esc_tokenizer::iterator &end);
void process_join(int sockfd, esc_tokenizer::iterator &it,
                  const esc_tokenizer::iterator &end);
void process_put(int sockfd, esc_tokenizer::iterator &it,
                 const esc_tokenizer::iterator &end);
void process_get(int sockfd, esc_tokenizer::iterator &it,
                 const esc_tokenizer::iterator &end);
void process_dump(int sockfd, esc_tokenizer::iterator &it,
                  const esc_tokenizer::iterator &end);

static const char * const SUCCEEDED_NEW         = "200";
static const char * const SUCCEEDED_DELETE      = "201";
static const char * const SUCCEEDED_JOIN        = "202";
static const char * const SUCCEEDED_PUT         = "203";
static const char * const SUCCEEDED_GET         = "204";
static const char * const SUCCEEDED_SET_ID      = "205";

static const char * const ERR_UNKNOWN_COMMAND   = "400";
static const char * const ERR_INVALID_STATEMENT = "401";
static const char * const ERR_CANNOT_OPEN_PORT  = "402";
static const char * const ERR_ALREADY_EXIST     = "403";
static const char * const ERR_DEL_NO_SUCH_NODE  = "404";
static const char * const ERR_JOIN_NO_SUCH_NODE = "405";
static const char * const ERR_JOIN_FAILED       = "406";
static const char * const ERR_PUT_NO_SUCH_NODE  = "407";
static const char * const ERR_GET_NO_SUCH_NODE  = "408";
static const char * const ERR_GET               = "409";

/*
  escape character: \
  reserved words:   words in small letters, and numbers
  variables:        words in capital letters

  new,NODE_NAME,PORT_NUMBER |
  new,NODE_NAME,PORT_NUMBER,global
  -> 200,new,NODE_NAME,PORT_NUMBER |
     400 | 401,COMMENT |
     402 | 403,new,NODE_NAME,PORT_NUMBER,COMMENT

  delete,NODE_NAME
  -> 201,delete,NODE_NAME |
     404,delete,NODE_NAME,COMMENT

  set_id,NODE_NAME,IDNETIFIER
  -> 205,set_id,NODE_NAME,IDENTIFIER |
     400 | 401,COMMENT

  join,NODE_NAME,HOST,PORT
  -> 202,join,NODE_NAME,HOST,PORT
     400 | 401,COMMENT
     405 | 406,join,NODE_NAME,HOST,PORT,COMMENT

  put,NODE_NAME,KEY,VALUE,TTL | 
  put,NODE_NAME,KEY,VALUE,TTL,unique
  -> 203,put,NODE_NAME,KEY,VALUE,TTL |
     400 | 401,COMMENT |
     407,put,NODE_NAME,KEY,VALUE,TTL,COMMENT

  get,NODE_NAME,key
  -> 204,get,NODE_NAME,KEY,VALUE1,VALUE2,VALUE3,... |
     400 | 401,COMMENT |
     408,get,KEY,COMMENT |
     409,get,KEY
 */

int
main(int argc, char *argv[])
{
        int   opt;
        int   port = 12080;
        bool  is_daemon = false;
        pid_t pid;

	while ((opt = getopt(argc, argv, "dhp:")) != -1) {
		switch (opt) {
                case 'h':
                        usage(argv[0]);
                        return 0;
		case 'd':
                        is_daemon = true;
                        break;
                case 'p':
                        port = atoi(optarg);
                        break;
                }
        }

        if (is_daemon) {
                if ((pid = fork()) < 0) {
                        return -1;
                } else {
                        exit(0);
                }

                setsid();
                chdir("/");
                umask(0);
        }

        event_init();

        if (start_listen(port) == false) {
                std::cerr << "can't listen port: " << port << std::endl;
                return -1;
        }

        event_dispatch();

        return 0;
}

void
usage(char *cmd)
{
        printf("%s [-d] [-p port]\n", cmd);
        printf("    -d: run as daemon\n");
        printf("    -h: show this help\n");
        printf("    -p: the port number to listen, default value is 12080\n");
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
        } else if (*it == "set_id") {
                D(std::cout << "process set_id" << std::endl;);
                process_set_id(sockfd, ++it, tokens.end());
        } else if (*it == "join") {
                D(std::cout << "process join" << std::endl;);
                process_join(sockfd, ++it, tokens.end());
        } else if (*it == "put") {
                D(std::cout << "process put" << std::endl);
                process_put(sockfd, ++it, tokens.end());
        } else if (*it == "get") {
                D(std::cout << "process get" << std::endl);
                process_get(sockfd, ++it, tokens.end());
        } else if (*it == "dump") {
                D(std::cout << "process dump" << std::endl);
                process_dump(sockfd, ++it, tokens.end());
        } else {
                D(std::cout << "unknown command: " << *it << std::endl);

                char result[1024];

                // format: 400,COMMENT
                snprintf(result, sizeof(result), 
                         "400,unknown command. cannot recognize '%s'\n",
                         it->c_str());
                send(sockfd, result, strlen(result), 0);
        }
}

void
process_set_id(int sockfd, esc_tokenizer::iterator &it,
               const esc_tokenizer::iterator &end)
{
        name2node_type::iterator it_n2n;
        std::string node_name;
        std::string esc_node_name;
        char        result[1024 * 4];

        // read node_name
        if (it == end || it->length() == 0) {
                // there is no port number
                // format: 401,COMMENT
                snprintf(result, sizeof(result),
                         "401,node name is required\n");
                send(sockfd, result, strlen(result), 0);
                return;
        }

        node_name = *it;
        esc_node_name = *it;
        replace(esc_node_name, ",", "\\,");

        it_n2n = name2node.find(node_name);
        if (it_n2n == name2node.end()) {
                // invalid node name
                // format: 404,delete,NODE_NAME,COMMENT
                snprintf(result, sizeof(result),
                         "%s,delete,%s,no such node named '%s'\n",
                         ERR_DEL_NO_SUCH_NODE, esc_node_name.c_str(),
                         esc_node_name.c_str());
                send(sockfd, result, strlen(result), 0);
                return;
        }

        ++it;


        // read port number
        if (it == end) {
                // there is no identifier
                // format: 401,COMMENT
                snprintf(result, sizeof(result),
                         "401,identifier is required\n");
                send(sockfd, result, strlen(result), 0);
                return;
        }

        it_n2n->second->set_id(it->c_str(), it->size());

        // format: 205,set_id,NODE_NAME,IDENTIFIER
        snprintf(result, sizeof(result), "205,set_id,%s,%s\n",
                 esc_node_name.c_str(), it->c_str());
        send(sockfd, result, strlen(result), 0);
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
                // format: 401,COMMENT
                snprintf(result, sizeof(result),
                         "401,node name is required\n");
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
                // format: 401,COMMENT
                snprintf(result, sizeof(result),
                         "401,port number is required\n");
                send(sockfd, result, strlen(result), 0);
                return;
        }

        try {
                port = boost::lexical_cast<int>(*it);
        } catch (boost::bad_lexical_cast) {
                // no integer
                // format: 401,COMMENT
                std::string esc_port = *it;
                replace(esc_port, ",", "\\,");
                snprintf(result, sizeof(result),
                         "401,'%s' is not a valid number\n",
                         esc_port.c_str());
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
                // format: 403,new,NODE_NAME,PORT_NUMBER,COMMENT
                snprintf(result, sizeof(result),
                         "%s,new,%s,%d,the node name '%s' exists already\n",
                         ERR_ALREADY_EXIST, esc_node_name.c_str(), port,
                         esc_node_name.c_str());
                send(sockfd, result, strlen(result), 0);
                return;
        }

        boost::shared_ptr<libcage::cage> c(new libcage::cage);

        // open port
        if (c->open(PF_INET, port) == false) {
                // cannot open port
                // format: 402,new,NODE_NAME,PORT_NUMBER,COMMENT
                snprintf(result, sizeof(result),
                         "%s,new,%s,%d,cannot open port(%d)\n",
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
        // format: 200,new,NODE_NAME,PORT_NUMBER
        snprintf(result, sizeof(result), "%s,new,%s,%d\n", 
                 SUCCEEDED_NEW, esc_node_name.c_str(), port);

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
                // format: 401,COMMENT
                snprintf(result, sizeof(result),
                         "401,node name is required\n");
                send(sockfd, result, strlen(result), 0);
                return;
        }

        node_name = *it;
        esc_node_name = *it;
        replace(esc_node_name, ",", "\\,");

        it_n2n = name2node.find(node_name);
        if (it_n2n == name2node.end()) {
                // invalid node name
                // format: 404,delete,NODE_NAME,COMMENT
                snprintf(result, sizeof(result),
                         "%s,delete,%s,no such node named '%s'\n",
                         ERR_DEL_NO_SUCH_NODE, esc_node_name.c_str(),
                         esc_node_name.c_str());
                send(sockfd, result, strlen(result), 0);
                return;
        }

        D(std::cout << "    node_name: " << node_name);

        name2node.erase(it_n2n);

        // send result
        // format: 201,delete,NODE_NAME
        snprintf(result, sizeof(result),
                 "%s,delete,%s\n",
                 SUCCEEDED_DELETE, esc_node_name.c_str());
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
                        return;
                }

                if (is_join) {
                        // send result of the success
                        // format: 202,join,NODE_NAME,HOST,PORT
                        snprintf(result, sizeof(result),
                                 "%s,join,%s,%s,%d\n",
                                 SUCCEEDED_JOIN, esc_node_name.c_str(),
                                 esc_host.c_str(), port);
                        send(sockfd, result, strlen(result), 0);
                } else {
                        // send result of the fail
                        // format: 406,join,NODE_NAME,HOST,PORT,COMMENT
                        snprintf(result, sizeof(result),
                                 "%s,join,%s,%s,%d,failed in connecting to '%s:%d'\n",
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
                // format: 401,COMMENT
                snprintf(result, sizeof(result),
                         "401,node name is required\n");
                send(sockfd, result, strlen(result), 0);
                return;
        }

        node_name = *it;
        esc_node_name = node_name;
        replace(esc_node_name, ",", "\\,");

        ++it;
        if (it == end) {
                // there is no host
                // format: 401,COMMENT
                snprintf(result, sizeof(result),
                         "401,host is required\n");
                send(sockfd, result, strlen(result), 0);
                return;
        }

        host = *it;
        esc_host = host;
        replace(esc_host, ",", "\\,");

        ++it;
        if (it == end) {
                // there is no port number
                // format: 401,COMMENT
                snprintf(result, sizeof(result),
                         "401,port number is required\n");
                send(sockfd, result, strlen(result), 0);
                return;
        }


        try {
                port = boost::lexical_cast<int>(*it);
        } catch (boost::bad_lexical_cast) {
                // no integer
                // format: 401,COMMENT
                std::string esc_port = *it;
                replace(esc_port, ",", "\\,");
                snprintf(result, sizeof(result),
                         "401,'%s' is not a valid number\n",
                         esc_port.c_str());
                send(sockfd, result, strlen(result), 0);
                return;
        }

        it_n2n = name2node.find(node_name);
        if (it_n2n == name2node.end()) {
                // invalid node name
                // format: 405,join,NODE_NAME,HOST,PORT,COMMENT
                snprintf(result, sizeof(result),
                         "%s,join,%s,%s,%d,no such node named '%s'\n",
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
        std::string esc_node_name;
        std::string key, value;
        std::string esc_key, esc_value;
        uint16_t    ttl;
        bool        is_unique = false;
        char        result[1024 * 4];

        if (it == end) {
                // there is no node_name
                // format: result,401,COMMENT
                snprintf(result, sizeof(result),
                         "401,node name is required\n");
                send(sockfd, result, strlen(result), 0);
                return;
        }

        node_name = *it;
        esc_node_name = node_name;
        replace(esc_node_name, ",", "\\,");


        ++it;
        if (it == end) {
                // there is no key
                // format: 401,COMMENT
                snprintf(result, sizeof(result),
                         "401,key is required\n");
                send(sockfd, result, strlen(result), 0);
                return;
        }

        key = *it;
        esc_key = key;
        replace(esc_key, ",", "\\,");


        ++it;
        if (it == end) {
                // there is no value
                // format: result,401,COMMENT
                snprintf(result, sizeof(result),
                         "401,value is required\n");
                send(sockfd, result, strlen(result), 0);
                return;
        }

        value = *it;
        esc_value = value;
        replace(esc_value, ",", "\\,");


        ++it;
        if (it == end) {
                // there is no ttl
                // format: 401,COMMENT
                snprintf(result, sizeof(result),
                         "401,ttl is required\n");
                send(sockfd, result, strlen(result), 0);
                return;
        }


        try {
                ttl = boost::lexical_cast<uint16_t>(*it);
        } catch (boost::bad_lexical_cast) {
                // no integer
                // format: 401,COMMENT
                std::string esc_ttl = *it;
                replace(esc_ttl, ",", "\\,");
                snprintf(result, sizeof(result),
                         "401,'%s' is not a valid number\n",
                         esc_ttl.c_str());
                send(sockfd, result, strlen(result), 0);
                return;
        }

        ++it;
        if (it != end && *it == "unique")
                is_unique = true;


        name2node_type::iterator it_n2n;
        it_n2n = name2node.find(node_name);
        if (it_n2n == name2node.end()) {
                // invalid node name
                // format: 407,put,NODE_NAME,KEY,VALUE,TTL,COMMENT
                snprintf(result, sizeof(result),
                         "%s,put,%s,%s,%s,%d,no such node named '%s'\n",
                         ERR_PUT_NO_SUCH_NODE, esc_node_name.c_str(),
                         esc_key.c_str(), esc_value.c_str(), ttl,
                         esc_node_name.c_str());
                send(sockfd, result, strlen(result), 0);
                return;
        }

        it_n2n->second->put(key.c_str(), key.length(),
                            value.c_str(), value.length(), ttl, is_unique);

        D(std::cout << "    node_name: " << node_name
                    << "\n    key: " << key
                    << "\n    value: " << value
                    << "\n    TTL: " << ttl
                    << std::endl);


        // send result of the success
        if (is_unique) {
                // format: 203,put,NODE_NAME,KEY,VALUE,TTL,unique
                snprintf(result, sizeof(result),
                         "%s,put,%s,%s,%s,%d,unique\n",
                         SUCCEEDED_PUT, esc_node_name.c_str(),
                         esc_key.c_str(), esc_value.c_str(), ttl);
        } else {
                // format: 203,put,NODE_NAME,KEY,VALUE,TTL
                snprintf(result, sizeof(result),
                         "%s,put,%s,%s,%s,%d\n",
                         SUCCEEDED_PUT, esc_node_name.c_str(),
                         esc_key.c_str(), esc_value.c_str(), ttl);
        }

        send(sockfd, result, strlen(result), 0);
}

class func_get
{
public:
        std::string esc_node_name;
        std::string esc_key;
        int         sockfd;

        void operator() (bool is_get, libcage::dht::value_set_ptr vset)
        {
                sock2ev_type::iterator it;
                char                   result[1024 * 1024];

                it = sock2ev.find(sockfd);
                if (it == sock2ev.end()) {
                        return;
                }

                if (is_get) {
                        std::string values;
                        libcage::dht::value_set::iterator it;

                        for (it = vset->begin(); it != vset->end(); ++it) {
                                boost::shared_array<char> tmp(new char[it->len + 1]);
                                std::string value;

                                memcpy(tmp.get(), it->value.get(), it->len);

                                tmp[it->len] = '\0';

                                value = tmp.get();
                                replace(value, ",", "\\,");

                                values += ",";
                                values += value;
                        }

                        // send value
                        // format: 204,get,NODE_NAME,KEY,VALUE
                        snprintf(result, sizeof(result),
                                 "%s,get,%s,%s%s\n",
                                 SUCCEEDED_GET, esc_node_name.c_str(),
                                 esc_key.c_str(), values.c_str());

                        send(sockfd, result, strlen(result), 0);
                        return;
                } else {
                        // send result of fail
                        // format: 409,get,NODE_NAME,KEY
                        snprintf(result, sizeof(result),
                                 "%s,get,%s,%s\n",
                                 ERR_GET, esc_node_name.c_str(),
                                 esc_key.c_str());
                        send(sockfd, result, strlen(result), 0);
                }
        }
};

void process_get(int sockfd, esc_tokenizer::iterator &it,
                 const esc_tokenizer::iterator &end)
{
        std::string node_name;
        std::string esc_node_name;
        std::string key;
        std::string esc_key;
        char        result[1024 * 4];
        func_get    func;

        if (it == end) {
                // there is no node_name
                // format: 401,COMMENT
                snprintf(result, sizeof(result),
                         "401,node name is required\n");
                send(sockfd, result, strlen(result), 0);
                return;
        }

        node_name = *it;
        esc_node_name = node_name;
        replace(esc_node_name, ",", "\\,");


        ++it;
        if (it == end) {
                // there is no key
                // format: 401,COMMENT
                snprintf(result, sizeof(result),
                         "401,key is required\n");
                send(sockfd, result, strlen(result), 0);
                return;
        }

        key = *it;
        esc_key = key;
        replace(esc_key, ",", "\\,");


        name2node_type::iterator it_n2n;
        it_n2n = name2node.find(node_name);
        if (it_n2n == name2node.end()) {
                // invalid node name
                // format: 408,NODE_NAME,get,KEY,COMMENT
                snprintf(result, sizeof(result),
                         "%s,get,%s,%s,no such node named '%s'\n",
                         ERR_GET_NO_SUCH_NODE, esc_node_name.c_str(),
                         esc_key.c_str(), esc_node_name.c_str());
                send(sockfd, result, strlen(result), 0);
                return;
        }

        D(std::cout << "    node_name: " << node_name
                    << "\n    key: " << key
                    << std::endl);

        func.esc_node_name = esc_node_name;
        func.esc_key       = esc_key;
        func.sockfd        = sockfd;
        it_n2n->second->get(key.c_str(), key.length(), func);
}

// for debug
void process_dump(int sockfd, esc_tokenizer::iterator &it,
                  const esc_tokenizer::iterator &end)
{
        name2node_type::iterator it_n;

        for (it_n = name2node.begin(); it_n != name2node.end(); ++it_n) {
                printf("%s:\n", it_n->first.c_str());
                it_n->second->print_state();
                printf("\n");
        }
}
