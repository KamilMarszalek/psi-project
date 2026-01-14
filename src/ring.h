#include <stdbool.h>
#include <stdint.h>

#define TOKEN_DATA_SIZE 256

typedef struct Token {
    char* sender;
    char* reciever;
    char* data;
    bool is_empty;
} token_t;

typedef struct Route {
    char* node_name;
    uint16_t port;
} route_t;

typedef struct RouteConfig {
    route_t current;
    route_t prev;
    route_t next;
} route_config_t;


int fill_config(route_config_t* config);
int setup_ring(route_config_t* config);
