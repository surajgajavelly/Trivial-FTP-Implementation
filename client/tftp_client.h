#ifndef TFTP_CLIENT_H
#define TFTP_CLIENT_H

#include "tftp.h"

typedef struct {
    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t server_len;
    char server_ip[INET_ADDRSTRLEN];
} tftp_client_t;

#define FAILURE -1
#define SUCCESS 0
// #define READ 65000
// #define WRITE 65001

// Function prototypes
void connect_to_server(tftp_client_t *client, char *ip, int port);
void put_file(tftp_client_t *client, char *filename);
void get_file(tftp_client_t *client, char *filename);
void disconnect(tftp_client_t *client);
void process_command(tftp_client_t *client, char *command);


void send_request(int sockfd, struct sockaddr_in server_addr, char *filename, uint16_t opcode);
void receive_request(int sockfd, struct sockaddr_in server_addr, char *filename, uint16_t opcode);
int validate_server_ip(char *ip_add);

#endif