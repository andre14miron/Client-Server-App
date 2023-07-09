# TCP and UDP client-server application for message management

## The description
This project involves implementing a platform with three components: a server, TCP clients, and UDP clients, for message management. The server facilitates communication between clients, allowing message publication and subscription. TCP clients can subscribe, unsubscribe, and display received messages. UDP clients publish messages to the server using a predefined protocol. The application also includes a store-and-forward feature for TCP clients to receive messages when they are offline.

## The structure
The project consists of the following files:

- **server.c**: Implementation of the server.
- **subscriber.c**: Implementation of the TCP client (subscriber).
- **lib.h**: Contains structures for UDP and TCP packets and messages.
- **client.h**: Contains structures for the client and topics.
- **pcom_hw2_udp_client**: Implementation of the UDP client (This folder was not implemented by me but received along with the assignment)

## How to Run
### Run the Server

```bash
./server <PORT>
```
Make sure to specify the port number on which you want to run the server.

### Run a TCP Subscriber
```bash
./subscriber <ID_CLIENT> <IP_SERVER> <PORT_SERVER>
```
Make sure to specify the client ID, server IP address, and server port number.

## The implementation
### lib.h
The protocol is implemented using the udp_packet and tcp_packet structures.
The tcp_packet structure includes the following fields:

- **len**: Size of the message in the packet.
- **type**: Type of the message, classified as follows:
    - Subscribe message
    - Unsubscribe message
    - UDP message
    - Disconnection message
- **message**: The actual message, which can be:
    - msg_subscribe
    - msg_unsubscribe
    - msg_udp

### client.h
This header file is intended for clients and contains important data about them, such as:

- **flag_active**: Connection status.
- **id**: Client ID.
- **fd**: File descriptor.
- **topics_size**: Number of subscribed topics.
- **topics**: Array of topic structures, storing topic names and store-and-forward flags.
- **inbox_size**: Number of unread messages.
- **inbox**: Array of tcp_packet structures, storing the packets awaiting transmission.

### Server
The server opens sockets for UDP, TCP, and STDIN connections, and poll is used to handle open descriptors. In an infinite loop, the server waits for various notifications. Depending on the source of the notification, the following actions take place:

1. **STDIN**: Only the "exit" command is expected to shut down the server. When received, an "exit" message packet is sent to all connected clients, and the server and clients are closed gracefully.
2. **TCP**: The connection with the client is accepted and checked for its status. If it is the client's first connection, it is added to the client database. If it is not the first connection and the client has subscribed to topics and requested not to lose any messages, all unread messages are sent from the queue to the client.
3. **UDP**: The UDP message is received and parsed according to the specified format. It is then encapsulated in a TCP packet along with the sender's IP address and port. Before sending these new TCP packets, the clients to whom they are intended are identified.
4. **Client**: A tcp_packet is received, and the following checks are performed:
    1. If the client has disconnected, the connection flag is set to zero.
    2. If the client wants to subscribe to a topic, a subscription message is sent.
    3. If the client wants to unsubscribe from a topic, an unsubscription message is sent.
    4. If none of the above conditions are met, an invalid packet is received.

The following functions are implemented for working with the client database:
- **find_client_by_id**: Searches for a client by ID.
- **find_client_by_fd**: Searches for a client by file descriptor.
- **find_topic**: Checks if a client is subscribed to a topic.
- **add_client**: Adds a client to the database.

### Subscriber
The subscriber opens sockets for TCP and STDIN connections, and poll is used to handle open descriptors. In an infinite loop, the subscriber waits for various notifications. Depending on the source of the notification, the following actions take place:

1. **STDIN**: The following commands are expected from Standard Input:

    - exit: Disconnects from the server.
    - subscribe: Subscribes to a topic.
    - unsubscribe: Unsubscribes from a topic.

For the last two commands, a tcp_packet structure is created with the appropriate type.

2. **TCP**: The subscriber receives either an "exit" command indicating that the server is shutting down or a packet with a message from a topic. In the latter case, the data is parsed and stored in the message field of the packet structure, depending on the payload type.