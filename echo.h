#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>

inline void myerror(const char* msg) {
    fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno);
}

inline bool sendAll(int sd, const char* buf, ssize_t len) {
    ssize_t sent = 0;

    while (sent < len) {
        int flags = 0;

#ifdef MSG_NOSIGNAL
        flags = MSG_NOSIGNAL;
#endif

        ssize_t res = send(sd, buf + sent, len - sent, flags);
        if (res <= 0) return false;

        sent += res;
    }

    return true;
}
