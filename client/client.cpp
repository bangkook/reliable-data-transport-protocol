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
#include "../rdtp.cpp"

using namespace std;

#define PORT 8080
#define MAXLINE 1024
#define CHUNK_SIZE 500

void sendPacket(const struct packet packet, struct sockaddr_in server_addr);
void sendFileRequest(const char* filename, struct sockaddr_in server_addr);
void recieveFile(struct sockaddr_in server_addr);
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

    sendFileRequest("test.txt", server_addr);
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

    // Recieve data
    recieveFile(server_addr);
}

void recieveFile(struct sockaddr_in server_addr) {
    int expectedSequenceNumber = 0;
    struct packet dataPacket;
    socklen_t serverAddrLen = sizeof(server_addr);
    recvfrom(sockfd, &dataPacket, sizeof(struct packet), MSG_WAITALL, (struct sockaddr*)&server_addr, &serverAddrLen);
    cout << dataPacket.data << endl;

    // Check if the acknowledgment is for the correct sequence number
    
    if (dataPacket.seqno == expectedSequenceNumber) {
        cout << "Data received with sequence number: " << dataPacket.seqno << endl;
        sendAck(dataPacket.seqno, server_addr);
        // Increment sequence number for the next packet
        //expectedSequenceNumber++;
    } else {
        cerr << "Unexpected packet received with sequence number: " << dataPacket.seqno << endl;
    }
}

void sendAck(int seqno, struct sockaddr_in server_addr) {
    struct ack_packet ackPacket;
    ackPacket.ackno = seqno;
    ackPacket.len = sizeof(struct ack_packet);
    sendto(sockfd, &ackPacket, sizeof(struct ack_packet), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
}
