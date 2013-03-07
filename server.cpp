#include "common.h"
#include "server.h"

int SERVERPORT = 5000;

class Connection
{
public:
    int socket;
    char peerip[MAXIPLEN];
    int peerport;
    int peerid;
    int peer_listen_port;
    char FILE_VECTOR[FILE_NUMBER];
    char sock_buf[MY_SOCK_BUFFER_LEN];
    int sock_buf_byte_counter;

    Connection()
    {
        socket = -1;
        peerport = -1;
        sprintf(peerip, "%s", "0.0.0.0");
        peerid = -1;
        peer_listen_port = -1;
        sock_buf_byte_counter = 0;
    }

    Connection(int sock, const char* ip, int port)
    {
        socket = sock;
        peerport = port;
        sprintf(peerip, "%s", ip);
        peerid = -1;
        peer_listen_port = -1;
        sock_buf_byte_counter = 0;
    }
};

int init_quit = 0;
vector<Connection> activeconnections;
set<int> peerstodelete;

unsigned char file_hash[FILE_NUMBER][20];

void get_file_hash(void)
{
    char buf[FILE_LENGTH];
    for (int i = 0; i < FILE_NUMBER; i++)
    {
        generate_file(i, FILE_LENGTH, buf);
        find_file_hash(FILE_LENGTH, buf, &(file_hash[i][0]));
    }
}

struct option long_options[] = {
    {"serverport", required_argument, NULL, 'p'},
    {"debug", required_argument, NULL, 'd'},
    {"config", required_argument, NULL, 'c'},
    { 0, 0, 0, 0}
};


// master file descriptor list
fd_set master;
fd_set read_fds; // temp file descriptor list for select()
// listening socket descriptor for p2p connections
int listener;
// socket for getting server responses
int serversockfd;
// highest file descriptor seen so far
int highestsocket;

void read_config(const char*);
void read_from_activesockets();

void read_config(const char* configfile)
{
    FILE* f = fopen(configfile, "r");
    if (f)
    {
        fscanf(f, "SERVERPORT %d\n", &SERVERPORT);
        fclose(f);
    }
    else
    {
        printf("cannot open config file!\n");
        fflush(stdout);
        exit(1);
    }
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

void send_packet_to_client(Packet *packet, int clientid)
{
    int sockettosend = find_socket(clientid);
    int send_result = send_a_control_packet_to_socket(sockettosend, packet);

    if (send_result == -1)
    {
        printf("Oh! Cannot send packet to client %d!\n", clientid);
        if (errno == EPIPE)
        {
            printf("Trouble sending data on socket %d to client %d with EPIPE .. CLOSING HIS CONNECTION\n", sockettosend, clientid);
            //printf("Trouble sending data on socket %d to client %d with EPIPE .. CLOSING HIS CONNECTION\n", socket2send, event->recipient);
            //connection_close(event->recipient);
        }
    }
    else
    {
        //printf("Ha! Sent packet to client %d!\n", clientid);
        //printpacket((char*) packet, PACKET_SIZE);            
    }
}

int send_quit_to_clients(void)
{
    for (int i = 0; i < activeconnections.size(); i++)
    {
        int clientid = activeconnections[i].peerid;

        Packet packet;
        packet.sender = SERVER_NODE_ID;
        packet.recipient = clientid;
        packet.event_type = EVENT_TYPE_SERVER_QUIT;

        send_packet_to_client(&packet, clientid);
    }
    init_quit = 1;
}

void give_peer_to_client(int clientid, int peer_to_give, int file_index)
{
    Packet packet;

    packet.sender = SERVER_NODE_ID;
    packet.recipient = clientid;
    packet.event_type = EVENT_TYPE_SERVER_REPLY_REQ_FILE;

    for (int i = 0; i < activeconnections.size(); i++)
    {
        if (activeconnections[i].peerid == peer_to_give)
        {
            memcpy(packet.peerip, activeconnections[i].peerip, MAXIPLEN);
            packet.peerid = activeconnections[i].peerid;
            packet.peer_listen_port = activeconnections[i].peer_listen_port;
            memcpy(packet.hash, &(file_hash[file_index][0]), 20);
            break;
        }
    }

    send_packet_to_client(&packet, clientid);
}

void server_event_handler(Packet* packet)
{
    if (packet->event_type == EVENT_TYPE_CLIENT_REGISTER)
    {
        int clientid = packet->sender;
        int listen_port = packet->port_number;
        printf("server got new client %d, listen port_number %d\n", clientid, listen_port);

        for (int i = 0; i < activeconnections.size(); i++)
        {
            if (activeconnections[i].peerid == clientid)
            {
                activeconnections[i].peer_listen_port = listen_port;
                memcpy(activeconnections[i].FILE_VECTOR, packet->FILE_VECTOR, FILE_NUMBER);
                break;
            }
        }
    }
    else if (packet->event_type == EVENT_TYPE_CLIENT_REQ_FILE)
    {
        int clientid = packet->sender;
        int file_index = packet->req_file_index;
        printf("server got req from client %d for file %d\n", clientid, file_index);

        int peer_to_give = -1;
        vector<int>temp;

        for (int i = 0; i < activeconnections.size(); i++)
        {
            // todo: add your code here. Implement the search for peer function. 
            for (int j = 0; j < 64; j++)
            {
                if ((activeconnections[i].FILE_VECTOR[j] == '1') && (j == file_index))
                {
                    if (activeconnections[i].peerid > clientid)
                    {
                        peer_to_give = activeconnections[i].peerid;
                        break;
                    }
                    else
                    {
                        temp.push_back(activeconnections[i].peerid);
                    }
                }
            }
            if (peer_to_give != -1)
            {
                break;
            }
        }
        if ((!temp.empty()) && (peer_to_give == -1))
        {
            cout << " I am here for sort\n";
            sort(temp.begin(), temp.end());
            peer_to_give = temp[0];
        }
        cout << "Size ==>" << temp.size() << endl;
        if (peer_to_give != -1)
        {
            printf("server tells client %d to get file %d from client %d\n", clientid, file_index, peer_to_give);
            give_peer_to_client(clientid, peer_to_give, file_index);
            temp.clear();
            peer_to_give = -1;
        }
        else
            printf("Oops! No client has file %d\n", file_index);

    }
    else if (packet->event_type == EVENT_TYPE_CLIENT_QUIT)
    {
        printf("Client %d wants to quit!\n", packet->sender);
        FD_CLR(find_socket(packet->sender), &master); // remove from master set
        peerstodelete.insert(packet->sender);
    }
    else if (packet->event_type == EVENT_TYPE_CLIENT_GOT_FILE)
    {
        // todo: add your code here.
        printf("Client %d has received the file %d \n", packet->sender, packet->req_file_index);
        int id = find_socket(packet->sender);
        for (int i = 0; i < activeconnections.size(); i++)
        {
            if (id == activeconnections[i].socket)
            {
                activeconnections[i].FILE_VECTOR[packet->req_file_index] = '1';
                break;
            }
        }
    }
}
bool myfun(Connection const &c1,Connection const &c2)
{
    if(c1.peerid != c2.peerid)
    return c1.peerid < c2.peerid;        
}
void read_from_activesockets(void)
{
    struct sockaddr_in myaddr; // my address
    struct sockaddr_in remoteaddr; // peer address
    struct sockaddr_in server_addr; // peer address
    int newfd; // newly accept()ed socket descriptor
    char buf[MAXBUFLEN]; // buffer for client data
    int nbytes;
    int yes = 1; // for setsockopt() SO_REUSEADDR, below
    socklen_t addrlen;
    int i, j;


    if (FD_ISSET(listener, &read_fds))
    {
        // handle new connections
        addrlen = sizeof (remoteaddr);
        if ((newfd = accept(listener, (struct sockaddr *) &remoteaddr, &addrlen)) == -1)
        {
            printf("Trouble accepting a new connection");
        }
        else
        {
            FD_SET(newfd, &master); // add to master set
            if (newfd > highestsocket)
            {
                highestsocket = newfd;
            }

            printf("New connection from %s port number %d on socket %d\n", inet_ntoa(remoteaddr.sin_addr), ntohs(remoteaddr.sin_port), newfd);

            Connection newconn(newfd, inet_ntoa(remoteaddr.sin_addr), ntohs(remoteaddr.sin_port));
            activeconnections.push_back(newconn);
            sort(activeconnections.begin(),activeconnections.end(),&myfun);
        }
    }
    else if (FD_ISSET(fileno(stdin), &read_fds))
    {
        char c = getchar();
        if (c == 'q')
        {
            send_quit_to_clients();
        }
    }
    else
    {
        // run through the existing connections looking for data to read
        peerstodelete.clear();
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

                    close(activeconnections[i].socket); // bye!
                    FD_CLR(activeconnections[i].socket, &master); // remove from master set
                    peerstodelete.insert(activeconnections[i].peerid);
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
                            server_event_handler(packet);
                        }
                        else
                        {
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

        for (set<int>::iterator iter = peerstodelete.begin(); iter != peerstodelete.end(); iter++)
        {
            deleteconnection(*iter);
        }

        if ((init_quit) && (activeconnections.size() == 0))
        {
            printf("All clients quit. Server quitting.\n");
            exit(0);
        }
    }
}

void server_run()
{
    struct sockaddr_in myaddr; // my address
    struct sockaddr_in remoteaddr; // peer address
    struct sockaddr_in server_addr; // peer address
    int yes = 1; // for setsockopt() SO_REUSEADDR, below
    socklen_t addrlen;
    int i, j;

    get_file_hash();

    FD_ZERO(&master); // clear the master and temp sets
    FD_ZERO(&read_fds);
    // get the listener

    if ((listener = socket(PF_INET, SOCK_STREAM, 0)) == -1)
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
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = htons(SERVERPORT);
    memset(&(myaddr.sin_zero), '\0', 8);
    if (bind(listener, (struct sockaddr *) &myaddr, sizeof (myaddr)) == -1)
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

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    if (listener > highestsocket)
    {
        highestsocket = listener;
    }

    FD_SET(fileno(stdin), &master);

    if (fileno(stdin) > highestsocket)
    {
        highestsocket = fileno(stdin);
    }

    struct timeval timeout;

    // main loop
    while (1)
    {
        read_fds = master;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        if (select(highestsocket + 1, &read_fds, NULL, NULL, &timeout) == -1)
        {
            if (errno == EINTR)
            {
                cout << "got the EINTR error in select" << endl;
            }
            else
            {
                cout << "select problem, server got errno " << errno << endl;
                printf("Select problem .. exiting server");
                fflush(stdout);
                exit(1);
            }
        }

        read_from_activesockets();
    }

}

int main(int argc, char** argv)
{
    int c, option_index = 0;
    char* configfile;

    while ((c = getopt_long(argc, argv, "c:p:d:", long_options, &option_index)) != EOF)
    {
        switch (c)
        {
        case 'c':
            // configfile
            configfile = optarg;
            read_config(configfile);
            break;
        case 'p':
            // my port
            SERVERPORT = atoi(optarg);
            break;
        case 'd':
            // what is the debug value
            int debug = atoi(optarg);
            cout << "DEBUG LEVEL IS " << debug << endl;
            break;
        }
    }

    printf("server running!\n");
    server_run();

    return 0;

}
