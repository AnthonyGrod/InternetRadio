#include "err.h"
#include "common.h"

#include <iostream>


// to przy receiverze
int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {fatal("setsockopt");}
    opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {fatal("setsockopt");}

    struct addrinfo hint = {0}, *res;
    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo("255.255.255.255",  NULL, &hint, &res) != 0) {fatal("getaddrinfo");}

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(38477);
    addr.sin_addr = ((struct sockaddr_in *) res->ai_addr)->sin_addr;
    freeaddrinfo(res);

    while (1) {
        char message[20] = "ZERO_SEVEN_COME_IN\n";
        if (sendto(sock, message, 19, 0, (struct sockaddr *) &addr, sizeof(addr)) < 0) {fatal("sendto");}
        // std::cout << "sent" << std::endl;
        sleep(5);
    }
}