#include "common.h"
#include "client.h"
#include <pthread.h>

#define EXP_CMD_TYPE_FIRST_LEVEL 0
#define EXP_CMD_TYPE_FILE_NUMBER 1
#define EXP_CMD_TYPE_SEND_FILE_YN 2
#define EXP_CMD_TYPE_SEND_KEY     3

unsigned char exp_cmd_type = 0;

int current_asking_file_index = -1;
int current_file_asked_by_client = -1;
unsigned char current_asking_file_hash[20];
char current_asking_file[FILE_LENGTH];
int current_asking_file_next_location = 0;
int current_asking_peer = -1;


char FILE_VECTOR[FILE_NUMBER] = "000000000000000000000000000000000000000000000000000000000000000";

char server_sock_buf[MY_SOCK_BUFFER_LEN];
int server_sock_buf_byte_counter;

char peer_sock_buf[MY_SOCK_BUFFER_LEN];
int peer_buf_counter;

class Connection
{
public:
    int socket;
    char peerip[MAXIPLEN];
    int peerport;
    int peerid;
    char sock_buf[MY_SOCK_BUFFER_LEN];
    int sock_buf_byte_counter;
    int needed_file;
    int current_block_count;

    Connection()
    {
        socket = -1;
        peerport = -1;
        sprintf(peerip, "%s", "0.0.0.0");
        peerid = -1;
        sock_buf_byte_counter = 0;
        needed_file = -1;
        current_block_count = 0;
    }

    Connection(int sock, const char* ip, int port)
    {
        socket = sock;
        peerport = port;
        sprintf(peerip, "%s", ip);
        peerid = -1;
        sock_buf_byte_counter = 0;
        needed_file = -1;
        current_block_count = 0;
    }

    Connection(int sock, const char* ip, int port, int id)
    {
        socket = sock;
        peerport = port;
        sprintf(peerip, "%s", ip);
        peerid = id;
        sock_buf_byte_counter = 0;
        needed_file = -1;
        current_block_count = 0;
    }
};

vector<Connection> activeconnections;

struct option long_options[] = {
    {"serverip", required_argument, NULL, 's'},
    {"serverport", required_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {"config", required_argument, NULL, 'c'},
    {"id", required_argument, NULL, 'i'},
    {"debug", required_argument, NULL, 'd'},
    {"servername", required_argument, NULL, 'm'},
    { 0, 0, 0, 0}
};

fd_set master;
fd_set read_fds;

int serversockfd = -1;
int listener = -1;

int highestsocket;
void read_config(const char*);
void read_from_activesockets();
void send_file(char key = '\0');

int connect_to_server(const char* IP, int port);
int connect_client(const char* ip, int port, int Peer);
struct sockaddr_in myaddr; // my address

int MYPEERID = 99;
int SERVERPORT = 51000;
int MY_LISTEN_PORT = 52000;

char SERVERNODE[100]; // = "diablo.cs.fsu.edu";
char SERVERIP[40]; // = "127.0.0.1";

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

struct filetosend
{
    int peerid;
    char thisfile[FILE_LENGTH];
    int blocksent;
    int file_index;
    bool done;
    int block_index;
    char key;
};

vector<filetosend> outfiles(100);

struct filetosend tosend;

void mute_fun(struct filetosend&);

int find_socket(int clientid)
{
    for (int i = 0; i < activeconnections.size(); i++)
    {
        if (activeconnections[i].peerid == clientid)
        {
            return activeconnections[i].socket;
        }
    }
    return -1;
}

void read_config(const char* configfile)
{
    FILE* f = fopen(configfile, "r");

    if (f)
    {
        fscanf(f, "CLIENTID %d\n", &MYPEERID);
        fscanf(f, "SERVERPORT %d\n", &SERVERPORT);
        fscanf(f, "MYPORT %d\n", &MY_LISTEN_PORT);
        fscanf(f, "FILE_VECTOR %s\n", FILE_VECTOR);

        fclose(f);

        printf("My ID is %d\n", MYPEERID);
        printf("Sever port is %d\n", SERVERPORT);
        printf("My port is %d\n", MY_LISTEN_PORT);
        printf("File vector is %s\n", FILE_VECTOR);
    }
    else
    {
        cout << "Cannot read the configfile!" << configfile << endl;
        ;
        fflush(stdout);
        exit(1);
    }
}

int connect_to_server(const char* IP, int PORT)
{
    struct sockaddr_in server_addr; // peer address\n";
    int sockfd;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("Cannot create a socket");
        return -1;
    }

    server_addr.sin_family = AF_INET; // host byte order 
    server_addr.sin_port = htons(PORT); // short, network byte order 
    inet_aton(IP, (struct in_addr *) &server_addr.sin_addr.s_addr);
    memset(&(server_addr.sin_zero), '\0', 8); // zero the rest of the struct 

    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof (struct sockaddr)) == -1)
    {
        printf("Error connecting to the server %s on port %d\n", IP, PORT);
        sockfd = -1;
    }

    if (sockfd != -1)
    {
        FD_SET(sockfd, &master);
        if (highestsocket <= sockfd)
        {
            highestsocket = sockfd;
        }
    }

    return sockfd;
}

void send_packet_to_server(Packet *packet)
{
    int send_result = send_a_control_packet_to_socket(serversockfd, packet);

    if (send_result == -1)
    {
        printf("Oh! Cannot send packet to server!\n");
        if (errno == EPIPE)
        {
            //printf("Trouble sending data on socket %d to client %d with EPIPE .. CLOSING HIS CONNECTION\n", socket2send, event->recipient);
            //connection_close(event->recipient);
        }
    }
    else
    {
        //printf("Ha! Sent packet to server!\n");
    }
}

void send_packet_to_peer(Packet *packet, int clientid)
{
    int sockettosend = find_socket(clientid);

    int send_result = send_a_control_packet_to_socket(sockettosend, packet);

    if (send_result == -1)
    {
        printf("Oh! Cannot send packet to client %d!\n", clientid);
        if (errno == EPIPE)
        {
            //printf("Trouble sending data on socket %d to client %d with EPIPE .. CLOSING HIS CONNECTION\n", sockettosend, clientid);
        }
    }
    else
    {
        //printf("Ha! Sent packet to client %d!\n", clientid);
        //printpacket((char*) packet, PACKET_SIZE);            
    }
}

void send_a_file_block_to_peer(char *block, int clientid)
{
    int sockettosend = find_socket(clientid);
    int send_result = send_a_file_block_to_socket(sockettosend, block);

    if (send_result == -1)
    {
        printf("Oh! Cannot send packet to client %d!\n", clientid);
        if (errno == EPIPE)
        {
            //printf("Trouble sending data on socket %d to client %d with EPIPE .. CLOSING HIS CONNECTION\n", sockettosend, clientid);
        }
    }
    else
    {
        //printf("Ha! Sent packet to client %d!\n", clientid);
        //printpacket((char*) packet, PACKET_SIZE);            
    }
}

void send_register_to_server(int clientid)
{
    Packet packet;

    packet.sender = clientid;
    packet.recipient = SERVER_NODE_ID;
    packet.event_type = EVENT_TYPE_CLIENT_REGISTER;
    packet.port_number = MY_LISTEN_PORT;
    memcpy(packet.FILE_VECTOR, FILE_VECTOR, FILE_NUMBER);

    send_packet_to_server(&packet);
}

void send_req_file_to_server(int clientid, int file_index)
{
    Packet packet;

    packet.sender = clientid;
    packet.recipient = SERVER_NODE_ID;
    packet.event_type = EVENT_TYPE_CLIENT_REQ_FILE;
    packet.req_file_index = file_index;

    send_packet_to_server(&packet);
}

void send_quit_to_server(int clientid)
{
    Packet packet;

    packet.sender = clientid;
    packet.recipient = SERVER_NODE_ID;
    packet.event_type = EVENT_TYPE_CLIENT_QUIT;

    send_packet_to_server(&packet);
}

void send_got_file_to_server(int clientid, int file_index)
{
    // todo: add your code here.
    Packet packet;
    packet.sender = clientid;
    packet.recipient = SERVER_NODE_ID;
    packet.event_type = EVENT_TYPE_CLIENT_GOT_FILE;
    packet.req_file_index = file_index;

    send_packet_to_server(&packet);
}

void send_req_file_to_peer(int clientid, int peerid, int file_index)
{
    // todo: add your code here.
    Packet packet;
    packet.sender = clientid;
    packet.recipient = peerid;
    packet.req_file_index = file_index;
    packet.event_type = EVENT_TYPE_CLIENT_REQ_FILE_FROM_PEER;

    send_packet_to_peer(&packet, peerid);
}

void client_init(void)
{
    tosend.peerid = -1;
}

void client_got_server_name(void)
{
    struct hostent *he_server;
    if ((he_server = gethostbyname(SERVERNODE)) == NULL)
    {
        printf("error resolving hostname for server %s\n", SERVERNODE);
        fflush(stdout);
        exit(1);
    }

    struct sockaddr_in server;
    memcpy(&server.sin_addr, he_server->h_addr_list[0], he_server->h_length);
    printf("SERVER IP is %s\n", inet_ntoa(server.sin_addr));
    strcpy(SERVERIP, inet_ntoa(server.sin_addr));
}

void client_got_server_IP(void)
{
    struct in_addr ipv4addr;
    inet_pton(AF_INET, SERVERIP, &ipv4addr);

    struct hostent *he_server;
    if ((he_server = gethostbyaddr(&ipv4addr, sizeof ipv4addr, AF_INET)) == NULL)
    {
        printf("error resolving hostnam for server %s\n", SERVERIP);
        fflush(stdout);
        exit(1);
    }

    printf("SERVER name is %s\n", he_server->h_name);
}

void deleteconnection(int clientid)
{
    bool flag = true;
    vector<Connection>::iterator iter;
    for (iter = activeconnections.begin(); iter != activeconnections.end() && flag; iter++)
    {
        Connection currentconn = *iter;
        if (currentconn.peerid == clientid)
        {
            flag = false;
            break;
        }
    }
    if (iter != activeconnections.end())
    {
        close((*iter).socket);
        activeconnections.erase(iter);
    }
}

void process_server_message(Packet *packet)
{
    int sock;
    if (packet->event_type == EVENT_TYPE_SERVER_REPLY_REQ_FILE)
    {
        printf("Server tells client %d to get file %d from client %d on %s with listen port_number %d\n", MYPEERID, current_asking_file_index, packet->peerid, packet->peerip, packet->peer_listen_port);
        printf("Hash of the file is => \n");

        print_hash(packet->hash);
        memcpy(current_asking_file_hash, packet->hash, 20);
        // todo: add your code here.
        /*********************************************************************************/
        printf("Connecting to client %d.......\n", packet->peerid);
        sock = connect_client(packet->peerip, packet->peer_listen_port, packet->peerid);
        if (sock != -1)
        {
            printf("I am connected to Client %d  and on socket %d\n", packet->peerid, sock);
            printf("Sending message to client %d for file no %d \n", packet->peerid, current_asking_file_index);
            send_req_file_to_peer(MYPEERID, packet->peerid, current_asking_file_index);
        }
    }
    else if (packet->event_type == EVENT_TYPE_SERVER_QUIT)
    {
        printf("server telling me to quit!\n");
        exit(0);
    }
}

/******************************************************************************/
int connect_client(const char* ip, int port, int Peer)
{
    int sockfd;
    struct sockaddr_in peer_addr;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("Cannot create a socket");
    }
    bzero(&peer_addr, sizeof (peer_addr));
    peer_addr.sin_family = AF_INET; // host byte order 
    peer_addr.sin_port = htons(port); // short, network byte order 
    inet_aton(ip, (struct in_addr *) &peer_addr.sin_addr.s_addr);

    if (connect(sockfd, (struct sockaddr *) &peer_addr, sizeof (peer_addr)) == -1)
    {
        printf("Error connecting to the server %s on port %d\n", ip, port);
        return -1;
    }

    if (sockfd != -1)
    {
        FD_SET(sockfd, &master);
        if (highestsocket <= sockfd)
        {
            highestsocket = sockfd;
        }
        // cout << "sockfd is " << sockfd << endl;
        Connection newconn(sockfd, inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port), Peer);
        activeconnections.push_back(newconn);
    }
    return sockfd;
}

/******************************************************************************/
void process_peer_message(Packet *packet)
{
    // todo: add your code here.
    if (packet->event_type == EVENT_TYPE_CLIENT_REQ_FILE_FROM_PEER)
    {
        int sock = find_socket(packet->sender);
        for (int i = 0; i < activeconnections.size(); i++)
        {
            if ((activeconnections[i].socket == sock) && (activeconnections[i].needed_file == -1))
            {
                activeconnections[i].needed_file = packet->req_file_index;
                cout << "Connection from " << activeconnections[i].peerid << " needs the file " << activeconnections[i].needed_file << endl;
                break;
            }
        }
        printf("Client %d is requesting for file %d, do you want to send the correct file?\n", packet->sender, packet->req_file_index);

        current_file_asked_by_client = packet->req_file_index;
        current_asking_peer = packet->sender;
    }

}

void process_stdin_message(void)
{
    char key;
    if (exp_cmd_type == EXP_CMD_TYPE_FIRST_LEVEL)
    {
        char c;
        scanf("\n%c", &c);

        if (c == 'f')
        {
            printf("please tell me what file you need, thanks.\n");
            exp_cmd_type = EXP_CMD_TYPE_FILE_NUMBER;
        }
        else if (c == 'q')
        {
            send_quit_to_server(MYPEERID);
        }
        else if (c == 'Y' || c == 'y')
        {
            //cout << "I am here\n";
            send_file(key);
            exp_cmd_type = EXP_CMD_TYPE_SEND_FILE_YN;
        }
        else if (c == 'N' || c == 'n')
        {
            printf("Enter the single byte/character key\n");
            scanf("\n%c", &key);
            send_file(key);
            exp_cmd_type = EXP_CMD_TYPE_SEND_KEY;
        }
        else
        {
            printf("wrong command. q or f. %c\n", c);
        }
    }
    else if (exp_cmd_type == EXP_CMD_TYPE_FILE_NUMBER)
    {
        int file_index = 0;
        scanf("%d", &file_index);

        printf("client %d need file %d\n", MYPEERID, file_index);
        if (FILE_VECTOR[file_index] == '1')
        {
            printf("I have this file. \n");
        }
        else
        {
            current_asking_file_index = file_index;
            send_req_file_to_server(MYPEERID, file_index);
        }
        exp_cmd_type = EXP_CMD_TYPE_FIRST_LEVEL;
    }
    else if (exp_cmd_type == EXP_CMD_TYPE_SEND_FILE_YN)
    {
        // todo: add your code here.
        //cout << "I am here in yes portion\n";
        //
        //scanf("\n%c", &key);

        exp_cmd_type = EXP_CMD_TYPE_FIRST_LEVEL;
    }
    else if (exp_cmd_type == EXP_CMD_TYPE_SEND_KEY)
    {
        // todo: add your code here.

        exp_cmd_type = EXP_CMD_TYPE_FIRST_LEVEL;
    }
    else
    {
        printf("oops, bad read\n");
    }

}

/******************************************************************************/
void send_file(char ch)
{
    //cout << "In send file for sending file\n";
    struct filetosend tos;
    if (!activeconnections.empty())
    {
        //  cout << "I am inside this checkpoint\n";
        for (int i = 0; i < activeconnections.size(); i++)
        {
            if ((activeconnections[i].needed_file == current_file_asked_by_client)
                    && (activeconnections[i].peerid == current_asking_peer))
            {
                tos.done = 0;
                tos.blocksent = 0;
                tos.file_index = activeconnections[i].needed_file;
                tos.key = '\0';
                tos.block_index = 0;
                tos.peerid = activeconnections[i].peerid;
                //cout << "I am inside loop\n";
                if (ch)
                {
                    char *temp = (char*) malloc(FILE_LENGTH * sizeof (char));
                    memset(temp, '\0', FILE_LENGTH);

                    char *tempbuf = (char*) malloc(sizeof (char) *FILE_LENGTH);
                    memset(tempbuf, '\0', FILE_LENGTH);

                    generate_file(current_file_asked_by_client, FILE_LENGTH, temp);

                    for (int j = 0; j < FILE_LENGTH; j++)
                    {
                        tempbuf[j] = temp[j] ^ ch;
                    }
                    memcpy(tos.thisfile, tempbuf, FILE_LENGTH);
                    free(temp);
                    free(tempbuf);
                }
                else
                {
                    //  cout << "I am here for copying file\n";
                    char *tempbuf = (char*) malloc(sizeof (char) *FILE_LENGTH);
                    memset(tempbuf, '\0', FILE_LENGTH);
                    generate_file(current_file_asked_by_client, FILE_LENGTH, tempbuf);
                    memcpy(tos.thisfile, tempbuf, FILE_LENGTH);
                    free(tempbuf);
                    // memcpy(tos.thisfile, current_asking_file, FILE_LENGTH);
                }
                /**************************************************************/
                // cout << "I am before mutex\n";
                mute_fun(tos);
                //cout << "I am after the mutex\n";
                /**************************************************************/
            }
        }
    }
}

void mute_fun(struct filetosend &ft)
{
    pthread_mutex_lock(&mtx);
    outfiles.push_back(ft);
    pthread_mutex_unlock(&mtx);
}

void add_one_file_block(char* buf, char *file_buf)
{
    // todo: add your code here.
    // memcpy(buf,file_buf,1000);
}

bool got_entire_file(void)
{
    // todo: add your code here.
}

bool check_sock(int id)
{
    int res = find_socket(id);

    if (res == -1)
    {
        return 0;
    }
    else
    {
        return 1;
    }

}

void *thread_sending_file(void *arg)
{
    cout << "Thread is running***\n";

    while (1)
    {
        fflush(stdout);

        while (outfiles.size() > 0)
        {
            for (int i = 0; i < outfiles.size(); i++)
            {
                if (outfiles[i].blocksent < 20)
                {
                    if (check_sock(outfiles[i].peerid))
                    {
                        char *temp = (char*) malloc(sizeof (char) *1000);
                        memset(temp, '\0', 1000);
                        memcpy(temp, outfiles[i].thisfile + outfiles[i].block_index, 1000);
                        send_a_file_block_to_peer(temp, outfiles[i].peerid);
                        outfiles[i].block_index = outfiles[i].block_index + 1000;
                        printf("Block %d sent to client %d\n", outfiles[i].blocksent, outfiles[i].peerid);
                        outfiles[i].blocksent++;
                        free(temp);
                        sleep(1);

                    }
                    else
                    {
                        outfiles.erase(outfiles.begin() + i);
                        cout << "Transfer interrupted: client hung up" << endl;
                    }
                }
                else
                {
                    // cout << "I am here for the erase\n";
                    cout << "File transfer complete.!!\n";
                    outfiles[i].done = 1;
                    vector<filetosend>::iterator itr;
                    for (itr = outfiles.begin(); itr != outfiles.end(); ++itr)
                    {
                        if ((*itr).done == 1)
                        {
                            break;
                        }
                    }
                    if (itr != outfiles.end())
                    {
                        outfiles.erase(itr);
                    }
                }
            }
        }
    }
}

/*******************************************************************************/
void read_from_activesockets(void)
{
    struct sockaddr_in remoteaddr; // peer address
    struct sockaddr_in server_addr; // peer address
    int newfd; // newly accept()ed socket descriptor
    char buf[MAXBUFLEN]; // buffer for client data
    int nbytes;
    int yes = 1; // for setsockopt() SO_REUSEADDR, below
    socklen_t addrlen;
    int i, j;


    if ((listener != -1) && FD_ISSET(listener, &read_fds))
    {
        // todo: add your code here.
        /******************************************************************/
        addrlen = sizeof (remoteaddr);
        if ((newfd = accept(listener, (struct sockaddr *) &remoteaddr, &addrlen)) < 0)
        {
            printf("Error while accepting the client \n");
        }
        else
        {
            FD_SET(newfd, &master); // add to master set
            if (newfd > highestsocket)
            {
                highestsocket = newfd;
            }
            struct sockaddr_in my_addr;
            int len = sizeof (my_addr);
            getpeername(newfd, (struct sockaddr *) &my_addr, (socklen_t *) & len);

            printf("Connected to client \n");
            printf("Clients IP address is %s, port number is %d\n", inet_ntoa(my_addr.sin_addr), ntohs(my_addr.sin_port));
            Connection newconn(newfd, inet_ntoa(remoteaddr.sin_addr), ntohs(remoteaddr.sin_port));
            activeconnections.push_back(newconn);
        }
    }
    else if (FD_ISSET(fileno(stdin), &read_fds))
    {
        process_stdin_message();
    }
    else if ((serversockfd != -1) && FD_ISSET(serversockfd, &read_fds))
    {
        nbytes = recv(serversockfd, buf, MAXBUFLEN, 0);
        // handle server response or data         
        if (nbytes <= 0)
        {
            // got error or connection closed by client
            if (nbytes == 0)
            {
                printf("Server hung up with me. I will also quit.\n");
                exit(0);
            }
            else
            {
                printf("client %d recv error from server \n", MYPEERID);
            }
            close(serversockfd); // bye!
            FD_CLR(serversockfd, &master); // remove from master set
            serversockfd = -1;
        }
        else
        {
            memcpy(server_sock_buf + server_sock_buf_byte_counter, buf, nbytes);
            server_sock_buf_byte_counter += nbytes;

            int type = server_sock_buf[0];
            int num_to_read = get_num_to_read(type);

            while (num_to_read <= server_sock_buf_byte_counter)
            {
                if (type == PACKET_TYPE_CONTROL)
                {
                    Packet* packet = (Packet*) (server_sock_buf + 1);
                    process_server_message(packet);
                }
                else
                {
                    // todo: add your code here.
                }

                remove_read_from_buf(server_sock_buf, num_to_read);
                server_sock_buf_byte_counter -= num_to_read;

                if (server_sock_buf_byte_counter == 0)
                    break;

                type = server_sock_buf[0];
                num_to_read = get_num_to_read(type);
            }
        }
    }
        // todo: add your code here to handle other clients 
        /******************************************************************************/
        /******************************************************************************/
    else
    {
        for (int i = 0; i < activeconnections.size(); i++)
        {
            if (FD_ISSET(activeconnections[i].socket, &read_fds))
            {
                nbytes = recv(activeconnections[i].socket, buf, MAXBUFLEN, 0);
                if (nbytes <= 0)
                {
                    // got error or connection closed by client
                    if (nbytes == 0)
                    {
                        // connection closed
                        printf("Socket %d client %d hung up\n", activeconnections[i].socket, activeconnections[i].peerid);
                    }
                    else
                    {
                        printf("server recv error\n");
                    }
                    memset(current_asking_file, '\0', FILE_LENGTH);
                    current_asking_file_next_location = 0;
                    close(activeconnections[i].socket); // bye!
                    FD_CLR(activeconnections[i].socket, &master); // remove from master set
                    deleteconnection(activeconnections[i].peerid);
                }
                else
                {
                    memcpy(activeconnections[i].sock_buf + activeconnections[i].sock_buf_byte_counter, buf, nbytes);
                    activeconnections[i].sock_buf_byte_counter += nbytes;

                    int type = activeconnections[i].sock_buf[0];
                    int num_to_read = get_num_to_read(type);

                    while (num_to_read <= activeconnections[i].sock_buf_byte_counter)
                    {
                        if (type == PACKET_TYPE_CONTROL)
                        {
                            Packet* packet = (Packet*) (activeconnections[i].sock_buf + 1);
                            if (activeconnections[i].peerid == -1)
                            {
                                activeconnections[i].peerid = packet->sender;
                            }
                            process_peer_message(packet);
                        }
                        else if (type == PACKET_TYPE_DATA)
                        {
                            cout << "File packet received\n";
                            int flag = 0;
                            memcpy(current_asking_file + current_asking_file_next_location, activeconnections[i].sock_buf + 1, nbytes - 1);
                            current_asking_file_next_location += 1000;
                            activeconnections[i].current_block_count++;
                            // cout << "Count is " << activeconnections[i].f_count << endl;
                            if (activeconnections[i].current_block_count == 20)
                            {
                                cout << "File complete.Lets check its hash!!\n";
                                unsigned char temphash[20];
                                find_file_hash(FILE_LENGTH, current_asking_file, temphash);

                                print_hash(temphash);
                                print_hash(current_asking_file_hash);

                                for (int j = 0; j < 20; j++)
                                {
                                    if (temphash[j] != current_asking_file_hash[j])
                                    {
                                        flag = 1;
                                    }
                                }
                                if (flag)
                                {
                                    cout << "Incorrect file!!\n";
                                    memset(current_asking_file, '\0', FILE_LENGTH);
                                    current_asking_file_next_location = 0;
                                    activeconnections[i].current_block_count = 0;
                                    sleep(2);
                                    send_req_file_to_peer(MYPEERID, activeconnections[i].peerid, current_asking_file_index);
                                }
                                else
                                {
                                    cout << "Correct file\n";
                                    sleep(2);
                                    send_got_file_to_server(MYPEERID, current_asking_file_index);
                                    memset(current_asking_file, '\0', FILE_LENGTH);
                                    current_asking_file_next_location = 0;
                                    activeconnections[i].current_block_count = 0;
                                    FILE_VECTOR[current_asking_file_index] = '1';
                                    close(activeconnections[i].socket); // bye!
                                    FD_CLR(activeconnections[i].socket, &master); // remove from master set
                                    deleteconnection(activeconnections[i].peerid);
                                }
                            }
                        }
                        remove_read_from_buf(activeconnections[i].sock_buf, num_to_read);
                        activeconnections[i].sock_buf_byte_counter -= num_to_read;

                        if (activeconnections[i].sock_buf_byte_counter == 0)
                            break;

                        type = activeconnections[i].sock_buf[0];
                        num_to_read = get_num_to_read(type);
                    }
                }
            }
        }
    }
}

void client_run(void)
{
    // listen on peer connections on some port
    struct sockaddr_in remoteaddr; // peer address
    struct sockaddr_in server_addr; // peer address
    int yes = 1; // for setsockopt() SO_REUSEADDR, below
    socklen_t addrlen;
    int i, j;

    FD_ZERO(&master); // clear the master and temp sets
    FD_ZERO(&read_fds);

    highestsocket = 0;

    struct timeval timeout;

    serversockfd = connect_to_server(SERVERIP, SERVERPORT);

    send_register_to_server(MYPEERID);

    FD_SET(fileno(stdin), &master);

    if (fileno(stdin) > highestsocket)
    {
        highestsocket = fileno(stdin);
    }

    // todo: add your code here for the listening socket
    /***************************************************/
    if ((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("cannot create a socket");
        fflush(stdout);
        exit(1);
    }
    // lose the pesky "address already in use" error message
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (int)) == -1)
    {
        printf("setsockopt");
        fflush(stdout);
        exit(1);
    }
    // bind to the port
    bzero(&remoteaddr, sizeof (remoteaddr));
    remoteaddr.sin_family = AF_INET;
    remoteaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    remoteaddr.sin_port = htons(MY_LISTEN_PORT);

    if (bind(listener, (struct sockaddr *) &remoteaddr, sizeof (remoteaddr)) == -1)
    {
        printf("could not bind to MYPORT");
        fflush(stdout);
        exit(1);
    }
    // listen
    if (listen(listener, 40) == -1)
    {
        printf("too many backlogged connections on listen");
        fflush(stdout);
        exit(1);
    }
    cout << "Listener is working and has value " << listener << endl;
    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    if (listener > highestsocket)
    {
        highestsocket = listener;
    }
    /******************************************************************/

    // main loop
    while (1)
    {
        read_fds = master;
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;

        if (select(highestsocket + 1, &read_fds, NULL, NULL, &timeout) == -1)
        {
            if (errno == EINTR)
            {
                printf("Select for client %d interrupted by interrupt...\n", MYPEERID);
            }
            else
            {
                printf("Select problem .. client %d exiting iteration\n", MYPEERID);
                fflush(stdout);
                exit(1);
            }
        }
        else
        {
            read_from_activesockets();
        }
    }
}

int main(int argc, char** argv)
{
    int c, option_index = 0;
    char* configfile;
    outfiles.clear();

    while ((c = getopt_long(argc, argv, "c:s:h:p:d:i:m:", long_options, &option_index)) != EOF)
    {
        switch (c)
        {

        case 'c':
            configfile = optarg;
            read_config(configfile);
            break;
        case 's':
            memcpy(SERVERIP, optarg, strlen(optarg));
            client_got_server_IP();
            break;
        case 'h':
            SERVERPORT = atoi(optarg);
            break;
        case 'p':
            MY_LISTEN_PORT = atoi(optarg);
            break;
        case 'i':
            MYPEERID = atoi(optarg);
            break;

        case 'm':
            memcpy(SERVERNODE, optarg, strlen(optarg));
            client_got_server_name();
            break;

        case 'd':
            // what is the debug value
            int debug = atoi(optarg);
            printf("DEBUG LEVEL IS %d\n", debug);
            break;
        }
    }


    client_init();

    pthread_t tid;
    int res = pthread_create(&tid, NULL, thread_sending_file, NULL);

    if (res == 0)
        cout << "successfully created thread!" << endl;
    else
        cout << "unsuccessful in creating thread!" << endl;

    client_run();

    return 0;
}
