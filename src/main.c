#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_PORTS 100
#define BUFFER_SIZE 2048
#define CONFIG_FILE "/etc/config/port_forward"

struct port_forward {
    int ipv6_port;
    char ipv4_address[INET_ADDRSTRLEN];
    int ipv4_port;
};

struct port_forward ports[MAX_PORTS];
int num_ports = 0;

void add_port_forward(int ipv6_port, char *ipv4_address, int ipv4_port) {
    if (num_ports < MAX_PORTS) {
        ports[num_ports].ipv6_port = ipv6_port;
        strncpy(ports[num_ports].ipv4_address, ipv4_address, INET_ADDRSTRLEN);
        ports[num_ports].ipv4_port = ipv4_port;
        num_ports++;
    }
}

void *handle_client(void *arg) {
    int ipv6_sock = *((int *)arg);
    free(arg); // 释放分配的内存
    char buffer[BUFFER_SIZE];
    
    ssize_t n = read(ipv6_sock, buffer, sizeof(buffer));
    if (n > 0) {
        for (int i = 0; i < num_ports; i++) {
            int ipv4_sock = socket(AF_INET, SOCK_STREAM, 0);
            if (ipv4_sock < 0) {
                perror("Failed to create IPv4 socket");
                continue;
            }

            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(ports[i].ipv4_port);
            inet_pton(AF_INET, ports[i].ipv4_address, &addr.sin_addr);

            if (connect(ipv4_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                perror("Failed to connect to IPv4 address");
                close(ipv4_sock);
                continue;
            }

            write(ipv4_sock, buffer, n);
            close(ipv4_sock);
        }
    }
    close(ipv6_sock);
    return NULL;
}

void read_config() {
    FILE *file = fopen(CONFIG_FILE, "r");
    if (!file) {
        perror("Failed to open config file");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || strlen(line) <= 1) continue; // Ignore comment lines and empty lines
        
        if (strncmp(line, "config port", 11) == 0) {
            int ipv6_port = 0;
            char ipv4_address[INET_ADDRSTRLEN] = "";
            int ipv4_port = 0;
            
            while (fgets(line, sizeof(line), file)) {
                if (sscanf(line, "option ipv6_port '%d'", &ipv6_port) == 1 ||
                    sscanf(line, "option ipv4_address '%s'", ipv4_address) == 1 ||
                    sscanf(line, "option ipv4_port '%d'", &ipv4_port) == 1) {
                    // Successfully parsed one of the options
                } else if (strncmp(line, "config port", 11) == 0) {
                    break; // Moving to next config block
                }
            }
            add_port_forward(ipv6_port, ipv4_address, ipv4_port);
        }
    }
    fclose(file);
}

int main() {
    read_config();

    for (int i = 0; i < num_ports; i++) {
        int ipv6_sock = socket(AF_INET6, SOCK_STREAM, 0);
        if (ipv6_sock < 0) {
            perror("Failed to create IPv6 socket");
            return EXIT_FAILURE;
        }

        struct sockaddr_in6 addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        addr.sin6_addr = in6addr_any;
        addr.sin6_port = htons(ports[i].ipv6_port);

        if (bind(ipv6_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("Failed to bind IPv6 socket");
            return EXIT_FAILURE;
        }

        if (listen(ipv6_sock, 5) < 0) {
            perror("Failed to listen on IPv6 socket");
            return EXIT_FAILURE;
        }

        printf("Listening on IPv6 port %d\n", ports[i].ipv6_port);

        while (1) {
            int *client_sock = malloc(sizeof(int)); // 分配内存并存储文件描述符
            *client_sock = accept(ipv6_sock, NULL, NULL);
            if (*client_sock >= 0) {
                pthread_t thread;
                if(pthread_create(&thread, NULL, handle_client, client_sock) != 0){
                    perror("Failed to create thread");
                    close(*client_sock);
                    free(client_sock);
                } else {
                    pthread_detach(thread); // Detach the thread to avoid memory leak
                }
            } else {
                free(client_sock); // 如果accept失败，记得释放内存
            }
        }

        close(ipv6_sock);
    }

    return 0;
}