/* Common file for server & client */

#ifndef TFTP_H
#define TFTP_H

#include <stdint.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#define PORT 6969
#define BUFFER_SIZE 516  // TFTP data packet size (512 bytes data + 4 bytes header)
#define TIMEOUT_SEC 5    // Timeout in seconds

// Mode constants
#define MODE_NORMAL   1   // 512-byte blocks, raw
#define MODE_OCTET    2   // 1-byte blocks
#define MODE_NETASCII 3   // 512-byte blocks, CRLF translation

// TFTP OpCodes
typedef enum {
    RRQ = 1,  // Read Request
    WRQ = 2,  // Write Request
    DATA = 3, // Data Packet
    ACK = 4,  // Acknowledgment
    ERROR = 5 // Error Packet
} tftp_opcode;

// TFTP Packet Structure
typedef struct {
    uint16_t opcode; // Operation code (RRQ/WRQ/DATA/ACK/ERROR)
    union {
        struct {
            char filename[256];
            char mode[8];  // Typically "octet", "netascii", etc.
        } request;  // RRQ and WRQ
        struct {
            uint16_t block_number;
            char data[512];
        } data_packet; // DATA
        struct {
            uint16_t block_number;
        } ack_packet; // ACK
        struct {
            uint16_t error_code;
            char error_msg[512];
        } error_packet; // ERROR
    } body;
} __attribute__((packed)) tftp_packet;

// Function prototypes
void send_file(int sockfd, struct sockaddr_in peer_addr, socklen_t peer_len, char *filename);
void receive_file(int sockfd, struct sockaddr_in peer_addr, socklen_t peer_len, char *filename);

#endif // TFTP_H
