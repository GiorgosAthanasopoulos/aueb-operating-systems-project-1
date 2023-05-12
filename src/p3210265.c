#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "p3210265.h"

# define FREE(ptr) do { \
    free((ptr)); \
    (ptr) = NULL; \
} while(0)

struct Pizza {
    int special;
} Pizza;
typedef struct Pizza TPizza;

struct Order {
    TPizza **pizzas;
} Order;
typedef struct Order TOrder;

void freeOrder(TOrder *order) {
    FREE(order->pizzas);
    FREE(order);
}

typedef struct timespec ttimespec;
void my_sleep(int seed) {
    ttimespec *tp;
    clock_gettime(CLOCK_REALTIME, tp);
    sleep(tp->tv_sec);
}

void orderSystem(int customerCount, int seed) {
    while(customerCount > 0) {
        my_sleep(seed);
    }
}

int main(int argc, const char **argv) 
{
    if (argc < 3) {
        printf("Missing arguments(need customer_count & seed)!\n");
    }
    
    char *customerCountRest;
    int customerCount = strtol(argv[1], &customerCountRest, 10);
    char *seedRest;
    int seed = strtol(argv[2], &seedRest, 10);

    orderSystem(customerCount, seed);

    FREE(customerCountRest);
    FREE(seedRest);

    return 0;
}
