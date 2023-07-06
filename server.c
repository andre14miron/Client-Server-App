#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <poll.h>
#include "lib.h"
#include "client.h"
#include <stdlib.h>

/* Return the customer with the requested ID or null if it does not exist */
struct client* find_client_by_id(char id[MAX_ID]) {
    for (int i = 0; i < clients_size; i++) {
        if (strcmp(clients[i].id, id) == 0) {
            return &clients[i];
        }
    }
    return NULL;
}

/* Return the customer with the requested FD or null if it does not exist */
struct client* find_client_by_fd(uint32_t fd) {
    for (int i = 0; i < clients_size; i++) {
        if (clients[i].fd == fd) {
            return &clients[i];
        }
    }
    return NULL;
}

/* Return the index where the searched topic is located, or -1 if it is not found */
int find_topic(struct client *cl, char topic[MAX_TOPIC]){
    for (int i = 0; i < cl->topics_size; i++) {
        if (strcmp(cl->topics[i].name, topic) == 0) {
            return i;
        }
    }

    return -1;
}

/* Add a client who is connecting for the first time to the database  */
void add_client(char id[MAX_ID], int fd) {
    int size = clients_size;

    clients[size].flag_active = 1;
    memset(&clients[size].id, 0, MAX_ID);
    strcpy(clients[size].id, id);
    clients[size].fd = fd;
    clients[size].topics_size = 0;
    memset(clients[size].topics, 0, sizeof(clients[size].topics));
    clients[size].inbox_size = 0;
    memset(clients[size].inbox, 0, sizeof(clients[size].inbox));

    clients_size++;
}

int main(int argc, char *argv[])
{
    uint16_t port;
    int udp_sockfd, tcp_sockfd;
    struct sockaddr_in server_addr;
    int ret, flag = 1;

    /* Check the number of arguments */
    DIE(argc != 2, "[SERV] Usage: ./server <PORT_SERVER>");

    /* Get the server port */
    ret = sscanf(argv[1], "%hu", &port);
    DIE(ret != 1, "[SERV] Given port is invalid");

    /* Disable buffering */
    ret = setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    DIE(ret < 0, "[SERV] Error while disabling buffering");
    
    /* Create UDP socket */
    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_sockfd < 0, "[SERV] Error while creating UDP socket");

    /* Create TCP socket */
    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_sockfd < 0, "[SERV] Error while creating TCP socket");

    /* Disable the Nagle algorithm */
    ret = setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    DIE (ret < 0, "[SERV] Error while disabling the Nagle algorithm");
    
    /* Set port and IP that we'll be listening for */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    /* Bind to the set port and IP */
    ret = bind(udp_sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr));
    DIE(ret < 0, "[SERV] Couldn't bind to the port");

    ret = bind(tcp_sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr));
    DIE(ret < 0, "[SERV] Couldn't bind to the port");
    
    /* Listen for clients */
    ret = listen(tcp_sockfd, MAX_CONNECTIONS);
	DIE(ret < 0, "[SERV] Error while listening");

    /* Create the poll of descriptors */
    struct pollfd poll_fds[MAX_CONNECTIONS];
    int nfds = 3;

    poll_fds[0].fd = STDIN_FILENO;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = tcp_sockfd;
    poll_fds[1].events = POLLIN;

    poll_fds[2].fd = udp_sockfd;
    poll_fds[2].events = POLLIN;

    int running = 1;
    while (running) {       
        /* Wait for readiness notification */
        poll(poll_fds, nfds, -1);

        /* The notification comes from STDIN */
        if (poll_fds[0].revents & POLLIN) {
            char buffer[1601];
            memset(buffer, 0, 1601);

            /* Get the input */
            fgets(buffer, 1600, stdin);

            /* Server get the "exit" command */
            if (strncmp(buffer, "exit", 4) == 0) {
                running = 0;

                /* Disconnect all active clients */
                for (int j = 0; j < clients_size; j++) {
                    if (clients[j].flag_active == 1) {
                        struct tcp_packet sent_packet;
                        memset(&sent_packet, 0, sizeof(sent_packet));
                        sent_packet.type = 4;

                        ret = send(clients[j].fd, &sent_packet, sizeof(sent_packet), 0);
						DIE(ret < 0, "[SERV] Error while disconnecting a client");

						ret = close(clients[j].fd);
                        DIE (ret < 0, "[SERV] Error while closing Client socket");
                    }
                }
                        
                break;
            }

            DIE(1, "[SERV] The command from STDIN is invalid");
        }

        /* The notification comes from TCP */
        if (poll_fds[1].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);

            /* Accept the connection with the client */
            int client_sockfd = accept(tcp_sockfd, (struct sockaddr *)&client_addr, &client_len);
            DIE(client_sockfd < 0, "[SERV] Error accept");

            /* Disable the Nagle algorithm */
            ret = setsockopt(client_sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
            DIE (ret < 0, "[SERV] Error while disabling the Nagle algorithm");

            /* Get the packet with the ID of the client */
            char client_id[MAX_ID];
            memset(client_id, 0, sizeof(client_id));
            ret = recv(client_sockfd, client_id, MAX_ID, 0);  
            DIE(ret < 0, "[SERV] Error recv");

            /* Search for the client with the received ID */
            struct client *cl = find_client_by_id(client_id);  
                    
            /* Check the status of the client */
            if (cl == NULL) {
                /* The client is connecting for the first time */

                poll_fds[nfds].fd = client_sockfd;
                poll_fds[nfds].events = POLLIN;
                nfds++;

                add_client(client_id, client_sockfd);

                /* Print the message to STDOUT */
                printf("New client %s connected from %s:%hu.\n",
                            client_id, 
                            inet_ntoa(client_addr.sin_addr), 
                            ntohs(client_addr.sin_port));

            } else if (cl->flag_active == 1) {
                /* The client is already connected */

                /* Print the message to STDOUT */
                printf("Client %s already connected.\n", client_id);

                /* Notify the client to close the socket */
                struct tcp_packet sent_packet;
                memset(&sent_packet, 0, sizeof(sent_packet));
                sent_packet.type = 4;

                ret = send(client_sockfd, &sent_packet, sizeof(sent_packet), 0);
				DIE(ret < 0, "[SERV] Error while disconnecting a client");

                ret = close(client_sockfd);
                DIE (ret < 0, "[SERV] Error while closing Client socket");

            } else {
                /* The client is reconnecting */

                /* Print the message to STDOUT */
                printf("New client %s connected from %s:%hu.\n",
                            client_id, 
                            inet_ntoa(client_addr.sin_addr), 
                            ntohs(client_addr.sin_port));

                /* Update the infos about the client */
                cl->flag_active = 1;
                cl->fd = client_sockfd;

                /* Send the packages that were not received during the period of inactivity */
                for (int j = 0; j < cl->inbox_size; j++) {
                    ret = send(cl->fd, &cl->inbox[j], sizeof(struct tcp_packet), 0);
                    DIE(ret < 0, "[CLIENT] Error send messages from inbox");
                }
                memset(clients[clients_size].inbox, 0, sizeof(clients[clients_size].inbox));
                cl->inbox_size = 0;
            }        
            continue;
        }

        /* The notification comes from UDP */
        if (poll_fds[2].revents & POLLIN) {
            struct udp_packet* recv_packet;
            struct tcp_packet sent_packet;
            struct msg_udp message;
            struct sockaddr_in udp_addr;
            socklen_t udp_len = sizeof(udp_addr);
            char buffer[1552];

            memset(buffer, 0, sizeof(struct udp_packet));
            memset(&sent_packet, 0, sizeof(struct tcp_packet));
            memset(&message, 0, sizeof(struct msg_udp));

            /* Get the message from the UDP client */
            ret = recvfrom(udp_sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &udp_addr, &udp_len);
            DIE(ret < 0, "[SERV] Error recv");
            recv_packet = (struct udp_packet *) buffer;

            /* Create the message with the data from udp client */
            strcpy(message.ip, inet_ntoa(udp_addr.sin_addr));
            message.port = ntohs(udp_addr.sin_port);
            memset(message.topic, 0, MAX_TOPIC);
            strcpy(message.topic, recv_packet->topic);
            message.type = recv_packet->type;
            memset(message.payload, 0, MAX_PAYLOAD);
            memcpy(message.payload, recv_packet->payload, sizeof(recv_packet->payload));

            /* Create the packet for the tcp client */ 
            sent_packet.type = 3;
            sent_packet.len = sizeof(struct msg_udp);
            memset(sent_packet.message, 0, sizeof(sent_packet.message));
            memcpy(sent_packet.message, &message, sizeof(message));

            /* Send the packet to clients */
            for (int j = 0; j < clients_size; j++) {
                /* Verify if the client is subscribed to the topic */
                int index = find_topic(&clients[j], recv_packet->topic);
                if (index == -1)
                    continue;

                /* If the client is active, the packet will be sent now */
                if (clients[j].flag_active == 1) {
                    ret = send(clients[j].fd, &sent_packet, sizeof(sent_packet), 0);
                    DIE(ret < 0, "[CLIENT] Error send unsubscribe message");

                /* The client is inactive, but the packet will be sent when they connect */
                } else if (clients[j].topics[index].sf == 1) {
                    uint32_t size = clients[j].inbox_size;
                    clients[j].inbox[size] = sent_packet;
                    clients[j].inbox_size++;
                }
            }
            continue;
        }
 
        /* The notification comes from a connected client */
        for (int i = 3; i < nfds; i++) {
            if (poll_fds[i].revents & POLLIN) {
                struct tcp_packet packet;
                struct client *cl;

                cl = find_client_by_fd(poll_fds[i].fd);
                memset(&packet, 0, sizeof(struct tcp_packet));

                /* Receive the packet */
                ret = recv(poll_fds[i].fd, &packet, sizeof(struct tcp_packet), 0);
                DIE(ret < 0, "[SERV] Error recv");

                /* The client wants to be disconnected */
                if (ret == 0) {
                    /* Print the message to STDOUT */
                    printf("Client %s disconnected.\n", cl->id);
                        
                    /* Update the infos about the client */
                    cl->flag_active = 0;
                    ret = close(cl->fd);
                    DIE (ret < 0, "[SERV] Error while closing Client socket");

                    break;
                }

                /* The client wants to subscribe to a topic */
                if (packet.type == 1) {
                    struct msg_subscribe *message = (struct msg_subscribe*) packet.message;

                    /* Verify if the client is not subscribed to the topic */
                    if (find_topic(cl, message->topic) == -1) {
                        int size = cl->topics_size;
                        memset(cl->topics[size].name, 0, sizeof(cl->topics[size].name));
                        strcpy(cl->topics[size].name, message->topic);
                        cl->topics[size].sf = message->sf;

                        cl->topics_size++;
                    } 

                    break;

                /* The client wants to unsubscribe to a topic */
                } else if (packet.type == 2) {
                    struct msg_unsubscribe *message = (struct msg_unsubscribe*) packet.message;
                    int index = find_topic(cl, message->topic);

                    /* Verify if the client is subscribed to the topic */
                    if (index != -1) {
                        int size = cl->topics_size;
                        for (int j = index + 1; j < size; j++)
                            cl->topics[j-1] = cl->topics[j];

                        cl->topics_size--;   
                    }

                    break;
                } 
                        
                DIE(1, "[SERV] The command from client is invalid");
                break;
            }
        }
    }  

    /* Close the sockets */
    ret = close(udp_sockfd);
    DIE (ret < 0, "[SERV] Error while closing UDP socket");
    ret = close(tcp_sockfd);
    DIE (ret < 0, "[SERV] Error while closing TCP socket");
    
    return 0;
}