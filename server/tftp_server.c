#include "tftp.h"
#include <signal.h>
#include <sys/wait.h>

int mode = MODE_NORMAL; // The global mode variable for the server process

void handle_client(struct sockaddr_in client_addr, tftp_packet *request_pkt) {
    int child_sockfd;
    socklen_t addr_len = sizeof(client_addr);
    
    // Create a new socket for the data transfer (TID)
    child_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (child_sockfd < 0) {
        perror("Child socket failed");
        exit(1);
    }

    // Bind to port 0 to let OS pick a random available port for this transfer
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = 0; 
    bind(child_sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr));

    // 1. DYNAMIC MODE DETECTION
    char *client_mode = request_pkt->body.request.mode;
    if (strcasecmp(client_mode, "octet") == 0) {
        mode = MODE_OCTET;
        printf("[INFO] Client requested OCTET mode.\n");
    } else if (strcasecmp(client_mode, "netascii") == 0) {
        mode = MODE_NETASCII;
        printf("[INFO] Client requested NETASCII mode.\n");
    } else {
        mode = MODE_NORMAL;
        printf("[INFO] Client requested NORMAL mode.\n");
    }

    uint16_t opcode = ntohs(request_pkt->opcode);
    char *filename = request_pkt->body.request.filename;

    if (opcode == RRQ) {
        printf("[RRQ] Sending %s to client...\n", filename);
        send_file(child_sockfd, client_addr, addr_len, filename);
    } 
    else if (opcode == WRQ) {
        printf("[WRQ] Receiving %s from client...\n", filename);
        
        // Initial ACK 0 for WRQ
        tftp_packet ack;
        ack.opcode = htons(ACK);
        ack.body.ack_packet.block_number = htons(0);
        sendto(child_sockfd, &ack, 4, 0, (struct sockaddr *)&client_addr, addr_len);
        
        receive_file(child_sockfd, client_addr, addr_len, filename);
    }

    close(child_sockfd);
    exit(0); // Child process finishes
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    tftp_packet request_pkt;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("Socket failed"); exit(1); }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    printf("========================================\n");
    printf("      SIMPLE TFTP SERVER RUNNING        \n");
    printf("========================================\n");
    printf("Listening on port %d...\n", PORT);

    // Clean up zombie processes
    signal(SIGCHLD, SIG_IGN);

    while (1) {
        memset(&request_pkt, 0, sizeof(request_pkt));
        int n = recvfrom(sockfd, &request_pkt, sizeof(request_pkt), 0, (struct sockaddr *)&client_addr, &addr_len);
        
        if (n > 0) {
            if (fork() == 0) {
                // Child process
                handle_client(client_addr, &request_pkt);
            }
            // Parent continues to listen on 6969
        }
    }

    close(sockfd);
    return 0;
}