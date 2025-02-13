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
    int ipv6_sock = *(int *)arg;
    char buffer[BUFFER_SIZE];
    
    // Read data from the IPv6 socket
    ssize_t n = read(ipv6_sock, buffer, sizeof(buffer));
    if (n > 0) {
        // Iterate over all port forwarding rules
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

            // Connect to the IPv4 address and port
            if (connect(ipv4_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                perror("Failed to connect to IPv4 address");
                close(ipv4_sock);
                continue;
            }

            // Forward the data
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
        if (line[0] == '#') continue; // Ignore comment lines
        if (strncmp(line, "config port", 11) == 0) {
            int ipv6_port = 0;
            char ipv4_address[INET_ADDRSTRLEN];
            int ipv4_port = 0;
            // Read port forwarding config
            while (fgets(line, sizeof(line), file)) {
                if (strncmp(line, "option ipv6_port", 16) == 0) {
                    sscanf(line, "option ipv6_port '%d'", &ipv6_port);
                } else if (strncmp(line, "option ipv4_address", 20) == 0) {
                    sscanf(line, "option ipv4_address '%s'", ipv4_address);
                } else if (strncmp(line, "option ipv4_port", 16) == 0) {
                    sscanf(line, "option ipv4_port '%d'", &ipv4_port);
                } else if (strncmp(line, "config port", 11) == 0) {
                    break; // Moving to next config block
                }
            }
            // Call to add port forwarding
            add_port_forward(ipv6_port, ipv4_address, ipv4_port);
        }
    }
    fclose(file);
}

int main() {
    read_config();

    // Create IPv6 listening socket
    int ipv6_sock = socket(AF_INET6, SOCK_STREAM, 0);
    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any; // Listening on all interfaces

    for (int i = 0; i < num_ports; i++) {
        addr.sin6_port = htons(ports[i].ipv6_port);

        if (bind(ipv6_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("Failed to bind IPv6 socket");
            return EXIT_FAILURE;
        }

        if (listen(ipv6_sock, 5) < 0) {
            perror("Failed to listen on IPv6 socket");
            return EXIT_FAILURE;
        }
    }

    while (1) {
        int client_sock = accept(ipv6_sock, NULL, NULL);
        if (client_sock >= 0) {
            pthread_t thread;
            pthread_create(&thread, NULL, handle_client, &client_sock);
            pthread_detach(thread); // Detach the thread to avoid memory leak
        }
    }

    close(ipv6_sock);
    return 0;
}