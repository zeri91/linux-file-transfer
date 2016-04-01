#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char const *argv[])
{
	char *ip = "127.0.0.1";
	char *port = "2880";
	char *rootdir = "./";
	int   ratelimit = 0;    // unlimited
    int   timescale = 1000; // 1000ms = 1s
    char  buf[BUFFERSIZE];
    
    if(argc < 5)
    {
        fprintf(stderr, "usage: %s [-l ip] [-p port] [-d rootdir] [-r ratelimit] [-t timescale]\n", argv[0]);
        return 1;
    }

    return 0;
}
