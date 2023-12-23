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
#define WINDOW_SIZE 4
#define TIMEOUT 0

#define MSS 508

enum state{SlowStart, CongestionAvoidance, FastRecovery};

void handleClient(const int sockfd, const sockaddr_in& clientAddr, const char* filename, int seed, float prob);
void sendPacket(const int sockfd, const struct packet& packet, const sockaddr_in& clientAddr);
void receiveAck(const int sockfd, uint32_t expectedSequenceNumber, const sockaddr_in& clientAddr);
void sendAck(int sockfd, int seqno, const sockaddr_in& clientAddr, int fileSize);
void sendFile(const int sockfd, const char* filename, const sockaddr_in& clientAddr, int seed, float prob);
vector<string> splitFile(const char* filename);
vector<string> readArgs(string filename);

int main() {
    vector<string> args = readArgs("server.in");
    int port = stoi(args[0]);
    int seed = stoi(args[1]);
    float prob = stof(args[2]);

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
    servaddr.sin_port = htons(port); 
       
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
        
        // Fork a child process to handle the client
        pid_t childPid = fork();

        if (childPid == 0) {
            // Child process
            close(sockfd); // Close the parent socket in the child

            // Create a new UDP socket for file transfer
            int udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
            
            // Handle the client in the child process
            handleClient(udpSocket, clientAddr, packet.data, seed, prob);

            close(udpSocket); // Close the client socket in the child
            cout << "Connection closed\n";
            exit(0);
        } else if (childPid > 0) {
            // Parent process
        } else {
            cerr << "Error forking process" << endl;
        }
    }

    close(sockfd);

    return 0; 
}

void handleClient(int sockfd, const sockaddr_in& clientAddr, const char* filename, int seed, float prob) {
    cout << "Handling client from " << inet_ntoa(clientAddr.sin_addr) << endl;

    // Implement the file transfer logic as before
    sendFile(sockfd, filename, clientAddr, seed, prob);
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

set<int> findLostPackets(int seed, float p, int numOfPackets) {
    set<int> lost_packets;
    int numOfLossPackets = ceil(p * numOfPackets);
    srand(seed);
    while(lost_packets.size() < numOfLossPackets) {
        int k = rand() % numOfPackets;
        if(!lost_packets.count(k)){
            lost_packets.insert(k);
        }
    }
    return lost_packets;
}

void sendFile(int sockfd, const char* filename, const sockaddr_in& clientAddr, int seed, float prob) {
    vector<string> chunks = splitFile(filename);
    // Send acknowledgement for the file request packet
    sendAck(sockfd, 0, clientAddr, chunks.size());

    int numOfPackets = chunks.size();
    socklen_t clientAddrLen = sizeof(clientAddr);
    bool sent[numOfPackets];
    bool acked[numOfPackets];
    memset(sent, false, sizeof(sent));
    memset(acked, false, sizeof(acked));

    set<int> lost = findLostPackets(seed, prob, numOfPackets);

    queue<pair<int, time_t>> time_sequence;

    int sequenceNumber = 0;
    int window_base = 0;
    int cwnd = 1;
    int ssthresh = 8 * MSS;//64000;
    int dupAckCnt = 0;
    int notAcked = 0;

    state st = SlowStart;

    // Write to log file when cwnd changes
    ofstream file("log.txt");

    // cwnd = last sent - last unacked
    cout << "Num of packets to be sent: " << numOfPackets << endl;
    while(window_base < numOfPackets) {
        for(int j = window_base; j < window_base + cwnd && j < numOfPackets; j++) {
            if(lost.count(j)) {
                lost.erase(j);
                sent[j] = 1;
                time_t start_time = time(NULL);
                time_sequence.push({j, start_time});
                cout << "Packet lost with sequence number " << j << endl;
            } else if(!sent[j]) {
                struct packet dataPacket;
                dataPacket.seqno = j;
                strncpy(dataPacket.data, chunks[j].c_str(), CHUNK_SIZE);
                dataPacket.len = sizeof(dataPacket);

                // Save sending time
                time_t starttime = time(NULL);
                time_sequence.push(make_pair(j, starttime));
                sent[j] = true;
                
                // Send data packet
                sendPacket(sockfd, dataPacket, clientAddr);
                notAcked++;
            }
        }

        dupAckCnt = 0;
        while(notAcked) {
            struct ack_packet ackPacket;
            int bytes = recvfrom(sockfd, &ackPacket, sizeof(struct ack_packet), 0, (struct sockaddr*)&clientAddr, &clientAddrLen);
            if(bytes != sizeof(ack_packet)) {
                cout << "Expected ACK packet but received something else" << endl;
                continue;
            }
            if(ackPacket.ackno < window_base || ackPacket.ackno >= window_base + cwnd)
                continue;
            
            // duplicate ACK
            if(acked[ackPacket.ackno]) {
                dupAckCnt++;
                cout << "Duplicate Acks" << endl;
                if(st == FastRecovery) {
                    cwnd++;
                    //break;
                } else if(dupAckCnt == 3) {
                    cout << "Triple duplicate Acks" << endl;
                    ssthresh = (cwnd * MSS) / 2;
                    cwnd = cwnd / 2 + 3;
                    st = FastRecovery;
                    cout << "New ssthresh = " << ssthresh << " , cwnd = " << cwnd << " segments\n";
                }
                file << cwnd << endl;
                continue;
            }

            // new ACK
            acked[ackPacket.ackno] = 1;
            dupAckCnt = 0;
            notAcked--;
            cout << "Received new ack " << ackPacket.ackno << endl;
            if(window_base == ackPacket.ackno) {
                window_base++;
                while(window_base < numOfPackets && acked[window_base])// Slide window to last acknowledged
                    window_base++;
            }

            if(st == SlowStart) {
                // Multiplicative increase
                if(cwnd * 2 * MSS <= ssthresh){
                    cwnd *= 2;
                }
                else {// Additive increase
                    cwnd += 1, st = CongestionAvoidance;
                }
            } else if(st == CongestionAvoidance) {
                cwnd++;
            } else if (st == FastRecovery) {
                cwnd = ssthresh / MSS;
                dupAckCnt = 0;
                st = CongestionAvoidance;
            }
            file << cwnd << endl;
            cout << "cwnd = " << cwnd << endl;
        }

        // Handle time-out
        while(!time_sequence.empty()) {
            pair<int, time_t> seq = time_sequence.front();
            time_t now_time = time(NULL);
            if(now_time - seq.second < TIMEOUT)
                break;

            if(acked[seq.first]) {
                time_sequence.pop();
                continue;
            }
            // TIMEOUT
            cout << "Timeout for packet with sequence number " << seq.first << endl;
            time_sequence.pop();
            // retransmit packet
            struct packet dataPacket;
            dataPacket.seqno = seq.first;
            strncpy(dataPacket.data, chunks[seq.first].c_str(), CHUNK_SIZE);
            dataPacket.len = sizeof(dataPacket);

            // Save sending time
            time_sequence.push(make_pair(seq.first, now_time));
            sent[seq.first] = true;
            
            cout << "Retransmitting packet with sequence number " << seq.first << endl;
            // Send data packet
            sendPacket(sockfd, dataPacket, clientAddr);
            notAcked++;

            ssthresh = (cwnd * MSS) / 2;
            cwnd = 1;
            st = SlowStart;
            cout << "Switching to slow start ssthresh = " << ssthresh << ", cwnd = " << cwnd << endl;
            file << cwnd << endl;
            break;
        }
    }
    struct packet dataPacket;
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

void sendAck(int sockfd, int seqno, const sockaddr_in& clientAddr, int fileSize) {
    struct ack_packet ackPacket;
    ackPacket.ackno = seqno;
    ackPacket.len = fileSize;
    sendto(sockfd, &ackPacket, sizeof(struct ack_packet), 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr));
}

void receiveAck(int sockfd, uint32_t expectedSequenceNumber, const sockaddr_in& clientAddr) {
    struct ack_packet ackPacket;
    socklen_t clientAddrLen = sizeof(clientAddr);
    recvfrom(sockfd, &ackPacket, sizeof(struct ack_packet), MSG_WAITALL, (struct sockaddr*)&clientAddr, &clientAddrLen);
    cout << "Acknowledgment received for sequence number: " << ackPacket.ackno << endl;
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
