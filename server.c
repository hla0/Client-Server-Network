#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include "packet.c"

#define TIMEOUT 5000

void error(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int min(int a, int b) {
    return a < b ? a : b;
}

void clear_window(struct packet window[]) {
    bzero((char *) window, sizeof(window));
}

int ack_packet(int seq_no, struct packet window[], int window_size) {
    if (window[0].seq_no == seq_no) {
        int j;
        for (j = 0; j < window_size - 1; j++) {
            window[j] = window[j + 1];
        }
        return 0;
    }
    else {
        return 1;
    }
    error("Packet isn't in the window");
    return 2;
}

void add_packet(struct packet new,struct packet window[], int window_size) {
    int i;
    for (i = 1; i < window_size; i++) {
        if (window[i].seq_no == window[i - 1].seq_no) {
            window[i] = new;
            return;
        }
    }
    error("Window is full!");
}

struct packet get_packet(int seq_no, struct packet window[], int window_size) {
    int i;
    for (i = 0; i < window_size; i++) {
        if (window[i].seq_no == seq_no) {
            return window[i];
        }
    }
    error("Packet isn't in the window");
}

int main(int argc, char *argv[]) {
    int socketfd = 0, mode = 0;
    struct sockaddr_in serv_addr, cli_addr;
    int pid, clilen, portno, n, base, next_seq_num;
    struct packet req_pkt, rspd_pkt;
    struct stat st;
    //add window_size to all function parameters
    //set window_size to argument
    int window_size;
    FILE *resou0rce;
    time_t timer;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    portno = atoi(argv[1]);
    window_size = argv[2];
    struct packet window[window_size];
    int loss_prob = argv[3];
    int corrupt_prob = argv[4];

    /* creates unnamed socket inside the kernel and returns socket descriptor
* AF_INET specifies IPv4 addresses
* SOCK_STREAM specifies transport layer should be reliable
* third argument is left 0 to default to TCP
*/
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd < 0)
        error("ERROR opening socket");
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno);

    // assigns the details specified in the structure ‘serv_addr’ to the socket created in the step above. The details include, the family/domain, the interface to listen on(in case the system has multiple interfaces to network) and the port on which the server will wait for the client requests to come.
    if (bind(socketfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    clilen = sizeof(cli_addr);

    // run in an infinite loop so that the server is always running
    while(1) {
        if (recvfrom(socketfd, &req_pkt, sizeof(req_pkt), 0, (struct sockaddr*) &cli_addr, (socklen_t*) &clilen) < 0 || random_num() < loss_prob)
            error("ERROR on receiving from client");
        printf("Server received %d bytes from %s: %s\n", req_pkt.length, inet_ntoa(cli_addr.sin_addr), req_pkt.data);

        base = 1;
        next_seq_num = 1;
        clear_window(window);

        int i, total_packets;
        resource = fopen(req_pkt.data, "rb");
        if (resource == NULL || random_num() < corrupt_prob)
            error("ERROR opening file");

        stat(req_pkt.data, &st);
        total_packets = st.st_size / DATA_SIZE;
        if (st.st_size % DATA_SIZE)
            total_packets++;
        printf("Total packets: %d\n", total_packets);

        bzero((char *) &rspd_pkt, sizeof(rspd_pkt));
        rspd_pkt.type = 2;

        for (i = 0; i < window_size; i++) {
            rspd_pkt.seq_no = i + 1;
            rspd_pkt.length = fread(rspd_pkt.data, 1, DATA_SIZE, resource);
            window[i] = rspd_pkt;
            if (sendto(socketfd, &rspd_pkt, sizeof(int) * 3 + rspd_pkt.length, 0, (struct sockaddr *) &cli_addr, clilen) < 0)
                error("ERROR on sending");
            printf("Sent packet number %d\n", rspd_pkt.seq_no);
            next_seq_num++;
        }

        while(base <= total_packets) {
            if (mode == 1 && time(NULL) > timer + TIMEOUT) {
                printf("Timeout on packet %d!\n", window[0].seq_no);
                for (i = 0; i < window_size; i++) {
                    if (window[i].seq_no >= next_seq_num || window[i].seq_no == window[i - 1].seq_no)
                        break;
                    else if (sendto(socketfd, &window[i], sizeof(int) * 3 + window[i].length, 0, (struct sockaddr *) &cli_addr, clilen) < 0)
                        error("ERROR on sending");
                }
                mode = 0;
            }
            if (recvfrom(socketfd, &req_pkt, sizeof(req_pkt), 0, (struct sockaddr*) &cli_addr, (socklen_t*) &clilen) > 0) {
                printf("Received ACK for packet %d\n", req_pkt.seq_no);
                if (ack_packet(req_pkt.seq_no,window,window_size) == 1 && mode == 0) {
                    printf("Entering timeout mode\n");
                    mode = 1;
                    time(&timer);
                    continue;
                }
                
                if (mode == 0)
                    base = req_pkt.seq_no + 1;

                if (next_seq_num <= min(base + WIN_SIZE, total_packets)) {
                    rspd_pkt.seq_no = next_seq_num;
                    rspd_pkt.length = fread(rspd_pkt.data, 1, DATA_SIZE, resource);
                    add_packet(rspd_pkt,window,window_size);
                    if (sendto(socketfd, &rspd_pkt, sizeof(int) * 3 + rspd_pkt.length, 0, (struct sockaddr *) &cli_addr, clilen) < 0)
                        error("ERROR on sending");
                    printf("Sent packet number %d\n", next_seq_num);
                    next_seq_num++;
                }
            }
        }
        bzero((char *) &rspd_pkt, sizeof(rspd_pkt));
        rspd_pkt.type = 3;
        puts("Teardown");
        if (sendto(socketfd, &rspd_pkt, sizeof(rspd_pkt.type) * 3, 0, (struct sockaddr *) &cli_addr, clilen) < 0)
            error("ERROR on sending");
        fclose(resource);
    }
    return 0;
}