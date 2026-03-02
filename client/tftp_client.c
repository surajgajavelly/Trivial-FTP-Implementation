#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "tftp.h"
#include "tftp_client.h"

int mode = 1;
int pack_num = 1;

void read_input(char *buffer, size_t size)
{
    if (fgets(buffer, size, stdin) != NULL)
    {
        buffer[strcspn(buffer, "\n")] = 0;  // remove newline
    }
}
 
int main() {
    char command[256];
    tftp_client_t client;
    memset(&client, 0, sizeof(client));  // Initialize client structure

    printf("========================================\n");
    printf("        TFTP INTERACTIVE CLIENT         \n");
    printf("========================================\n");

    // Main loop for command-line interface
    while (1) {
        printf("tftp> ");
        printf("=============| MENU |=============\n");
        printf("1. Connect\n2. Put(READ From Client and WRITE in Server)\n3. Get(READ from Server and WRITE in Client)\n4. Mode\n5. Exit\nEnter the Command: ");

        read_input(command, sizeof(command));

        // Process the command
        process_command(&client, command);
    }

    return 0;
}

// Function to process commands
void process_command(tftp_client_t *client, char *command) 
{
    if (strcmp(command, "connect") == 0) 
    {
        printf("Enter server IP: ");
        read_input(client->server_ip, sizeof(client->server_ip));

        if (validate_server_ip(client->server_ip) == FAILURE)
        {
            printf("Invalid IP address format. Please try again.\n");
            return;
        }

        printf("===========| IP address is valid |===========\n");

        connect_to_server(client, client->server_ip, PORT);
    } 
    else if (strcmp(command, "put") == 0) 
    {
        char filename[256];

        printf("========================================\n");
        printf("Available files in current directory:\n");
        printf("========================================\n");
        system("ls");
        printf("========================================\n");
        printf("Enter the file name: ");
        read_input(filename, sizeof(filename));
        put_file(client, filename);
    }
    else if (strcmp(command, "get") == 0)
    {
        printf("Enter the file name: ");
        char filename[256];
        read_input(filename, sizeof(filename));
        get_file(client, filename);
    }
    else if (strcmp(command, "mode") == 0)
    {
        printf("1.Normal mode\n2.Octal mode\n3.Net ASCII\n");
        printf("Enter the mode :");
        scanf("%d", &mode);
        while (getchar() != '\n'); 

        if (mode == 1) printf(">>> Mode 1: Normal (512-byte blocks) selected successfully.\n");
        else if (mode == 2) printf(">>> Mode 2: Octet (1-byte blocks) selected successfully.\n");
        else if (mode == 3) printf(">>> Mode 3: NetASCII (Text translation) selected successfully.\n");
        else printf("!!! Invalid mode selection. Defaulting to Normal.\n");
    }
    else if (strcmp(command, "exit") == 0)
    {
        printf("Disconnecting from server...\n");

        disconnect(client);

        printf("Exiting...\n");

        exit(0);
    }
    else
    {
        printf("Invalid command. Please try again.\n");
    }
   
}

// This function is to initialize socket with given server IP, no packets sent to server in this function
void connect_to_server(tftp_client_t *client, char *ip, int port) {
    // Create UDP socket
    client->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client->sockfd < 0) 
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&client->server_addr, 0, sizeof(client->server_addr));
    client->server_addr.sin_family = AF_INET;
    client->server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &client->server_addr.sin_addr) != 1)
    {
        printf("Invalid IP address.\n");
        close(client->sockfd);
        client->sockfd = -1;
        return;
    }

    client->server_len = sizeof(client->server_addr);


    printf("| Connected to TFTP server %s:%d |\n", ip, port);

}

void put_file(tftp_client_t *client, char *filename) 
{
    // Send WRQ request and send file
    int fd = open(filename, O_RDONLY);
    if (fd < 0) 
    {
        perror("Invalid file name\n");
        return;
    }

    close(fd);

    client->server_len = sizeof(client->server_addr);

    send_request(client->sockfd, client->server_addr, filename, WRQ);
}

void get_file(tftp_client_t *client, char *filename) 
{
    // Send RRQ and recive file 
    send_request(client->sockfd, client->server_addr, filename, RRQ);  
}

void disconnect(tftp_client_t *client) 
{
    // close fd
    close(client->sockfd);

    printf("Disconnected from server.\n");
   
}
void send_request(int sockfd, struct sockaddr_in server_addr, char *filename, uint16_t opcode)
{
    pack_num = 1;
    tftp_packet packet;
    memset(&packet, 0, sizeof(packet));

    packet.opcode = htons(opcode);
    strcpy(packet.body.request.filename, filename);
    
    if (mode == 3) {
        strcpy(packet.body.request.mode, "netascii");
    } else {
        strcpy(packet.body.request.mode, "octet");
    }

    sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));

    receive_request(sockfd, server_addr, filename, opcode);

}

void receive_request(int sockfd, struct sockaddr_in server_addr, char *filename, uint16_t opcode)
{
    tftp_packet packet;
    socklen_t addr_len = sizeof(server_addr);
    memset(&packet, 0, sizeof(packet));

    // PEEK allows us to check the opcode without removing Block 1 from the buffer
    int n = recvfrom(sockfd, &packet, sizeof(packet), MSG_PEEK, (struct sockaddr *)&server_addr, &addr_len);
    if (n < 0) {
        perror("recvfrom");
        return;
    }

    uint16_t op = ntohs(packet.opcode);

    // Scenario A: We are uploading (WRQ) -> Server MUST send ACK 0
    if (op == ACK && opcode == WRQ)
    {
        printf("READY TO SEND\n");
        // Remove the peeked ACK from the buffer so send_file starts fresh
        recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&server_addr, &addr_len);
        send_file(sockfd, server_addr, addr_len, filename);
    }
    // Scenario B: We are downloading (RRQ) -> Server MUST send DATA Block 1
    else if (op == DATA && opcode == RRQ)
    {
        printf("READY TO RECEIVE\n");
        // DO NOT recvfrom here! 
        // Let receive_file() do the first recvfrom() so it gets Block 1.
        receive_file(sockfd, server_addr, addr_len, filename);
    }
    else if (op == ERROR)
    {
        printf("Server Error: %s\n", packet.body.error_packet.error_msg);
    }
    else
    {
        printf("Unexpected packet received: %d\n", op);
    }
}

int validate_server_ip(char *ip_add)
{
    struct sockaddr_in sa;

    int result = inet_pton(AF_INET, ip_add, &(sa.sin_addr));

    if (result == 1)
    {
        return SUCCESS;
    } 
    else 
    {
        return FAILURE;
    }
}