#include "echo.h"

using namespace std;

struct Param {
    char* ip{nullptr};
    char* port{nullptr};

    bool parse(int argc, char* argv[]) {
        if (argc != 3) return false;

        ip = argv[1];
        port = argv[2];

        int p = atoi(port);
        if (p <= 0 || p > 65535) return false;

        return true;
    }
} param;

void recvThread(int sd) {
    printf("connected\n");
    fflush(stdout);

    static const int BUFSIZE = 65536;
    char buf[BUFSIZE];

    while (true) {
        ssize_t res = recv(sd, buf, BUFSIZE, 0);

        if (res == 0) {
            fprintf(stderr, "recv return 0\n");
            break;
        }

        if (res == -1) {
            myerror("recv");
            break;
        }

        fwrite(buf, 1, res, stdout);
        fflush(stdout);
    }

    printf("disconnected\n");
    fflush(stdout);

    close(sd);
    _exit(0);
}

int main(int argc, char* argv[]) {
    if (!param.parse(argc, argv)) {
        printf("syntax: echo-client <ip> <port>\n");
        printf("sample: echo-client 127.0.0.1 1234\n");
        return -1;
    }

    signal(SIGPIPE, SIG_IGN);

    struct addrinfo aiInput;
    struct addrinfo* aiOutput;
    struct addrinfo* ai;

    memset(&aiInput, 0, sizeof(aiInput));
    aiInput.ai_family = AF_INET;
    aiInput.ai_socktype = SOCK_STREAM;
    aiInput.ai_flags = 0;
    aiInput.ai_protocol = 0;

    int res = getaddrinfo(param.ip, param.port, &aiInput, &aiOutput);
    if (res != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(res));
        return -1;
    }

    int sd = -1;

    for (ai = aiOutput; ai != nullptr; ai = ai->ai_next) {
        sd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sd == -1) continue;

        if (connect(sd, ai->ai_addr, ai->ai_addrlen) == 0) break;

        myerror("connect");
        close(sd);
        sd = -1;
    }

    freeaddrinfo(aiOutput);

    if (sd == -1) {
        fprintf(stderr, "cannot connect to server\n");
        return -1;
    }

    thread(recvThread, sd).detach();

    while (true) {
        string s;

        if (!getline(cin, s)) break;

        s += "\r\n";

        if (!sendAll(sd, s.data(), s.size())) {
            myerror("send");
            break;
        }
    }

    shutdown(sd, SHUT_RDWR);
    close(sd);

    return 0;
}
