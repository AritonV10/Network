#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <poll.h>
#include <sys/time.h>

#define LOG(type, fmt, ...) { \
    printf("[" type "]: " fmt, __VA_ARGS__); \
}

#define ASSERT(cond) \
struct _assert { \
    int8_t b[!(cond) * -1] \
};

#define MAX_CONNECTIONS (0x0F)
#define MAX_CLIENTS     (0x64)

#define SIZEu8       (sizeof(uint8_t))
#define SIZEu8_BITS  (SIZEu8 * CHAR_BIT)

#define SIZEu16      (sizeof(uint16_t))
#define SIZEu16_BITS (SIZEu16 * CHAR_BIT)

#define BIT(a, c) (((a) & (1u << (c))) == 0 ? 0 : 1)

#define ENTER_CRITICAL(c) pthread_mutex_lock(&(c));
#define LEAVE_CRITICAL(c) pthread_mutex_unlock(&(c));


typedef int sock_t;

typedef struct _node {

    sock_t client;

    struct _node *next;

} node_t;

typedef struct _queue {

    uint16_t size;

    pthread_mutex_t lock;

    pthread_cond_t condition;

    node_t *top;

} queue_t;

typedef struct _client {


    /* If the client has a name is set to 1 and the client is able to receive broadcasted messages */
    uint8_t  is_ready;

    uint8_t  name[32];

    int8_t   available;

    sock_t   sd;

    struct timeval session;

} client_t;


typedef struct _thread_pool {

    uint8_t  exit_flag;

    uint8_t  nthreads;

    /* Queue used for handling the clients */
    queue_t *queue;

}thread_pool_t;


/*******************************************************************************/
/************************** FUNCTION DECLARATIONS ******************************/


void *
server_thread(void *);

void
start_server(uint16_t, const char * __restrict);


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


/*******************************************************************************/

queue_t *
make_queue(void);


/* Adds a new client in the queue and signals the sleeping threads that are waiting for a client to handle */
uint8_t
queue_put(queue_t *, uint8_t);

/* Removes a client from the queue */
uint8_t
queue_pop(queue_t *);

node_t *
make_node(uint8_t);


/*******************************************************************************/

/* Creates a thread pool of N threads that handle the incoming clients */
thread_pool_t *
make_thread_pool(uint8_t);

/* The function that each thread in the thread pool executes */
void *
handle_task(void *);

/* Adds a new client in the queue */
uint8_t
add_task(thread_pool_t *, uint8_t);



/*******************************************************************************/


void
remove_client(sock_t client);

/* Checks if the client passed as parameter is already connected */
int8_t
is_new_client(sock_t);

/* Searches through the clients array for a client that matches the client passed as parameter and returns a structure
   containing the name of the client and the socket descriptor */
client_t
find_client(sock_t);

/* Adds a new client in the clients array */
uint8_t
new_client(sock_t, const uint8_t *);

uint8_t
is_name_available(uint8_t *);

/* Send a message to all the connected clients */
void
broadcast_message(const uint8_t *);

void
send_message(uint8_t *, sock_t);



/*******************************************************************************/
/*******************************************************************************/


const char *welcome_message = "[Server]: Welcome. Please enter your name: \n";
const char *session_expire  = "\n[Server]: Your session has expired\n";
const char *name_err        = "[Server]: Name already taken. Please choose a new name.\n";


client_t clients[MAX_CLIENTS];
pthread_mutex_t clients_lock;

volatile uint8_t flag = 1;

int main(int argc, char **argv) {

    uint8_t i;

    /* init the clients array */
    for(i = 0; i < MAX_CLIENTS; ++i) {

        clients[i].sd        = -1;
        clients[i].available = 0;
        clients[i].is_ready  = 0;
        clients[i].name[0]   = 0;

    }

    LOG("INFO", "Starting the server\n", 0);

    start_server(1330, "0.0.0.0");

    return 0;
}

void
start_server(uint16_t port, const char * __restrict ip) {

    sock_t sd;
    sock_t client_sd;

    int32_t  err;

    uint32_t client_len;

    int32_t nevents;


    struct sockaddr_in  addrin;
    struct sockaddr_in  client_addr;

    struct pollfd poll_set[MAX_CLIENTS];

    thread_pool_t *pool;


    nevents                = 0;


    addrin.sin_family      = AF_INET;
    addrin.sin_port        = u16_port_to_nbo(1330);
    addrin.sin_addr.s_addr = ipv4_to_nbo(ip);

    pool                   = make_thread_pool(5);

    /* TCP/IP */
    sd = socket(AF_INET, SOCK_STREAM, 0);

    if(sd == 0)
        die("[ERROR]: Couldn't create sock_t\n");

    err = bind(sd, (struct sockaddr *)&addrin, sizeof(addrin));

    if(err != 0)
        die("[ERROR]: Couldn't bind the socket\n");


    err = listen(sd, MAX_CONNECTIONS);

   if(err != 0)
        die("[ERROR]: Failed to listen to the port\n");

    poll_set[0].fd = sd;
    poll_set[0].events = POLLIN;

    ++nevents;

    while(1) {

        int32_t  index;
        uint32_t readlen;

        /* update the events for all the clients */
        poll(poll_set, nevents, -1);

        for(index = 0; index < nevents; ++index) {

            /* check to see if there's activity on that specific connection */
            if(poll_set[index].revents & POLLIN) {

                if(poll_set[index].fd == sd) {

                    client_len         = sizeof(client_addr);
                    client_sd          = accept(sd, (struct sockaddr *)&client_addr, (socklen_t *)&client_len);

                    poll_set[nevents].fd     = client_sd;
                    poll_set[nevents].events = POLLIN;

                    ++nevents;

                    LOG("Server", "A new client has connected\n", NULL);

                    add_task(pool, client_sd);

                } else {

                    client_t client_info;

                    /* get the number of bytes sent by the client */
                    ioctl(poll_set[index].fd, FIONREAD, &readlen);

                    client_info = find_client(poll_set[index].fd);

                    /* if the client has client sent more than 0 bytes then queue the connection to be handled
                       and update the time interval */
                    if(readlen > 0) {

                        gettimeofday(&(client_info.session), NULL);

                        add_task(pool, poll_set[index].fd);

                    } else {

                        struct timeval session;

                        gettimeofday(&session, NULL);

                        /* verify if the session expired */
                        if((session.tv_sec - client_info.session.tv_sec) > 20) {

                            int32_t j;

                            send(poll_set[index].fd, session_expire, strlen(session_expire), 0);

                            close(poll_set[index].fd);

                            poll_set[index].events = 0;

                            remove_client(poll_set[index].fd);

                            /* remove the client from the events */
                            for(j = index; j < nevents; ++j)
                                poll_set[j] = poll_set[j + 1];




                            --nevents;

                        }

                    }

                }
            }
        }

    }
}


void
remove_client(sock_t client_sd) {

    pthread_mutex_lock(&clients_lock);

    uint8_t i;

    for(i = 0; i < MAX_CLIENTS; ++i) {

        if(clients[i].sd == client_sd) {

            clients[i].available = 0;
            clients[i].is_ready  = 0;
            clients[i].sd        = -1;

            pthread_mutex_unlock(&clients_lock);

        }
    }

    pthread_mutex_unlock(&clients_lock);

}

int8_t
is_new_client(sock_t client_sd) {

    pthread_mutex_lock(&clients_lock);

    uint8_t i;

    for(i = 0; i < MAX_CLIENTS; ++i) {

        /* if it matches the client passed as paramter, then return false */
        if(clients[i].sd == client_sd) {
            pthread_mutex_unlock(&clients_lock);
            return(0);

        }
    }

    pthread_mutex_unlock(&clients_lock);

    return(1);
}

uint8_t
new_client(sock_t client_sd, const uint8_t *name) {


    pthread_mutex_lock(&clients_lock);

    uint8_t i;
    uint8_t len;

    len = 0;

    for(i = 0; i < MAX_CLIENTS; ++i) {
        if(clients[i].available == 0) {

            clients[i].sd = client_sd;

            for(; *name != 0; ++name)
                clients[i].name[len++] = *name;

            clients[i].name[len] = 0;
            clients[i].available = 1;
            clients[i].is_ready  = 1;
            gettimeofday(&(clients[i].session), NULL);

            pthread_mutex_unlock(&clients_lock);

            return(1);
        }
    }

    pthread_mutex_unlock(&clients_lock);

    return(0);
}

uint32_t
ipv4_to_nbo(const char * __restrict ip) {

    uint8_t buf[4];
    uint8_t bytes[4];

    uint8_t buf_index   = 0;
    uint8_t bytes_index = 0;

    uint32_t iip        = 0;

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


queue_t *
make_queue(void) {

    queue_t * queue;

    queue = malloc(sizeof(queue_t));

    if(queue != NULL){

        if((pthread_mutex_init(&(queue->lock), NULL)) == 0) {

            if((pthread_cond_init(&(queue->condition), NULL)) == 0) {

                queue->top   = NULL;
                queue->size  = 0;

                return(queue);
            }

            pthread_mutex_destroy(&(queue->lock));
        }

        free((void *) queue);
    }

    return(NULL);
}

uint8_t
queue_put(queue_t *queue, uint8_t data) {

    /* lock the queue */
    pthread_mutex_lock(&(queue->lock));

    node_t *node;

    node = queue->top;

    /* if the queue doesn't contain any nodes it means that the top node must be empty */
    if(queue->top == NULL) {

        queue->top = make_node(data);

    } else {

        while(node->next != NULL)
            node = node->next;

        node->next = make_node(data);

        if(node->next == NULL) {

            pthread_mutex_unlock(&(queue->lock));

            return(0);
        }
    }

    queue->size += 1;


    pthread_mutex_unlock(&(queue->lock));

    /* notify the sleeping threads */
    pthread_cond_signal(&(queue->condition));

    return(1);
}

uint8_t
queue_pop(queue_t *queue) {

    pthread_mutex_lock(&(queue->lock));

    while(1) {

        /* wait until there are clients in the queue */
        pthread_cond_wait(&(queue->condition), &(queue->lock));

        if(queue->size != 0)
            break;
    }

    node_t *node = queue->top;

    if(node == NULL)
        return(0);

    uint8_t client = node->client;

    queue->top = queue->top->next;

    free(node);

    queue->size -= 1;

    pthread_mutex_unlock(&(queue->lock));

    return(client);

    off:
        pthread_mutex_unlock(&(queue->lock));

        return(0);
}

node_t *
make_node(uint8_t client) {

    node_t *node;

    node = malloc(sizeof(node_t));

    if(node != NULL) {

        node->client = client;
        node->next   = NULL;

        return(node);
    }

    return NULL;
}

thread_pool_t *
make_thread_pool(uint8_t nthreads) {


    thread_pool_t *pool;

    if(nthreads <= 0)
        return(NULL);

    pool = malloc(sizeof(thread_pool_t));

    if(pool != NULL) {

        queue_t *queue = make_queue();

        if(queue != NULL){

            uint8_t i;
            pthread_t thread;

            pool->nthreads  = nthreads;
            pool->queue     = queue;
            pool->exit_flag = 1;

            for(i = 0; i < nthreads; ++i)
                pthread_create(&thread, NULL, &handle_task, (void*)pool);


            /* so we can synchronize all the threads */
            sleep(2);
            return(pool);
        }
    }

    free(pool);

    return(NULL);
}

void *
handle_task(void *ptr) {


    uint8_t  name[64];
    uint8_t  input_buf[256];
    uint8_t  msg_buf[512];

    uint32_t len;

    client_t client_info;

    thread_pool_t *pool  = (thread_pool_t*)ptr;
    queue_t       *queue = pool->queue;

    while(pool->exit_flag) {

        /* get a client connection if available */
        sock_t client = queue_pop(queue);

        /* check to see if its a new client */
        if(is_new_client(client)) {

            uint32_t client_index;

            (void) client_index;

            send(client, welcome_message, strlen(welcome_message), 0);

            /* read the clients name */
            len = read(client, name, 64);

            name[len] = 0;

            /* verify that the name is not taken */
            if(!is_name_available(name)) {

                do {

                    send(client, name_err, strlen(name_err), 0);

                    read(client, name, 64);

                }while(!is_name_available(name));
            }

            client_index = new_client(client, name);

            sprintf(msg_buf, "\n%s has joined the room\n", name);

            /* broadcast a message that there's a new client */
            broadcast_message(msg_buf);

            continue;

        }

        /* find information about the client */
        client_info = find_client(client);

        /* read the message that the client has sent */
        len = read(client, input_buf, 256);

        input_buf[len] = 0;

        sprintf(msg_buf, "[%s]: %s\n", client_info.name, input_buf);

        /* send the message to all the connected clients */
        send_message(msg_buf, client);

    }

    return(NULL);
}

uint8_t
add_task(thread_pool_t *pool, uint8_t client) {

    return(queue_put(pool->queue, client));
}

void
broadcast_message(const uint8_t *msg) {

    pthread_mutex_lock(&clients_lock);

    uint8_t i;

    for(i = 0; i < MAX_CLIENTS; ++i) {

        if(clients[i].available == 1 && clients[i].is_ready == 1)
            send(clients[i].sd, msg, strlen(msg), 0);

    }

    pthread_mutex_unlock(&clients_lock);

}


void *
server_thread(void *ptr) {

    start_server(1330, "0.0.0.0");

    return(NULL);
}

client_t
find_client(sock_t client) {

    uint8_t i;

    pthread_mutex_lock(&clients_lock);

    for(i = 0; i < MAX_CLIENTS; ++i) {

        if(clients[i].sd == client) {

            pthread_mutex_unlock(&clients_lock);

            return(clients[i]);
        }

    }

    pthread_mutex_unlock(&clients_lock);
}

void
send_message(uint8_t *msg, sock_t client) {

    uint8_t i;

    ENTER_CRITICAL(clients_lock)

    for(i = 0; i < MAX_CLIENTS; ++i) {

        if(clients[i].available == 1 && clients[i].is_ready == 1 && clients[i].sd != client)
            send(clients[i].sd, msg, strlen(msg), 0);
    }

    LEAVE_CRITICAL(clients_lock)
}

uint8_t
is_name_available(uint8_t *name) {

    ENTER_CRITICAL(clients_lock)

    uint8_t i;

    for(i = 0; i < MAX_CONNECTIONS; ++i) {

        if(strcmp(clients[i].name, (const char *)name) == 0) {
            LEAVE_CRITICAL(clients_lock)
            return(0);
        }
    }

    LEAVE_CRITICAL(clients_lock)
    return(1);

}

void
die(const char *msg) {

    printf("%s", msg);

    exit(1);
}
