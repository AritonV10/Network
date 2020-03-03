#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>


#define LOG(type, fmt, ...) { \
    printf("[" type "]: " fmt, __VA_ARGS__); \
}

#define SIZEu8       (sizeof(uint8_t))
#define SIZEu8_BITS  (SIZEu8 * CHAR_BIT)

#define SIZEu16      (sizeof(uint16_t))
#define SIZEu16_BITS (SIZEu16 * CHAR_BIT)


typedef int sock_t;




void *
client_thread(void *);

void *
client_read_thread(void *);

/* Converts a uint8_t to Network Byte Order (Big Endian) */
static inline uint8_t
u8_port_to_nbo(uint8_t);


/* Converts a uin16_t to Network Byte Order (Big Endian) */
uint16_t
u16_port_to_nbo(uint16_t);


/* Converts an IP address from a string to a unsigned 32-bit integer */
uint32_t
ipv4_to_nbo(const char * __restrict);

/* Converts a string to a unsigned 8-bit integer */
uint8_t
u8_stoi(const uint8_t * __restrict);

/* Packs 4 unsigned 8-bit integers into a 32-bit unsigned integer. The left-most byte is the first byte in the string */
uint32_t
u32_pack(uint8_t[4]);

void
die(const char *)__attribute__((noreturn));


volatile uint8_t flag = 1;

int main(void) {

    pthread_t thread;
    client_thread(NULL);
    pthread_create(&thread, NULL, &client_read_thread, NULL);

    return 0;
}


void *
client_thread(void *ptr) {

    uint8_t  name[64];
    uint8_t  input_buf[1024];
    int8_t   ch;


    uint16_t buf_index;

    sock_t sd;

    pthread_t read_thread;
    int32_t  err;

    struct sockaddr_in  addrin;

    name[0]                = 0;
    addrin.sin_family      = AF_INET;
    addrin.sin_port        = u16_port_to_nbo(1330);
    addrin.sin_addr.s_addr = ipv4_to_nbo("0.0.0.0");

    /* TCP/IP */
    sd = socket(AF_INET, SOCK_STREAM, 0);

    if(sd == 0)
        die("[ERROR]: Couldn't create sock_t");

    err = connect(sd, (struct sockaddr *)&addrin, sizeof(addrin));

    if(err != 0)
        die("[ERROR]: Couldn't connect to the server");

    pthread_create(&read_thread, NULL, &client_read_thread, (void*)&sd);

    while(1) {

        while((ch = fgetc(stdin)) != EOF) {

            if (ch == '\n') {


                input_buf[buf_index]   = 0;

                send(sd, input_buf, buf_index, 0);

                buf_index = 0;

            } else if(buf_index >= 1024) {

                printf("\n%s\n", "[Warning]: Message too long");

            } else {

                input_buf[buf_index++] = ch;

            }
        }

    }

    return(NULL);
}

void *
client_read_thread(void *sock) {


    uint8_t buf[1024];

    uint32_t len;

    sock_t sd = *((sock_t*)sock);

    while(flag) {

        len = read(sd, buf, 1024);

        buf[len] = 0;

        printf("%s", buf);
    }

    return(NULL);
}


uint32_t
ipv4_to_nbo(const char * __restrict ip) {

    uint8_t buf[4];
    uint8_t bytes[4];

    uint8_t buf_index   = 0;
    uint8_t bytes_index = 0;

    for(; ; ++ip) {

        if(*ip == '.' || *ip == 0) {

            buf[buf_index] = 0;

            /* convert the string to a unsigned 8-bit integer */
            bytes[bytes_index++] = u8_stoi(buf);

            buf_index = 0;

            if(*ip == 0)
                break;

        } else {

            buf[buf_index++] = *ip;

        }

    }

    /* pack the 4 integers into a unsigned 32-bit integer */
    return(u32_pack(bytes));

}

uint8_t
u8_stoi(const uint8_t * __restrict integer) {

    uint8_t buf;

    buf = 0;

    for(; *integer != 0; ++integer)
        buf = (buf * 10) + (*integer & 0x0F);

    return(buf);
}


uint16_t
u16_port_to_nbo(uint16_t port) {

    static const volatile uint32_t endianess = 0x01234567;
    uint16_t fixed_port;


    /* check for endianness */
    if(*((uint8_t*)&endianess) == 0x67)
        return(port);

    fixed_port = 0;

    fixed_port |= ( ((uint16_t) u8_port_to_nbo((uint8_t)(port & 0x00FF))) << 8);

    fixed_port |= ( ((uint16_t) u8_port_to_nbo((uint8_t)(port >> 8))));

    return(port);

}


/* Converts the port to a unsigned 8-bit network order byte form (big endian) */
static inline uint8_t
u8_port_to_nbo(uint8_t port) {

    uint8_t pport;

    pport = 0;

    pport = (pport | ((((port & 0x01) > 0) << (SIZEu8_BITS - 1))));
    pport = (pport | ((((port & 0x02) > 0) << (SIZEu8_BITS - 2))));
    pport = (pport | ((((port & 0x04) > 0) << (SIZEu8_BITS - 3))));
    pport = (pport | ((((port & 0x08) > 0) << (SIZEu8_BITS - 4))));
    pport = (pport | ((((port & 0x10) > 0) << (SIZEu8_BITS - 5))));
    pport = (pport | ((((port & 0x20) > 0) << (SIZEu8_BITS - 6))));
    pport = (pport | ((((port & 0x40) > 0) << (SIZEu8_BITS - 7))));
    pport = (pport | ((((port & 0x80) > 0) << (SIZEu8_BITS - 8))));

    return(pport);
}


uint32_t
u32_pack(uint8_t ip[4]) {
    return(
        (uint32_t)(ip[0] << 24)   |
        ((uint32_t)(ip[1] << 16)) |
        ((uint32_t)(ip[2] << 8))  |
        ((uint32_t)(ip[3]))
    );
}

void
die(const char *msg) {

    printf("%s", msg);

    exit(1);
}


