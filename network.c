#include "network.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>

static int server_fd;
static struct sockaddr_in address;
static int client_sockets[MAX_CLIENTS]; // Ŭ���̾�Ʈ ���� �迭

void init_network() {
    // 1. ���� ���� ��ũ���� ����
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // 2. ��Ʈ ���� �ɼ� ���� (���� ����)
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // 3. �ּ� ����ü ����
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(WIFI_SERVER_PORT);

    // 4. ������ �ּҿ� ���ε�
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // 5. ���� ��û ���
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    // Ŭ���̾�Ʈ ���� �迭 �ʱ�ȭ
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = 0;
    }

    printf(">>> Wi-Fi Server Initialized on port %d (Alerts Ready)\n", WIFI_SERVER_PORT);
}

// Ŭ���̾�Ʈ ���� ó���� ����ϴ� ������
void* wifiServerThreadFunc(void* arg) {
    int addrlen = sizeof(address);
    int new_socket;

    while (1) {
        printf(">>> Wi-Fi: Waiting for a client connection...\n");
        // Ŭ���̾�Ʈ ���� ����
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue; // ���� �߻� �� �ٽ� ���
        }

        // ����� Ŭ���̾�Ʈ�� ������ �迭�� ����
        int i;
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] == 0) {
                client_sockets[i] = new_socket;
                printf(">>> Wi-Fi: New client connected. Index: %d\n", i);
                break;
            }
        }

        // �ִ� Ŭ���̾�Ʈ �� �ʰ� �� ���� �ݱ�
        if (i == MAX_CLIENTS) {
            printf(">>> Wi-Fi: Max clients reached. Rejecting connection.\n");
            close(new_socket);
        }
    }
    // �� ������� ���α׷� ���� �ñ��� ��� �����
    return NULL;
}

// ��� �޽��� ���� �Լ�
void send_alert(int mode) {
    const char* message = NULL;

    if (mode == MODE_WARN) {
        message = "[WARN] Motion Detected!";
    }
    else if (mode == MODE_DANGER) {
        message = "[DANGER] Intrusion Detected!";
    }
    else {
        return; // �ٸ� ���� �˸� ����
    }

    // ��� ����� Ŭ���̾�Ʈ���� �޽��� ����
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] > 0) {
            if (send(client_sockets[i], message, strlen(message), 0) < 0) {
                perror("Send failed");
                // ���� ���� �� ���� �ݰ� �ʱ�ȭ
                close(client_sockets[i]);
                client_sockets[i] = 0;
            }
        }
    }
}
