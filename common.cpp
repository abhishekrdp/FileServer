#include "common.h"
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

double getcurrenttime()
{
    struct timeval timestruct;
    gettimeofday(&timestruct, NULL);
    return (double) (1.0 * timestruct.tv_sec + (timestruct.tv_usec / 1000000.0));
}

int get_num_to_read(char type)
{
    if (type == 0) //control message
        return (sizeof (Packet) + 1);
    else
        return (DATA_BLOCK_SIZE + 1);
}

void remove_read_from_buf(char *buf, int num)
{
    char tempbuf[MY_SOCK_BUFFER_LEN];

    memcpy(tempbuf, buf + num, MY_SOCK_BUFFER_LEN - num);
    memcpy(buf, tempbuf, MY_SOCK_BUFFER_LEN);
}

int send_a_control_packet_to_socket(int sockfd, Packet *packet)
{
    char buf[1500];
    buf[0] = 0;
    memcpy(&(buf[1]), (char *) packet, sizeof (Packet));

    int send_result = send(sockfd, (void*) (buf), sizeof (Packet) + 1, 0);

    return send_result;
}

int send_a_file_block_to_socket(int sockfd, char *block)
{
    char buf[1500];
    buf[0] = 1;
    memcpy(&(buf[1]), block, DATA_BLOCK_SIZE);

    int send_result = send(sockfd, (void*) (buf), DATA_BLOCK_SIZE + 1, 0);

    return send_result;
}

void printpacket(char *buf, int nbytes)
{
    printf("\n");
    for (int i = 0; i < nbytes / 20; i++)
    {
        for (int j = 0; j < 20; j++)
        {
            printf("%-4d ", buf[i * 20 + j]);
        }
        printf("\n");
    }
    printf("\n");
}

void print_packet(struct Packet *p)
{
    int nbytes = sizeof (Packet);
    char *buf;

    buf = (char *) p;

    printf("\n");
    for (int i = 0; i < nbytes / 20; i++)
    {
        for (int j = 0; j < 20; j++)
        {
            printf("%-4d ", buf[i * 20 + j]);
        }
        printf("\n");
    }
    printf("\n");
}

void generate_file(int file_id, int length, char *buf)
{
    int i;
    int random_number;

    srand48(file_id);

    for (i = 0; i < length; i++)
    {
        random_number = lrand48() & 0xff;
        buf[i] = (char) random_number;
    }
}

void find_file_hash(int file_length, char *file, unsigned char *hash)
{
    SHA1Context foo;

    SHA1Init(&foo);
    SHA1Update(&foo, file, file_length);
    SHA1Final(&foo, hash);
}

void print_hash(unsigned char *hash)
{
    for (int i = 0; i < 20; i++)
        printf("%-4d ", hash[i]);

    printf("\n");
}

void show_file(int length, char *buf)
{
    int i;

    for (i = 0; i < length; i++)
    {
        printf("%-4d", buf[i]);
    }
}
