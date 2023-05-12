#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "p3210265.h"

uint totalIncome = 0;
uint plainsSold = 0;
uint specialsSold = 0;
uint orderCount = 0;
uint failedOrders = 0;
uint totalServiceTime = 0;
uint maxServiceTime = 0;
uint totalCoolingTime = 0;
uint maxCoolingTime = 0;

pthread_mutex_t statisticsLock = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t handlerDone = PTHREAD_COND_INITIALIZER;

pthread_mutex_t *handlersDone;

uint randRange(uint seed, uint low, uint high) {
    return low + (rand_r(&seed) % (high - low + 1));
}

struct OrderId {
    uint seed;
    uint oid;
} OrderId;
typedef struct OrderId TOrderId;

TOrderId *createOrderId(uint seed, uint oid) {
    TOrderId *orderId = malloc(sizeof(TOrderId));
    orderId->seed = seed;
    orderId->oid = oid;
    return orderId;
}

struct Order {
    uint quantity;
    uint *pizzas;
    uint seed;
    uint oid;
} Order;
typedef struct Order TOrder;

TOrder *randOrder(TOrderId *orderId) {
    TOrder *order = malloc(sizeof(TOrder));
    order->quantity = randRange(orderId->seed, NOrderlow, NOrderhigh);
    order->pizzas = malloc(sizeof(uint) * order->quantity);
    order->seed = orderId->seed;
    order->oid = orderId->oid;
    for (uint i = 0; i < order->quantity; ++i) {
        order->pizzas[i] = randRange(orderId->seed, 0, 100) <= PPlain ? 0 : 1;
    }
    return order;
}

int pay(TOrder *order) {
    sleep(randRange(order->seed, TPaymentlow, TPaymenthigh));
    pthread_mutex_lock(&statisticsLock);
    if (randRange(order->seed, 0, 100) <= PFail) {
        printf("Payment for order with id %d failed!\n", order->oid);
        failedOrders++;
        pthread_mutex_unlock(&statisticsLock);
        return 0;
    }
    for (uint i = 0; i < order->quantity; ++i) 
        if (order->pizzas[i]) {
            totalIncome += CSpecial;
            specialsSold++;
        } else {
            totalIncome += CPlain;
            plainsSold++;
        }
    
    pthread_mutex_unlock(&statisticsLock);
    return 1;
}

void *handleOrder(void *arg) {
    TOrderId *orderId = (TOrderId *) arg;
    TOrder *order = randOrder(orderId);
    pthread_mutex_lock(&statisticsLock);
    orderCount++;
    pthread_mutex_unlock(&statisticsLock);
    if(pay(order)) {
        
    }
    
    pthread_mutex_unlock(&handlersDone[orderId->oid]);
    pthread_cond_signal(&handlerDone);
    pthread_exit(NULL);
}

int orderSystem(uint seed, uint NCust) {
    pthread_t cook[NCook];
    pthread_t oven[NOven];
    pthread_t packer[NPacker];
    pthread_t deliveryMan[NDeliverer];

    pthread_t order[NCust];
    pthread_mutex_t *handlersDone = malloc(sizeof(pthread_mutex_t) * NCust);

    for (uint i = 0; i < NCust; ++i) {
        pthread_mutex_init(&handlersDone[i], NULL);
        int rc = pthread_create(&order[i], NULL, handleOrder, (void *) createOrderId(seed, i));
        if (rc) 
            printf("Error creating thread: %d.\n", i);
        sleep(randRange(seed, TOrderlow, TOrderhigh));
    }

    for (uint i = 0; i < NCust; ++i) {
        pthread_cond_wait(&handlerDone, &handlersDone[i]);
    }
    
    pthread_mutex_destroy(&statisticsLock);
    pthread_cond_destroy(&handlerDone);
    for (uint i = 0; i < NCust; ++i) {
        pthread_mutex_destroy(&handlersDone[i]);
    }

    printf("Total income: %d,\n", totalIncome);
    printf("Number of plain pizzas sold: %d,\n", plainsSold);
    printf("Number of special pizzas sold: %d,\n", specialsSold);
    printf("Successfull orders: %d.\n", (orderCount - 1) - failedOrders);
    printf("Failed orders: %d,\n", failedOrders);
    printf("Average service time: %d,\n", totalServiceTime / (orderCount - failedOrders));
    printf("Max service time: %d,\n", maxServiceTime);
    printf("Average cooling time: %d,\n", totalCoolingTime / (orderCount - failedOrders));
    printf("Max cooling time: %d.", maxCoolingTime);

    return 0;
}

int main(int argc, const char **argv) 
{
    if (argc < 3) {
        printf("Missing arguments(need customerCount & seed)!\n");
        return 1;
    }
    return orderSystem(strtol(argv[1], NULL, 10), strtol(argv[2], NULL, 10));
}
