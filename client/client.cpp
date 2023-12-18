#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <time.h>
#include "../rdtp.cpp"

using namespace std;

#define PORT 8080
#define MAXLINE 1024
#define CHUNK_SIZE 500
#define TIMEOUT 15

void sendPacket(const struct packet packet, struct sockaddr_in server_addr);
void sendFileRequest(const char* filename, struct sockaddr_in server_addr);
void recieveFile(struct sockaddr_in server_addr, const char* filename);
bool receiveAck(int sockfd, uint32_t expectedSequenceNumber, const sockaddr_in& server_addr);
void sendAck(int seqno, struct sockaddr_in server_addr);

int sockfd;

int main() {
    char* server_ip = "127.0.0.1";
    in_port_t server_port = PORT;
    int rtn_val;
    struct sockaddr_in server_addr;

     if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        error("socket failed");
     }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &(server_addr.sin_addr));

    // Set timeout to resend packet if datagram is lost
    //struct timeval timeout;
    //timeout.tv_sec = TIMEOUT;
    //timeout.tv_usec = 0;
    //setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    sendFileRequest("code.txt", server_addr);

    close(sockfd);
    return 0;
}

void sendPacket(const struct packet packet, struct sockaddr_in server_addr) {
    sendto(sockfd, &packet, sizeof(struct packet), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    std::cout << "Sent packet with sequence number: " << packet.seqno << std::endl;
}

void sendFileRequest(const char* filename, struct sockaddr_in server_addr) {
    struct packet requestPacket;
    requestPacket.seqno = 0;
    requestPacket.len = sizeof(struct packet);
    strncpy(requestPacket.data, filename, CHUNK_SIZE);

    // Send file request packet
    sendPacket(requestPacket, server_addr);
    time_t starttime = time(NULL);

    while(true) {
        if(receiveAck(sockfd, requestPacket.seqno, server_addr))
            break;
        // If timeout happens, resend packet
        if(time(NULL) - starttime >= TIMEOUT) {
            cout << "Resending.." << endl; 
            sendPacket(requestPacket, server_addr);
            starttime = time(NULL);
        }
    }

    // Recieve data
    recieveFile(server_addr, filename);
}

void recieveFile(struct sockaddr_in server_addr, const char* filename) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    int expectedSequenceNumber = 0;
    struct packet dataPacket;
    socklen_t serverAddrLen = sizeof(server_addr);
    size_t bytes_recv;

    while (true) {
        bytes_recv = recvfrom(sockfd, &dataPacket, sizeof(struct packet), 0, (struct sockaddr*)&server_addr, &serverAddrLen);
        cout << dataPacket.data << endl;
        cout << dataPacket.len << endl;
        // Check if the acknowledgment is for the correct sequence number
        if (dataPacket.seqno == expectedSequenceNumber) {
            cout << "Data received with sequence number: " << dataPacket.seqno << endl;
            sendAck(dataPacket.seqno, server_addr);
            // Write data to the file
            fwrite(dataPacket.data, 1, strlen(dataPacket.data), file);
            // Increment sequence number for the next packet
            expectedSequenceNumber++;
        } else if (dataPacket.seqno == UINT32_MAX) { 
            // End of file marker received
            sendAck(dataPacket.seqno, server_addr);
            cout << "End of file" << endl;
            break;
        } else {
            cerr << "Unexpected packet received with sequence number: " << dataPacket.seqno << endl;
        }
    }

    fclose(file);
}

void sendAck(int seqno, struct sockaddr_in server_addr) {
    struct ack_packet ackPacket;
    ackPacket.ackno = seqno;
    ackPacket.len = sizeof(struct ack_packet);
    sendto(sockfd, &ackPacket, sizeof(struct ack_packet), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
}

bool receiveAck(int sockfd, uint32_t expectedSequenceNumber, const sockaddr_in& server_addr) {
    struct ack_packet ackPacket;
    socklen_t serverAddrLen = sizeof(server_addr);
    size_t bytes = recvfrom(sockfd, &ackPacket, sizeof(struct ack_packet), MSG_DONTWAIT, (struct sockaddr*)&server_addr, &serverAddrLen);

    // No acknowledge is received
    if(bytes != sizeof(struct ack_packet))
        return false;

    // Check if the acknowledgment is for the correct sequence number
    if (ackPacket.ackno == expectedSequenceNumber) {
        cout << "Acknowledgment received for sequence number: " << ackPacket.ackno << endl;
        return true;
    } 

    cerr << "Unexpected acknowledgment received for sequence number: " << ackPacket.ackno << endl;
    return false;
}