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

void handleClient(const int sockfd, const sockaddr_in& clientAddr, const char* filename);
void sendPacket(const int sockfd, const struct packet& packet, const sockaddr_in& clientAddr);
void receiveAck(const int sockfd, uint32_t expectedSequenceNumber, const sockaddr_in& clientAddr);
void sendAck(int sockfd, int seqno, const sockaddr_in& clientAddr);
void sendFile(const int sockfd, const char* filename, const sockaddr_in& clientAddr);
vector<string> splitFile(const char* filename);

int main() {
    int sockfd; 
    struct packet packet;
    struct sockaddr_in servaddr, cliaddr; 

    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        error("socket creation failed"); 
    } 
       
    memset(&servaddr, 0, sizeof(servaddr)); 
    memset(&cliaddr, 0, sizeof(cliaddr));

    // Filling server information 
    servaddr.sin_family    = AF_INET; // IPv4 
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    servaddr.sin_port = htons(PORT); 
       
    // Bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,  
            sizeof(servaddr)) < 0 ) 
    { 
        error("bind failed"); 
    } 
    
    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        int n = recvfrom(sockfd, &packet, sizeof(struct packet),  
                MSG_WAITALL, ( struct sockaddr *) &clientAddr, 
                &clientAddrLen); 
        cout << packet.data << endl;
        
        /*
        // Accept a new connection
        int clientSocket = accept(sockfd, (struct sockaddr*)&clientAddr, &clientAddrLen);

        if (clientSocket < 0) {
            // cerr << "Error accepting connection" << endl;
            continue;
        }*/
        //handleClient(sockfd, clientAddr, packet.data);
        // Fork a child process to handle the client
        pid_t childPid = fork();

        if (childPid == 0) {
            // Child process
            close(sockfd); // Close the parent socket in the child

            // Create a new UDP socket for file transfer
            int udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
            // Send acknowledgement for the file request packet
            sendAck(udpSocket, packet.seqno, clientAddr);
            // Handle the client in the child process
            handleClient(udpSocket, clientAddr, packet.data);

            close(udpSocket); // Close the client socket in the child
            exit(0);
        } else if (childPid > 0) {
            // Parent process
            //close(clientSocket); // Close the client socket in the parent
        } else {
            cerr << "Error forking process" << endl;
        }
    }

    close(sockfd);

    return 0; 
}

void handleClient(int sockfd, const sockaddr_in& clientAddr, const char* filename) {
    cout << "Handling client from " << inet_ntoa(clientAddr.sin_addr) << endl;

    // Implement the file transfer logic as before
    sendFile(sockfd, filename, clientAddr);
}

vector<string> splitFile(const char* filename) {
    ifstream file(filename, ios::binary);
    vector<string> chunks;

    if (!file) {
        cerr << "Error opening file: " << filename << endl;
        return chunks;
    }

    string chunk;
    while (true) {
        char buffer[CHUNK_SIZE];
        file.read(buffer, CHUNK_SIZE);

        if (file.gcount() > 0) {
            chunk.assign(buffer, file.gcount());
            chunks.push_back(chunk);
        } else {
            break; // End of file
        }
    }

    file.close();
    return chunks;
}

// Send and wait
void sendFile(int sockfd, const char* filename, const sockaddr_in& clientAddr) {
    vector<string> chunks = splitFile(filename);
    int sequenceNumber = 0;

    struct packet dataPacket;
    for (const auto& chunk : chunks) {
        dataPacket.seqno = sequenceNumber;
        strncpy(dataPacket.data, chunk.c_str(), CHUNK_SIZE);
        dataPacket.len = sizeof(dataPacket);
        cout << "Sent data: " << dataPacket.data << endl;
        // Send data packet
        sendPacket(sockfd, dataPacket, clientAddr);
        
        // Wait for acknowledgment
        receiveAck(sockfd, sequenceNumber, clientAddr);

        // Increment sequence number for the next packet
        sequenceNumber++;
    }

    // End of file, send a special packet to indicate completion
    dataPacket.seqno = UINT32_MAX;
    string msg = "End of file";
    strncpy(dataPacket.data, msg.c_str(), CHUNK_SIZE);
    dataPacket.len = sizeof(dataPacket);
    sendPacket(sockfd, dataPacket, clientAddr);
}

void sendPacket(int sockfd, const struct packet& packet, const sockaddr_in& clientAddr){
    sendto(sockfd, &packet, sizeof(struct packet), 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr));
    cout << "Sent packet with sequence number: " << packet.seqno << endl;
}

void sendAck(int sockfd, int seqno, const sockaddr_in& clientAddr) {
    struct ack_packet ackPacket;
    ackPacket.ackno = seqno;
    ackPacket.len = sizeof(struct ack_packet);
    sendto(sockfd, &ackPacket, sizeof(struct ack_packet), 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr));
}

void receiveAck(int sockfd, uint32_t expectedSequenceNumber, const sockaddr_in& clientAddr) {
    struct ack_packet ackPacket;
    socklen_t clientAddrLen = sizeof(clientAddr);
    recvfrom(sockfd, &ackPacket, sizeof(struct ack_packet), MSG_WAITALL, (struct sockaddr*)&clientAddr, &clientAddrLen);

    // Check if the acknowledgment is for the correct sequence number
    if (ackPacket.ackno == expectedSequenceNumber) {
        cout << "Acknowledgment received for sequence number: " << ackPacket.ackno << endl;
    } else {
        cerr << "Unexpected acknowledgment received for sequence number: " << ackPacket.ackno << endl;
    }
}
