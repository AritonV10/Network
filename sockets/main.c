#

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_CLIENTS     (0x64)
#define MAX_CONNECTIONS (0x0F)

#define SIZEu8       (sizeof(uint8_t))
#define SIZEu8_BITS  (SIZEu8 * CHAR_BIT)

#define SIZEu16      (sizeof(uint16_t))
#define SIZEu16_BITS (SIZEu16 * CHAR_BIT)

#define BIT(a, c) (((a) & (1u << (c))) == 0 ? 0 : 1)

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
    
    int8_t   available;

    uint8_t  name[128];
    
    sock_t   sd;
    
} client_t;


typedef struct _thread_pool {
    
    uint8_t  exit_flag;
    
    uint8_t  nthreads;
    
    queue_t *queue;
    
}thread_pool_t;


void *
server_thread(void *);
void *
client_thread(void *);
void *
client_read_thread(void *);

void
start_server(uint16_t, const char * __restrict);


static inline uint8_t
u8_port_to_nbo(uint8_t);

uint16_t
u16_port_to_nbo(uint16_t);

uint32_t
ipv4_to_nbo(const char * __restrict);

uint8_t
u8_stoi(const uint8_t * __restrict);

uint32_t
u32_pack(uint8_t[4]);

void
die(const char *)__attribute__((noreturn));


/******************* QUEUE ***********************/

queue_t *
make_queue(void);

uint8_t
queue_put(queue_t *, uint8_t);

uint8_t
queue_pop(queue_t *);

node_t *
make_node(uint8_t);


/**************************************************/

thread_pool_t *
make_thread_pool(uint8_t);

void *
handle_task(void *);

uint8_t
add_task(thread_pool_t *, uint8_t);



/***********************************/

const char *welcome_message = "Welcome. Please enter your name: ";
const uint8_t size          = 35;

/***********************************/

int8_t
is_new_client(sock_t);

uint8_t
new_client(sock_t, const uint8_t *);

void
broadcast_message(const uint8_t *);


client_t clients[MAX_CLIENTS];
pthread_mutex_t clients_lock;

volatile uint8_t flag = 1;

int main(int argc, char **argv) {
    
    
    /*
    uint8_t i;
    
    for(i = 0; i < MAX_CLIENTS; ++i)
        clients[i].available = 0;
    
    pthread_t thread;
    pthread_create(&thread, NULL, &server_thread, NULL);
    
    sleep(3);
    client_thread(NULL);
    */
    
    
    int8_t x;
    uint16_t m = 0x002F;
    uint16_t k;
    
    k = 0;
    
    for(x = 15; x >= 0; --x) 
        printf("%d ", ((m & (1u << x)) == 0 ? 0 : 1));
    
        
    printf("%c", '\n');
    
    k |= ( ((uint16_t) u8_port_to_nbo((uint8_t)(m & 0x00FF))) << 8);

    k |= ( ((uint16_t) u8_port_to_nbo((uint8_t)(m >> 8))));
    
    uint16_t u =
    for(x = 15; x >= 0; --x)
        printf("%d ", ((k & (1u << x)) == 0 ? 0 : 1));
        
        
    uint8_t n;
    
    n = u8_port_to_nbo(0x0F);
    
    
    printf("%c", '\n');
    
    for(x = 7; x >= 0; --x)
        printf("%d", ((n & (1u << x)) == 0 ? 0 : 1));
    
    printf("%c", '\n');
    printf("%u", (unsigned int) n);
    
    return 0;
}


void
start_server(uint16_t port, const char * __restrict ip) {
    
    uint16_t pport;
    uint16_t read_size;
    
    sock_t sd;
    sock_t client_sd;
    
    int32_t  err;
    uint32_t iip;
    
    
    struct sockaddr_in  addrin;
    
    thread_pool_t *pool;
    
    
    addrin.sin_family      = AF_INET;
    addrin.sin_port        = u16_port_to_nbo(pport);
    addrin.sin_addr.s_addr = ipv4_to_nbo(ip);
    
    pool                   = make_thread_pool(5);
    
    /* TCP/IP */
    sd = socket(AF_INET, SOCK_STREAM, 0);
    
    if(sd == 0) 
        die("[ERROR]: Couldn't create sock_t");
    
    err = bind(sd, (struct sockaddr *)&addrin, sizeof(addrin));
    
    if(err != 0)
        die("[ERROR]: Couldn't bind the socket");
   
   
    err = listen(sd, MAX_CONNECTIONS);
    
   if(err != 0)
        die("[ERROR]: Failed to listen to the port");
    

    while(1) {
        
        client_sd = accept(sd, (struct sockaddr *)&addrin, (socklen_t*)&addrin);
        add_task(pool, client_sd);
        
    }
}

int8_t
is_new_client(sock_t client_sd) {
    
    pthread_mutex_lock(&clients_lock);
    
    uint8_t i;
    
    for(i = 0; i < MAX_CLIENTS; ++i) {
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
        
            bytes[bytes_index++] = u8_stoi(buf);
            
            buf_index = 0;
            
            if(*ip == 0)
                break;
        
        } else {
            buf[buf_index++] = *ip;
        }
        
    }
    
    
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
    uint16_t i;
    
    /* check for endianness */
    if(*((uint8_t*)&endianess) == 0x67)
        return(port);
    
    fixed_port = 0;
    
    
    
    /* [0101] => [1010] */
    /* -> maybe unroll the loop */
    
    
    
    /*
    for(i = 0; i < SIZEu16; ++i)
        fixed_port = (fixed_port | (BIT(port, i) << (SIZEu16_BITS - (i + 1))));
        
    */
    
    fixed_port |= ( ((uint16_t) u8_port_to_nbo((uint8_t)(port & 0x00FF))) << 8);
    
    fixed_port |= ( ((uint16_t) u8_port_to_nbo((uint8_t)(port >> 8))));
    
    return(port);
    
}


/* u8 to network byte order */
static inline uint8_t
u8_port_to_nbo(uint8_t port) {
    
    uint8_t pport;
    
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
    
    pthread_mutex_lock(&(queue->lock));
    
    node_t *node;
    
    node = queue->top;

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
    
    printf("Added value: %d\n", data);
    
    pthread_mutex_unlock(&(queue->lock));
    pthread_cond_signal(&(queue->condition));

    return(1);
}

uint8_t
queue_pop(queue_t *queue) {
    
    pthread_mutex_lock(&(queue->lock));
    
    while(1) {
        
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
            
            for(i = 0; i < nthreads; ++i) {
                pthread_create(&thread, NULL, &handle_task, (void*)pool);
            }
               
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
    
    uint8_t  msg_buf[1024];
    
    uint8_t  client_index;
    
    uint32_t len;
    
    
    thread_pool_t *pool  = (thread_pool_t*)ptr;
    queue_t       *queue = pool->queue;
    
    while(pool->exit_flag) {
        
        sock_t client = queue_pop(queue);
        
        if(is_new_client(client)) {
            
            send(client, welcome_message, strlen(welcome_message), 0);
            
            /* Wait for the clients name - we could wait an amount of time before it timeouts */
            while(1) {
                
                len = read(client, name, 1024);
                
                if(read > 0)
                    break;
            }
            
            client_index = new_client(client, name);
            
            sprintf((int8_t *)msg_buf, "%s has joined the room\n", name);
            
            broadcast_message(msg_buf);
            
            continue;
            
        }
        
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
        if(clients[i].available == 1)            
            send(clients[i].sd, msg, strlen(msg), 0);
        
    }
    
    pthread_mutex_unlock(&clients_lock);
    
}


void *
server_thread(void *ptr) {
    
    start_server(1337, "0.0.0.0");
    
    return(NULL);
}

void *
client_thread(void *ptr) {
    
    uint8_t  input_buf[1024];
    int8_t   ch;
    
    uint16_t pport;
    uint16_t read_size;
    uint16_t buf_index;
    
    sock_t sd;
    
    pthread_t read_thread;
    int32_t  err;
    uint32_t iip;
    
    
    struct sockaddr_in  addrin;

    addrin.sin_family      = AF_INET;
    addrin.sin_port        = u16_port_to_nbo(1337);
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
              
            if (buf_index >= 1024)
                printf("\n%s\n", "[Warning]: Message too long");
            else
                input_buf[buf_index++] = ch;
              
                  
              
            if (ch == '\n') {
                  
                
                input_buf[buf_index++] = '\n';
                input_buf[buf_index]   = 0;
                    
                send(sd, input_buf, strlen(input_buf), 0);
                    
                buf_index = 0;
            
                
            }
        }
                    
    }
    
    return(NULL);
}

void *
client_read_thread(void *sock) {
    
    
    
    sock_t sd = *((sock_t*)sock);
    uint8_t buf[1024];
    
    while(flag) {
        read(sd, buf, 1024);
        
        printf("%s", buf);
    }
    
    return(NULL);
}


void
die(const char *msg) {
    printf("%s", msg);
    exit(1);
}
