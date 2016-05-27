// client
// faraz

#include <arpa/inet.h>
#include <ctype.h>
#include <epoll.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXEVENTS  4
#define BUFFERSIZE 64 * 1024
#define CRLF "\r\n"
#define CLIENT_HEADER "FARAZ-FTP-client-v0.1" CRLF
#define SERVER_HEADER "FARAZ-FTP-server"

void print_addrinfo(struct addrinfo *input)
{
    int i = 0;
    for(struct addrinfo *p = input; p != NULL; p = p->ai_next)
    {
        char *ipver;
        void *addr;

        // get the pointer to the address itself, different fields in IPv4 and IPv6
        if (p->ai_family == AF_INET)
        {
            ipver = "IPv4";
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
        }
        else
        {
            ipver = "IPv6";
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
        }

        char ipstr[INET6_ADDRSTRLEN];
        inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr)); // converts the IP to a string
        printf("%2d. %s: %s\n", ++i, ipver, ipstr);
    }
}

void readline(int sockfd, char *buf, int size)
{
    char c;
    int pos = 0;
    while(pos < size - 1)
    {
        int n = recv(sockfd, &c, 1, 0);
        if(n < 1) break;
        buf[pos++] = c;
        if(c == '\n') break;
    }
    buf[pos] = '\0';
}

int main(int argc, char const *argv[])
{
    char *server = NULL;
    char *filename = NULL;
    char *port = "2880";
    char *outfile = NULL;
    int   ratelimit = 0;    // unlimited
    int   timescale = 1000; // 1000ms = 1s
    char  buf[BUFFERSIZE];

    if(argc < 5)
    {
        fprintf(stderr, "usage: %s -s server -f filename [-p port] [-o outfile] [-r ratelimit] [-t timescale]\n", argv[0]);
        return 1;
    }
    
    char  c;
    while((c = getopt (argc, argv, "s:f:p:o:r:t:")) != -1)
    {
        switch(c)
        {
            case 's':
                server = optarg;
                break;
            case 'f':
                filename = optarg;
                break;
            case 'p':
                port = optarg;
                break;
            case 'o':
                outfile = optarg;
                break;
            case 'r':
                ratelimit = atoi(optarg);
                break;
            case 't':
                timescale = atoi(optarg);
                break;
            case '?':
                fprintf(stderr, "Unknown option '-%c'.\n", optopt);
                return 1;
            default:
                abort();
        }
    }

    if(server == NULL)
    {
        fprintf(stderr, "-s server\n");
        return 2;
    }

    if(filename == NULL)
    {
        fprintf(stderr, "-f filename\n");
        return 2;
    }

    printf("Looking up addresses for %s ...\n", server);
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *dnsres;
    int getaddrinfo_r = getaddrinfo(server, port, &hints, &dnsres);
    if(getaddrinfo_r != 0)
    {
        fprintf(stderr, "dns lookup failed: %s\n", gai_strerror(getaddrinfo_r));
        return 3;
    }

    print_addrinfo(dnsres);

    printf("Connecting to %s ...\n", "the server");
    int sockfd = socket(dnsres->ai_family, dnsres->ai_socktype, dnsres->ai_protocol);
    if(connect(sockfd, dnsres->ai_addr, dnsres->ai_addrlen) != 0)
    {
        perror("connect");
        return 4;
    }
    printf("Connected.\n");

    freeaddrinfo(dnsres); // frees the memory that was dynamically allocated for the linked lists by getaddrinfo
    
    int efd = epoll_create1(0);
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
    event.data.fd = sockfd;
    epoll_ctl(efd, EPOLL_CTL_ADD, sockfd, &event);

    send(sockfd, CLIENT_HEADER, strlen(CLIENT_HEADER), 0);
    send(sockfd, filename, strlen(filename), 0);
    send(sockfd, CRLF, 2, 0);

    readline(sockfd, buf, BUFFERSIZE);
    printf("Received: %s", buf);
    buf[strlen(SERVER_HEADER)] = 0;
    if(strcmp(buf, SERVER_HEADER) != 0)
    {
        fprintf(stderr, "Unknown server\n");
        return 5;
    }
    
    readline(sockfd, buf, BUFFERSIZE);
    printf("Received: %s", buf);
    
    FILE * fp;
    fp = fopen(outfile == NULL ? filename : outfile, "w");
    
    struct epoll_event events[MAXEVENTS];
    for(;;)
    {
        int done = 0;
        int nfd = epoll_wait(efd, events, MAXEVENTS, -1);
        for(int i = 0; i < nfd; ++i)
        {
            if(events[i].data.fd == sockfd)
            {
                fprintf(stderr, "events: %d \n", events[i].events);
                if((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN)))
                {
                    fprintf(stderr, "EPOLLERR || EPOLLHUP \n");
                    done = 1;
                }
                else
                {
                    size_t rbytes = recv(sockfd, buf, BUFFERSIZE, 0);
                    size_t wbytes = fwrite(buf, rbytes, 1, fp);
                    if(rbytes != wbytes) fprintf(stderr, "rbytes != wbytes \n");
                }
            }
        }
        if(done) break;
    }

    close(sockfd);
    fclose(fp);

    return 0;
}
