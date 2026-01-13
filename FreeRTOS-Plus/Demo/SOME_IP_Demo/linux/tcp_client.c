#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <signal.h>
#include "FreeRTOS_IP.h"

#define SERVER_IP   "10.0.0.1"
#define SERVER_PORT 5001
#define BUF_SIZE    512

int main(void)
{
    int sock;
    struct sockaddr_in addr;
    char buffer[BUF_SIZE];

    signal(SIGPIPE, SIG_IGN);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    printf("[LINUX] Connecting to FreeRTOS %s:%d...\n",
           SERVER_IP, SERVER_PORT);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    printf("[LINUX] Connected to FreeRTOS echo server\n");

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        int maxfd = (sock > STDIN_FILENO) ? sock : STDIN_FILENO;

        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        /* From FreeRTOS */
        if (FD_ISSET(sock, &readfds)) {
            int n = recv(sock, buffer, BUF_SIZE - 1, 0);
            if (n <= 0) {
                printf("[LINUX] FreeRTOS disconnected\n");
                break;
            }
            buffer[n] = '\0';
            printf("[LINUX RX] %s\n", buffer);
        }

        /* From terminal */
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            int n = read(STDIN_FILENO, buffer, BUF_SIZE - 1);
            if (n > 0) {
                send(sock, buffer, n, 0);
                printf("[LINUX TX] %.*s", n, buffer);
            }
        }
    }

    close(sock);
    return 0;
}
