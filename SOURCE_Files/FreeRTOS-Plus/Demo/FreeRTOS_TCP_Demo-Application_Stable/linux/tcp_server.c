#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <signal.h>

#define SERVER_PORT 5001
#define BUF_SIZE 512

int main(void)
{
    int server_fd, client_fd;
    struct sockaddr_in addr;
    char buffer[BUF_SIZE];

    /* Prevent crash if client disconnects */
    signal(SIGPIPE, SIG_IGN);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen");
        return 1;
    }

    printf("[LINUX] TCP Server listening on port %d\n", SERVER_PORT);

    client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
        perror("accept");
        return 1;
    }

    printf("[LINUX] Client connected\n");
    printf("[LINUX] Type messages to send. Type 'quit' to close.\n");

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(client_fd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        int maxfd = (client_fd > STDIN_FILENO) ? client_fd : STDIN_FILENO;

        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        /* ---- Data from FreeRTOS client ---- */
        if (FD_ISSET(client_fd, &readfds)) {
            int n = recv(client_fd, buffer, BUF_SIZE - 1, 0);
            if (n == 0) {
                printf("[LINUX] Client disconnected\n");
                break;
            }
            if (n < 0) {
                perror("recv");
                break;
            }

            buffer[n] = '\0';
            printf("[LINUX RX] %s\n", buffer);
        }

        /* ---- Data from Linux terminal ---- */
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            int n = read(STDIN_FILENO, buffer, BUF_SIZE - 1);
            if (n > 0) {
                buffer[n] = '\0';

                if (strncmp(buffer, "quit", 4) == 0) {
                    printf("[LINUX] Closing connection\n");
                    break;
                }

                if (send(client_fd, buffer, n, 0) < 0) {
                    perror("send");
                    break;
                }

                printf("[LINUX TX] %s", buffer);
            }
            /* Ignore stdin EOF — keep TCP alive */
        }
    }

    close(client_fd);
    close(server_fd);
    return 0;
}
