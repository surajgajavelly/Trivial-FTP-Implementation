#include "tftp.h"

extern int mode; // From your main files

void send_file(int sockfd, struct sockaddr_in addr, socklen_t addr_len, char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) { perror("open"); return; }

    tftp_packet data_pkt, ack_pkt;
    uint16_t current_block = 1;
    int bytes_read;

    while (1) {
        memset(&data_pkt, 0, sizeof(data_pkt));
        data_pkt.opcode = htons(DATA);
        data_pkt.body.data_packet.block_number = htons(current_block);

        if (mode == MODE_NORMAL) {
            bytes_read = read(fd, data_pkt.body.data_packet.data, 512);
        }
        else if (mode == MODE_OCTET) {
            bytes_read = read(fd, data_pkt.body.data_packet.data, 1);
        }
        else if (mode == MODE_NETASCII) {
            char buff[512]; int i = 0; char ch;
            while (i < 512 && read(fd, &ch, 1) == 1) {
                if (ch == '\n') buff[i++] = '\r';
                buff[i++] = ch;
            }
            memcpy(data_pkt.body.data_packet.data, buff, i);
            bytes_read = i;
        }

        if (bytes_read < 0) { perror("read"); break; }
        if (bytes_read == 0) { printf("[DEBUG] EOF reached.\n"); break; }

        printf("[DEBUG] Sending Block: %d | Bytes: %d\n", current_block, bytes_read);

        if (sendto(sockfd, &data_pkt, 4 + bytes_read, 0, (struct sockaddr *)&addr, addr_len) < 0) {
            perror("sendto"); break;
        }

        int n = recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&addr, &addr_len);
        if (n < 0) { perror("recvfrom"); break; }

        if (ntohs(ack_pkt.opcode) == ACK &&
            ntohs(ack_pkt.body.ack_packet.block_number) == current_block) {
            current_block++;
        } else {
            printf("Timeout or ACK Error on block %d. Aborting.\n", current_block);
            break;
        }

        // End of file conditions
        if (mode == MODE_OCTET && bytes_read < 1) { printf("[DEBUG] EOF in Octet mode.\n"); break; }
        if (mode == MODE_NORMAL && bytes_read < 512) { printf("[DEBUG] Short block in Normal mode.\n"); break; }
        if (mode == MODE_NETASCII && bytes_read < 512) { printf("[DEBUG] Short block in Netascii mode.\n"); break; }
    }

    close(fd);
    printf("Send Complete.\n");
}

void receive_file(int sockfd, struct sockaddr_in addr, socklen_t addr_len, char *filename) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (fd < 0) { perror("open"); return; }

    tftp_packet data_pkt, ack_pkt;
    int bytes_received;

    while (1) {
        memset(&data_pkt, 0, sizeof(data_pkt));
        bytes_received = recvfrom(sockfd, &data_pkt, sizeof(data_pkt), 0,
                                  (struct sockaddr *)&addr, &addr_len);
        if (bytes_received < 0) { perror("recvfrom"); break; }

        if (ntohs(data_pkt.opcode) == DATA) {
            int data_len = bytes_received - 4;
            printf("[DEBUG] Received Block: %d | Bytes: %d\n",
                   ntohs(data_pkt.body.data_packet.block_number), data_len);

            if (data_len > 0) {
                if (mode == MODE_NORMAL || mode == MODE_OCTET) {
                    if (write(fd, data_pkt.body.data_packet.data, data_len) < 0) {
                        perror("write"); break;
                    }
                }
                else if (mode == MODE_NETASCII) {
                    char buff[512]; int j = 0;
                    for (int i = 0; i < data_len; i++) {
                        if (data_pkt.body.data_packet.data[i] == '\r' &&
                            i + 1 < data_len &&
                            data_pkt.body.data_packet.data[i+1] == '\n') {
                            buff[j++] = '\n'; i++;
                        } else {
                            buff[j++] = data_pkt.body.data_packet.data[i];
                        }
                    }
                    if (write(fd, buff, j) < 0) {
                        perror("write"); break;
                    }
                }
            }

            // Send ACK
            ack_pkt.opcode = htons(ACK);
            ack_pkt.body.ack_packet.block_number = data_pkt.body.data_packet.block_number;
            if (sendto(sockfd, &ack_pkt, 4, 0, (struct sockaddr *)&addr, addr_len) < 0) {
                perror("sendto"); break;
            }

            // End of file conditions
            if (data_len == 0) { printf("[DEBUG] EOF reached.\n"); break; }
            if (mode == MODE_OCTET && data_len < 1) { printf("[DEBUG] EOF in Octet mode.\n"); break; }
            if (mode == MODE_NORMAL && data_len < 512) { printf("[DEBUG] Short block in Normal mode.\n"); break; }
            if (mode == MODE_NETASCII && data_len < 512) { printf("[DEBUG] Short block in Netascii mode.\n"); break; }
        }
    }

    close(fd);
    printf("Receive Complete.\n");
}
