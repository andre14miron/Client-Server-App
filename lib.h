#ifndef _LIB_H_
#define _LIB_H_

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_TOPIC 50
#define MAX_PAYLOAD 1500
#define MAX_ID 10

#define DIE(assertion, call_description)                                       \
  do {                                                                         \
    if (assertion) {                                                           \
      fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                       \
      perror(call_description);                                                \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)

/* Structure for the messages received from udp clients */
struct udp_packet {
  char topic[MAX_TOPIC];          
  uint8_t type;
  unsigned char payload[MAX_PAYLOAD];
};

/* Structure with data received by UDP client */
struct msg_udp {
  char ip[16];
  uint16_t port;
  char topic[MAX_TOPIC];
  uint8_t type;
  unsigned char payload[MAX_PAYLOAD];
};

/* Structure for the messages for TCP */
/* Type:
   1 -> subscribe to topic
   2 -> unsubscribe from a topic 
   3 -> UDP message
   4 -> exit
*/
struct tcp_packet {
  uint16_t len;   
  uint8_t type;
  char message[sizeof(struct msg_udp)];
};

/* Structure with data for subscribe */
struct msg_subscribe {
  char topic[MAX_TOPIC];
  int sf;
};

/* Structure with data for unsubscribe */
struct msg_unsubscribe {
  char topic[MAX_TOPIC];
};

#endif /* _LIB_H_ */