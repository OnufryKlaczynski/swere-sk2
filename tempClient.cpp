#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <error.h>
#include <iostream>

int main(int argc, char ** argv){
    if(argc!=3)
        error(1,0,"Usage: %s <ip> <port>", argv[0]);
    
    addrinfo hints {};
    hints.ai_family = AF_INET;
    hints.ai_protocol = IPPROTO_TCP;
    
    addrinfo *resolved;
    if(int err = getaddrinfo(argv[1], argv[2], &hints, &resolved))
        error(1, 0, "Resolving address failed: %s", gai_strerror(err));
    
    int sock = socket(resolved->ai_family, resolved->ai_socktype, resolved->ai_protocol);
    if(connect(sock, resolved->ai_addr, resolved->ai_addrlen))
        error(1,errno, "Failed to connect");
    freeaddrinfo(resolved);
    
    for(char letter = 'a'; letter <= 'z' ; ++letter){
        write(sock, &letter, 1);
    }
    
    
    close(sock);
    return 0;
}

