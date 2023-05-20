// p3210265-pizzeria.c
// @ Giorgos Athanasopoulos 2023

// constants provided in `Input and data` section of the project requirements pdf
#include "p3210265-pizzeria.h"
// for threading api
#include <pthread.h>
// for semaphores api
#include <semaphore.h>
// for io
#include <stdio.h>
// for strtol
#include <stdlib.h>
// for CLOCK_REALTIME
#include <time.h>
// for sleep
#include <unistd.h>
// for va_start, va_end, va_list, etc... (printf wrapper)
#include <stdarg.h>

// wrapper class for order
// quantity: amount of pizzas in the order
// pizzas: uint array: 0 for plain pizza, 1 for special
// oid: uint of order id
struct Order
{
    uint quantity;
    uint *pizzas;
    uint *oid;
} Order;
// wrapper class for cook, oven, packer, deliveryMan thread function arg
// order: the order passed in to the thread
// startSeconds: time when the order was registered
// bakeSeconds: time when the order finished baking
struct CooksArg
{
    uint startSeconds;
    uint bakeSeconds;
    struct Order *order;
} CooksArg;

// rename some structs/types so that we type less
typedef struct Order TOrder;
typedef struct CooksArg TCooksArg;
typedef struct timespec ttimespec;

// total income that we made from all orders
uint totalIncome = 0;
uint plainPizzasSold = 0;
uint specialPizzasSold = 0;
uint orderCount = 0;
uint failedOrders = 0;
// total service time of all orders(service time: time from the moment the order is registered until it is delivered)
uint totalServiceTime = 0;
// max serive time of a single order
uint maxServiceTime = 0;
// total cooling time(cooling time: tiem from the moment the pizzas are out of the oven and util they are delivered)
uint totalCoolingTime = 0;
// max cooling time of a single order
uint maxCoolingTime = 0;
// amount of available ovens for a specific timestamp
uint availableOvens;
// uint array that signifies whether the ith packer has finished packing the order
// simpler than pthread_cond
uint *packerDone;

pthread_mutex_t statisticsLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t printfLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t availableOvensLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t packerDoneLock = PTHREAD_MUTEX_INITIALIZER;

sem_t cooksSem;
sem_t packersSem;
sem_t deliveryMenSem;

// generate a random number in the range [low, high]
// not using seed because rand_r generates the same number every function call(srand and rand dont)
uint randRange(uint low, uint high)
{
    return low + (rand() % (high - low + 1));
}

// making printf thread safe
void print(const char *format, ...)
{
    pthread_mutex_lock(&printfLock);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    pthread_mutex_unlock(&printfLock);
}

// a is boolean, mutex is mutex for thread safe accessing of a
// wait until a becomes true (1)
// we make 1 iteration minimum so that we dont lock the mutex 2 times
void waitUntilCond(uint *a, pthread_mutex_t *mutex)
{
    uint done;
    do
    {
        usleep(50 * 1000);
        pthread_mutex_lock(mutex);
        done = *a;
        pthread_mutex_unlock(mutex);
    } while (!done);
}

// so that we dont free invalid pointer
void safeFree(void *mem)
{
    if (mem != NULL)
    {
        free(mem);
        mem = NULL;
    }
}

// pthread_create wants __arg to be a pointer. since we want to pass in a number(when we create the order threads) and
// we cannot use the i index of the for loop for lifetime purposes, we just allocate memory and put the number in there
//
// short lifetime explanation: after we break out of the for-loop where we create the threads the memory that was given
// to the variable i will be deleted as it is out of scope/not used anymore
// if the thread then tries to access i, we ll get a segmentation fault
uint *getOid(uint _oid)
{
    uint *oid = malloc(sizeof(uint));
    *oid = _oid;
    return oid;
}

// generate a random order with oid: oid
TOrder *randOrder(uint *oid)
{
    TOrder *order = malloc(sizeof(TOrder));
    order->quantity = randRange(NOrderlow, NOrderhigh);
    order->pizzas = malloc(sizeof(uint) * order->quantity);
    order->oid = oid;
    for (uint i = 0; i < order->quantity; ++i)
        order->pizzas[i] = randRange(0, 100) <= PPlain ? 0 : 1;
    return order;
}

// pay for an order
int pay(TOrder *order)
{
    // wait for the system to charge customer's credit card
    sleep(randRange(TPaymentlow, TPaymenthigh));

    pthread_mutex_lock(&statisticsLock);

    if (randRange(0, 100) <= PFail)
    {
        print("Payment for order with id %d failed!\n", *order->oid);

        failedOrders++;
        pthread_mutex_unlock(&statisticsLock);
        return 0;
    }

    print("Payment for order with id %d was successfull!\n", *order->oid);

    for (uint i = 0; i < order->quantity; ++i)
        if (order->pizzas[i])
        {
            totalIncome += CSpecial;
            specialPizzasSold++;
        }
        else
        {
            totalIncome += CPlain;
            plainPizzasSold++;
        }

    pthread_mutex_unlock(&statisticsLock);

    return 1;
}

void updateAverageMaxService(uint serviceTime)
{
    pthread_mutex_lock(&statisticsLock);
    if (maxServiceTime < serviceTime)
        maxServiceTime = serviceTime;
    totalServiceTime += serviceTime;
    pthread_mutex_unlock(&statisticsLock);
}

void updateAverageMaxCooling(uint coolingTime)
{
    pthread_mutex_lock(&statisticsLock);
    if (maxCoolingTime < coolingTime)
        maxCoolingTime = coolingTime;
    totalCoolingTime += coolingTime;
    pthread_mutex_unlock(&statisticsLock);
}

void *deliveryMen(void *arg)
{
    sem_wait(&deliveryMenSem);

    TCooksArg *cooksArg = (TCooksArg *)arg;

    // deliver
    sleep(2 * randRange(TDellow, TDelhigh));

    // get current time
    ttimespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint deliveredSeconds = ts.tv_sec - cooksArg->startSeconds;

    // update statistics
    updateAverageMaxService(deliveredSeconds);
    updateAverageMaxCooling(ts.tv_sec - cooksArg->bakeSeconds);

    double deliveredMinutes = (double)(deliveredSeconds) / 60;
    print("Order with id %d was delivered in %.2f minutes!\n", *cooksArg->order->oid, deliveredMinutes);

    sem_post(&deliveryMenSem);
    pthread_exit(NULL);
}

void *packers(void *arg)
{
    sem_wait(&packersSem);

    TCooksArg *cooksArg = (TCooksArg *)arg;

    // pack
    sleep(TPack * cooksArg->order->quantity);

    // signal that we re done packing to ovens so that they are freed
    pthread_mutex_lock(&packerDoneLock);
    packerDone[*cooksArg->order->oid] = 1;
    pthread_mutex_unlock(&packerDoneLock);

    // get current time
    ttimespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    double readyMinutes = (double)(ts.tv_sec - cooksArg->startSeconds) / 60;
    print("Order with id %d got ready in %.2f minutes!\n", *cooksArg->order->oid, readyMinutes);

    pthread_t deliveryMan;
    pthread_create(&deliveryMan, NULL, deliveryMen, (void *)cooksArg);

    sem_post(&packersSem);
    // NOTE: we dont need to wait(pthread_join) for the delivery men to deliver the order in order to free our
    // packers(sem_post). we do it afterwards because we need to ensure that the thread exits successfilly.
    pthread_join(deliveryMan, NULL);
    pthread_exit(NULL);
}

void *ovens(void *arg)
{
    TCooksArg *cooksArg = (TCooksArg *)arg;

    // bake
    sleep(cooksArg->order->quantity * TBake);

    // get current time
    ttimespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    cooksArg->bakeSeconds = ts.tv_sec;

    pthread_t packer;
    pthread_create(&packer, NULL, packers, (void *)cooksArg);

    waitUntilCond(&packerDone[*cooksArg->order->oid], &packerDoneLock);

    pthread_mutex_lock(&availableOvensLock);
    availableOvens += cooksArg->order->quantity;
    pthread_mutex_unlock(&availableOvensLock);

    pthread_join(packer, NULL);
    pthread_exit(NULL);
}

void *cooks(void *arg)
{
    sem_wait(&cooksSem);

    TCooksArg *cooksArg = (TCooksArg *)arg;

    // cook
    sleep(TPrep * cooksArg->order->quantity);

    // wait until there are cooksArg->order->quanity amount of ovens available
    // cant use waitUntilCond because of complex expression
    int _availableOvens;
    do
    {
        usleep(50 * 1000);

        pthread_mutex_lock(&availableOvensLock);
        _availableOvens = availableOvens;
        pthread_mutex_unlock(&availableOvensLock);

    } while (_availableOvens < cooksArg->order->quantity);

    pthread_mutex_lock(&availableOvensLock);
    availableOvens -= cooksArg->order->quantity;
    pthread_mutex_unlock(&availableOvensLock);

    pthread_t oven;
    pthread_create(&oven, NULL, ovens, (void *)cooksArg);

    sem_post(&cooksSem);
    pthread_join(oven, NULL);
    pthread_exit(NULL);
}

// main thread for every order
void *handleOrder(void *arg)
{
    uint *oid = (uint *)arg;
    TOrder *order = randOrder(oid);

    // get current time
    ttimespec start;
    clock_gettime(CLOCK_REALTIME, &start);

    pthread_mutex_lock(&statisticsLock);
    orderCount++;
    pthread_mutex_unlock(&statisticsLock);

    if (pay(order))
    {
        TCooksArg *cooksArg = malloc(sizeof(TCooksArg));
        cooksArg->order = order;
        cooksArg->startSeconds = start.tv_sec;

        pthread_t cook;
        pthread_create(&cook, NULL, cooks, (void *)cooksArg);
        pthread_join(cook, NULL);

        safeFree(cooksArg);
    }

    safeFree(order->oid);
    safeFree(order->pizzas);
    safeFree(order);
    pthread_exit(NULL);
}

// prints all of the required statistics listed in the os_project.pdf
void printStatistics()
{
    printf("Total income: %d,\n", totalIncome);
    printf("Number of plain pizzas sold: %d,\n", plainPizzasSold);
    printf("Number of special pizzas sold: %d,\n", specialPizzasSold);
    printf("Successfull orders: %d,\n", orderCount - failedOrders);
    printf("Failed orders: %d,\n", failedOrders);
    printf("Average service time: %d,\n", totalServiceTime / (orderCount - failedOrders));
    printf("Max service time: %d,\n", maxServiceTime);
    printf("Average cooling time: %d,\n", totalCoolingTime / (orderCount - failedOrders));
    printf("Max cooling time: %d.\n", maxCoolingTime);
}

void initializeSems()
{
    sem_init(&cooksSem, 0, NCook);
    sem_init(&packersSem, 0, NPacker);
    sem_init(&deliveryMenSem, 0, NDeliverer);
}

void destroyMutexesSems()
{
    pthread_mutex_destroy(&statisticsLock);
    pthread_mutex_destroy(&printfLock);
    pthread_mutex_destroy(&availableOvensLock);
    pthread_mutex_destroy(&packerDoneLock);
    sem_destroy(&cooksSem);
    sem_destroy(&packersSem);
    sem_destroy(&deliveryMenSem);
}

int orderSystem(uint seed, uint NCust)
{
    srand(seed);

    pthread_t *order = malloc(sizeof(pthread_t) * NCust);
    packerDone = malloc(sizeof(uint) * NCust);
    availableOvens = NOven;

    initializeSems();

    for (uint i = 0; i < NCust; ++i)
    {
        packerDone[i] = 0;
        pthread_create(&order[i], NULL, handleOrder, (void *)getOid(i));

        // wait for next customer to come in
        sleep(randRange(TOrderlow, TOrderhigh));
    }

    for (uint i = 0; i < NCust; ++i)
        pthread_join(order[i], NULL);

    printStatistics();

    destroyMutexesSems();
    safeFree(order);
    safeFree(packerDone);
    return 0;
}

int main(int argc, const char **argv)
{
    if (argc < 3)
    {
        printf("Missing arguments(need seed && customerCount)!\n");
        return 1;
    }
    return orderSystem(strtol(argv[1], NULL, 10), strtol(argv[2], NULL, 10));
}
