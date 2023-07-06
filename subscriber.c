#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <netinet/tcp.h>
#include <math.h>
#include "lib.h"
#include "client.h"

int main(int argc, char *argv[])
{
    int sock_fd;
    struct sockaddr_in server_addr;
    char ID_client[11];
    char IP_server[16];
    uint16_t port;
    int ret;
    int flag = 1;

    /* Check the number of arguments */
    DIE(argc != 4, "[CLIENT] Usage: ./subscriber <ID_CLIENT> <IP_SERVER> <PORT_SERVER>");

    /* Check if the ID_CLIENT and PORT_SERVER are valid*/
    DIE(strlen(argv[1]) > 10, "[CLIENT] Given ID is invalid");
    DIE(atoi(argv[3]) == 0, "[CLIENT] Given port is invalid");

    /* Get the ID_CLIENT, IP_SERVER and PORT_SERVER */
    memset(ID_client, 0, sizeof(ID_client));
    memset(IP_server, 0, sizeof(IP_server)); 

    memcpy(ID_client, argv[1], strlen(argv[1]));  
    memcpy(IP_server, argv[2], strlen(argv[2]));
    port = atoi(argv[3]);

    /* Disable buffering */
    ret = setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    DIE(ret < 0, "[CLIENT] Error while disabling buffering");
    
    /* Create socket */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sock_fd < 0, "[CLIENT] Error while creating TCP socket");

    /* Disable the Nagle algorithm */
    ret = setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    DIE (ret < 0, "[CLIENT] Error while disabling the Nagle algorithm");
    
    /* Set port and IP the same as server-side */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    ret = inet_aton(IP_server, &server_addr.sin_addr);
    DIE(ret < 0, "[CLIENT] Error inet_atton");

    /* Send connection request to server */
    ret = connect(sock_fd, (struct sockaddr*) &server_addr, sizeof(server_addr));
    DIE(ret < 0, "[CLIENT] Unable to connect");

    /* Create the poll of descriptors */
    struct pollfd poll_fds[2];

    poll_fds[0].fd = STDIN_FILENO;
    poll_fds[0].events = POLLIN;
 
    poll_fds[1].fd = sock_fd;
    poll_fds[1].events = POLLIN;

    /* Send the ID to the server */
    ret = send(sock_fd, ID_client, MAX_ID, 0); 
    DIE(ret < 0, "[CLIENT] Unable to send message");

    int running = 1;
    while (running) {  
        /* Wait for readiness notification */
        poll(poll_fds, 2, -1);
 
        /* Get data from server by tcp */
        if ((poll_fds[1].revents & POLLIN) != 0) {
            struct tcp_packet packet;
            struct msg_udp *message;
            
            ret = recv(sock_fd, &packet, sizeof(packet), 0); 
            DIE(ret < 0, "[CLIENT] Error while receiving server's msg");

            /* The server was killed */
            if (ret == 0) {
                break;
            }

            /* The client is disconnecting */
            if (packet.type == 4) {
                break;
            }

            DIE(packet.type != 3, "[CLIENT] Invalid packet from server");

            /* Get the message from udp */
            message = (struct msg_udp *) packet.message;

            /* TYPE INT */
            if (message->type == 0) {
                /* Get the value */
                uint32_t value = ntohl(*((uint32_t*)(message->payload + 1)));

                /* Verify the sign bit */
				if (message->payload[0] == 1) {
                    value *= -1;
				}

                /* Print the message to STDOUT */
                printf("%s:%d - %s - INT - %d\n",
                        message->ip,
						message->port,
                        message->topic,
                        value);

            /* TYPE SHORT_REAL */
            } else if (message->type == 1) {
                /* Get the value */
                double value = ntohs(*(uint16_t*)(message->payload));
            	value /= 100;
                
                /* Print the message to STDOUT */
                printf("%s:%d - %s - SHORT_REAL - %.2f\n",
                        message->ip,
            			message->port,
                        message->topic,
                        value);   

            /* TYPE FLOAT */ 
            } else if (message->type == 2) {
                /* Get the value */
                double value = ntohl(*(uint32_t*)(message->payload + 1));
                uint8_t power = (uint8_t) message->payload[5];
			    value /= pow(10, power);

                /* Print the message to STDOUT */
			    if (message->payload[0] == 1) {
			        value *= -1;
			    }
                 
                /* Print the message to STDOUT */
                printf("%s:%d - %s - FLOAT - %f\n",
                        message->ip,
            			message->port,
                        message->topic,
                        value);

            /* TYPE STRING */
            } else if (message->type == 3) {
                /* Print the message to STDOUT */
                printf("%s:%d - %s - STRING - %s\n",
                        message->ip,
            			message->port,
                        message->topic,
                        message->payload);
            }

        } else if ((poll_fds[0].revents & POLLIN) != 0) {
            char buffer[1601]; 
            memset(&buffer, 0, 1601);

            /* Read command from STDIN */ 
            fgets(buffer, 1600, stdin);

            /* The client is disconnecting */
            if (strncmp(buffer, "exit", 4) == 0) {
                running = 0;
                break;
            }

            /* The client wants to subscribe to a topic */
            if (strncmp(buffer, "subscribe", 9) == 0) {
                struct msg_subscribe message;
                struct tcp_packet sent_packet;

                /* Get the data */
                char command[10];
                sscanf(buffer, "%s %s %d\n", command, message.topic, &message.sf);
                
                /* Create the packet with the subscribe information */ 
                sent_packet.type = 1;
                sent_packet.len = sizeof(struct msg_subscribe);
                memcpy(sent_packet.message, &message, sizeof(struct msg_subscribe));

                /* Send the packet */
                ret = send(sock_fd, &sent_packet, sizeof(sent_packet), 0);
                DIE(ret < 0, "[CLIENT] Error send subscribe message");

                /* Print the message for client to STDOUT */
                printf("Subscribed to topic.\n");

                continue;
            }

            /* The client wants to unsubscribe from a topic */
            if (strncmp(buffer, "unsubscribe", 11) == 0) {
                struct msg_unsubscribe message;
                struct tcp_packet sent_packet;

                /* Get the data */
                char command[12];
                sscanf(buffer, "%s %s\n", command, message.topic);
                
                /* Create the packet with the unsubscribe information */ 
                sent_packet.type = 2;
                sent_packet.len = sizeof(struct msg_unsubscribe);
                memcpy(sent_packet.message, &message, sizeof(struct msg_unsubscribe));

                /* Send the packet */
                ret = send(sock_fd, &sent_packet, sizeof(sent_packet), 0);
                DIE(ret < 0, "[CLIENT] Error send unsubscribe message");

                /* Print the message for client to STDOUT */
                printf("Unsubscribed from topic.\n");
                
                continue;
            }

            DIE(1, "[CLIENT] The command from input is invalid");
        }
    }  
    
    /* Close the socket */
    ret = close(sock_fd);
    DIE (ret < 0, "[SERV] Error closing client socket");
    
    return 0;
}