#ifndef __COMMON_H

#define __COMMON_H

#include "sha.h"

#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <stdint.h>
#include <string.h>
#include<algorithm>
double getcurrenttime();



#define MAXBUFLEN 2000
#define MAXIPLEN 100
#define FILE_NUMBER    64

#define PACKET_TYPE_CONTROL 0
#define PACKET_TYPE_DATA    1

#define DATA_BLOCK_SIZE 1000

#define MY_SOCK_BUFFER_LEN 3000

int myrecv (int s, char* buf, int numbytestoread, int flag);

#define EVENT_TYPE_CLIENT_REGISTER         0
#define EVENT_TYPE_CLIENT_REQ_FILE         1
#define EVENT_TYPE_SERVER_REPLY_REQ_FILE   2
#define EVENT_TYPE_CLIENT_GOT_FILE         3
#define EVENT_TYPE_CLIENT_REQ_FILE_FROM_PEER 4
#define EVENT_TYPE_CLIENT_QUIT               5
#define EVENT_TYPE_SERVER_QUIT               6

struct Packet
{
    int sender;
    int recipient;
    int event_type;    
    int port_number;  // for reporting listening port number
    int req_file_index; //
    char peerip[MAXIPLEN]; // for telling the client the peer IP
    int peerid;    
    int peer_listen_port;
    char FILE_VECTOR[FILE_NUMBER];
    unsigned char hash[20];
};

// it seemed that when casting into a packet, before and after Challenge, there are 4 bytes 0. 

void printpacket(char *buf, int nbytes);
void print_packet(struct Packet *p);
int send_a_control_packet_to_socket(int sockfd, Packet *packet);
int send_a_file_block_to_socket(int sockfd, char *block);
void print_hash(unsigned char *hash);
void remove_read_from_buf(char *buf, int num);

int get_num_to_read(char type);

#define SERVER_NODE_ID 0

#define FILE_LENGTH 20000
void generate_file(int file_id, int length, char *buf);
void show_file(int length, char *buf);
void find_file_hash(int file_length, char *file, unsigned char *hash);


using namespace std;


#endif
