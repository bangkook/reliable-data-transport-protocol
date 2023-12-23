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
void recieveFile(struct sockaddr_in server_addr, const char* filename, int fileSize);
int receiveAck(int sockfd, uint32_t expectedSequenceNumber, const sockaddr_in& server_addr);
void sendAck(int seqno, struct sockaddr_in server_addr);
vector<string> readArgs(string filename);

int sockfd;

int main() {
    vector<string> args = readArgs("client.in");

    const char* server_ip = args[0].c_str();
    in_port_t server_port = stoi(args[1]);
    string filename = args[2];

    int rtn_val;
    struct sockaddr_in server_addr;

     if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        error("socket failed");
     }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &(server_addr.sin_addr));

    sendFileRequest(filename.c_str(), server_addr);

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
    int len = 0;
    while(true) {
        len = receiveAck(sockfd, requestPacket.seqno, server_addr);
        if(len > -1)
            break;
        // If timeout happens, resend packet
        if(time(NULL) - starttime >= TIMEOUT) {
            cout << "Resending.." << endl; 
            sendPacket(requestPacket, server_addr);
            starttime = time(NULL);
        }
    }

    // Recieve data
    recieveFile(server_addr, filename, len);
}

void recieveFile(struct sockaddr_in server_addr, const char* filename, int fileSize) {
    int expectedSequenceNumber = 0;
    socklen_t serverAddrLen = sizeof(server_addr);
    size_t bytes_recv;
    string data[fileSize];

    while (true) {
        struct packet dataPacket;
        bytes_recv = recvfrom(sockfd, &dataPacket, sizeof(struct packet), 0, (struct sockaddr*)&server_addr, &serverAddrLen);
        if (dataPacket.seqno == UINT32_MAX) { 
            // End of file marker received
            sendAck(dataPacket.seqno, server_addr);
            cout << "End of file" << endl;
            break;
        }
        // Check if the acknowledgment is for the correct sequence number
        if(dataPacket.seqno < fileSize)
            data[dataPacket.seqno] = dataPacket.data;
        cout << "Data received with sequence number: " << dataPacket.seqno << endl;
        sendAck(dataPacket.seqno, server_addr);        
    }

    ofstream file(filename, ios::binary);
    if (!file) {
        cerr << "Error opening file: " << filename << endl;
        error("");
    }
    for(int i = 0; i < fileSize; i++) {
        file.write(data[i].c_str(), data[i].length());
    }
    file.close();
}

void sendAck(int seqno, struct sockaddr_in server_addr) {
    struct ack_packet ackPacket;
    ackPacket.ackno = seqno;
    ackPacket.len = sizeof(struct ack_packet);
    sendto(sockfd, &ackPacket, sizeof(struct ack_packet), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
}

int receiveAck(int sockfd, uint32_t expectedSequenceNumber, const sockaddr_in& server_addr) {
    struct ack_packet ackPacket;
    socklen_t serverAddrLen = sizeof(server_addr);
    size_t bytes = recvfrom(sockfd, &ackPacket, sizeof(struct ack_packet), MSG_DONTWAIT, (struct sockaddr*)&server_addr, &serverAddrLen);

    // No acknowledge is received
    if(bytes != sizeof(struct ack_packet))
        return -1;

    // Check if the acknowledgment is for the correct sequence number
    if (ackPacket.ackno == expectedSequenceNumber) {
        cout << "Acknowledgment received for sequence number: " << ackPacket.ackno << endl;
        return ackPacket.len;
    } 

    cerr << "Unexpected acknowledgment received for sequence number: " << ackPacket.ackno << endl;
    return -1;
}

vector<string> readArgs(string filename) {
    vector<string> args;
    ifstream file(filename, ios::binary);

    if (!file) {
        cerr << "Error opening file: " << filename << endl;
        return args;
    }

    string line;
    while(getline(file, line)) {
        args.push_back(line);
    }
    return args;
}