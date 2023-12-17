#include <iostream>
#include <bits/stdc++.h>
#include <string>

using namespace std;

/* Data-only packets */
struct packet {
    /* Header */
    uint16_t len;
    uint32_t seqno;
    /* Data */
    char data[500];
};

/* Ack-only packets are only 8 bytes */
struct ack_packet {
    uint16_t len;
    uint32_t ackno;
};

void error(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}