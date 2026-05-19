#include "echo.h"

using namespace std;

struct Param {
    bool echo{false};
    bool broadcast{false};
    uint16_t port{0};

    bool parse(int argc, char* argv[]) {
        if (argc < 2) return false;

        int p = atoi(argv[1]);
        if (p <= 0 || p > 65535) return false;

        port = p;

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-e") == 0) {
                echo = true;
                continue;
            }

            if (strcmp(argv[i], "-b") == 0) {
                broadcast = true;
                continue;
            }

            return false;
        }

        return true;
    }
} param;

vector<int> clients;
mutex mtx;

int serverSd = -1;
volatile sig_atomic_t stopFlag = 0;

void addClient(int sd) {
    lock_guard<mutex> lock(mtx);
    clients.push_back(sd);
}

void removeClient(int sd) {
    lock_guard<mutex> lock(mtx);
    clients.erase(remove(clients.begin(), clients.end(), sd), clients.end());
}

vector<int> getClients() {
    lock_guard<mutex> lock(mtx);
    return clients;
}

void sigintHandler(int) {
    stopFlag = 1;

    if (serverSd != -1) {
        close(serverSd);
        serverSd = -1;
    }
}

void recvThread(int sd) {
    printf("connected\n");
    fflush(stdout);

    static const int BUFSIZE = 65536;
    char buf[BUFSIZE];

    while (!stopFlag) {
        ssize_t res = recv(sd, buf, BUFSIZE, 0);

        if (res == 0) {
            fprintf(stderr, "recv return 0\n");
            break;
        }

        if (res == -1) {
            if (!stopFlag) myerror("recv");
            break;
        }

        fwrite(buf, 1, res, stdout);
        fflush(stdout);

        if (param.echo) {
            if (!sendAll(sd, buf, res)) {
                myerror("send");
                break;
            }
        }

        if (param.broadcast)
            for (int c : getClients())
                if (!sendAll(c, buf, res))
                    shutdown(c, SHUT_RDWR);
    }

    removeClient(sd);
    close(sd);

    printf("disconnected\n");
    fflush(stdout);
}

int main(int argc, char* argv[]) {
    if (!param.parse(argc, argv)) {
        printf("syntax: echo-server <port> [-e] [-b]\n");
        printf("sample: echo-server 1234 -e -b\n");
        return -1;
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sigintHandler);

    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        myerror("socket");
        return -1;
    }

    serverSd = sd;

    int optval = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        myerror("setsockopt");  
        close(sd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(param.port);

    if (bind(sd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        myerror("bind");
        close(sd);
        return -1;
    }

    if (listen(sd, 5) == -1) {
        myerror("listen");
        close(sd);
        return -1;
    }

    printf("server started\n");
    fflush(stdout);

    while (!stopFlag) {
        struct sockaddr_in clientAddr;
        socklen_t len = sizeof(clientAddr);

        int newsd = accept(sd, (struct sockaddr*)&clientAddr, &len);
        if (newsd == -1) {
            if (stopFlag) break;

            myerror("accept");
            continue;
        }

        addClient(newsd);
        thread(recvThread, newsd).detach();
    }
    
    for (int c : getClients())
        shutdown(c, SHUT_RDWR);

    if (serverSd != -1) {
        close(serverSd);
        serverSd = -1;
    }

    printf("server closed\n");
    fflush(stdout);

    return 0;
}
