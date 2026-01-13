#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_PORT 5001
#define BUF_SIZE 512

int main(void)
{
    int server_fd, client_fd;
    struct sockaddr_in addr;
    char buffer[BUF_SIZE];

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

    listen(server_fd, 1);
    printf("[LINUX] TCP Server listening on port %d\n", SERVER_PORT);

    client_fd = accept(server_fd, NULL, NULL);
    printf("[LINUX] Client connected\n");

    while (1) {
        int n = recv(client_fd, buffer, BUF_SIZE - 1, 0);
        if (n <= 0) break;

        buffer[n] = '\0';
        printf("[LINUX RX] %s\n", buffer);

        /* Echo back */
        send(client_fd, buffer, n, 0);
    }

    printf("[LINUX] Client disconnected\n");
    close(client_fd);
    close(server_fd);
    return 0;
}
