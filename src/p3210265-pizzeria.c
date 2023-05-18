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
// contains the order and 2 more time variables needed for statistics
struct CooksArg
{
    uint startSeconds;
    uint bakeSeconds;
    struct Order *order;
} CooksArg;

// rename some structs/types so that we have to type less
typedef struct Order TOrder;
typedef struct CooksArg TCooksArg;
typedef struct timespec ttimespec;

// total income that we made from all orders
uint totalIncome = 0;
// amount of plain pizzas sold
uint plainPizzasSold = 0;
// amount of special pizzas sold
uint specialPizzasSold = 0;
// amount of orders taken
uint orderCount = 0;
// amount of orders whose payment failed
uint failedOrders = 0;
// total service time of all orders(service time: time from the moment the order is registered until it is delivered)
uint totalServiceTime = 0;
// max serive time of a single order
uint maxServiceTime = 0;
// total cooling time(cooling time: tiem from the moment the pizzas are out of the oven and util they are delivered)
uint totalCoolingTime = 0;
// max cooling time of a single order
uint maxCoolingTime = 0;
// amount of available ovens
uint availableOvens;
// uint array that signifies whther the ith packer has finished packing the order
uint *packerDone;

// mutex lock for the previous uint statistics
pthread_mutex_t statisticsLock = PTHREAD_MUTEX_INITIALIZER;
// mutex lock for io(printf,etc.)
pthread_mutex_t printfLock = PTHREAD_MUTEX_INITIALIZER;
// mutex lock for the availableOvens variable
pthread_mutex_t availableOvensLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t packerDoneLock = PTHREAD_MUTEX_INITIALIZER;

// semaphore for cooks
sem_t cooks_sem;
// semaphore for packers
sem_t packers_sem;
// semaphore for the delivery men
sem_t deliveryMen_sem;

// generate a random number in the range [low, high]
// not using seed because rand_r generates the same number every function call(srand and rand dont)
uint randRange(uint low, uint high)
{
    return low + (rand() % (high - low + 1));
}

void print(const char *format, ...)
{
    pthread_mutex_lock(&printfLock);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    pthread_mutex_unlock(&printfLock);
}

void wait_until_cond(uint *a, pthread_mutex_t *mutex)
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

// pthread_create wants __arg to be a pointer. since we want to pass in a number and we cannot use
// the i index of the for loop for lifetime purposes, we just allocate memory and put the number in there
//
// short lifetime explanation: after we break out of the for-loop where we create the threads the memory that was given
// to the variable i will be deleted as it is out of scope
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

void *deliveryMen(void *arg)
{
    sem_wait(&deliveryMen_sem);

    TCooksArg *cooksArg = (TCooksArg *)arg;

    // deliver
    sleep(2 * randRange(TDellow, TDelhigh));

    // get current time
    ttimespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    // calculate deliveryTime and updateStatistics
    pthread_mutex_lock(&statisticsLock);
    long deliveryTime = ts.tv_sec - cooksArg->startSeconds;
    if (maxServiceTime < deliveryTime)
        maxServiceTime = deliveryTime;
    totalServiceTime += deliveryTime;

    // calculate coolingTime and update statistics
    long coolingTime = ts.tv_sec - cooksArg->bakeSeconds;
    if (maxCoolingTime < coolingTime)
        maxCoolingTime = coolingTime;
    totalCoolingTime += coolingTime;
    pthread_mutex_unlock(&statisticsLock);

    double deliveredMinutes = (double)(((double)ts.tv_sec - (double)cooksArg->startSeconds) / (double)60);
    print("Order with id %d was delivered in %.2f minutes!\n", *cooksArg->order->oid, deliveredMinutes);

    sem_post(&deliveryMen_sem);
    pthread_exit(NULL);
}

void *packers(void *arg)
{
    sem_wait(&packers_sem);

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

    double readyMinutes = (double)(((double)ts.tv_sec - (double)cooksArg->startSeconds) / (double)60);
    print("Order with id %d got ready in %.2f minutes!\n", *cooksArg->order->oid, readyMinutes);

    pthread_t deliveryMan;
    pthread_create(&deliveryMan, NULL, deliveryMen, (void *)cooksArg);

    sem_post(&packers_sem);
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

    wait_until_cond(&packerDone[*cooksArg->order->oid], &packerDoneLock);

    // update available ovens
    pthread_mutex_lock(&availableOvensLock);
    availableOvens += cooksArg->order->quantity;
    pthread_mutex_unlock(&availableOvensLock);

    pthread_join(packer, NULL);
    pthread_exit(NULL);
}

void *cooks(void *arg)
{
    sem_wait(&cooks_sem);

    TCooksArg *cooksArg = (TCooksArg *)arg;

    // cook
    sleep(TPrep * cooksArg->order->quantity);

    // wait until there are cooksArg->order->quanity amount of ovens available
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

    sem_post(&cooks_sem);
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

        free(cooksArg->order->oid);
        free(cooksArg->order->pizzas);
        free(cooksArg->order);
        free(cooksArg);
    }

    pthread_exit(NULL);
}

int orderSystem(uint seed, uint NCust)
{
    srand(seed);

    pthread_t *order = malloc(sizeof(pthread_t) * NCust);
    packerDone = malloc(sizeof(uint) * NCust);

    sem_init(&cooks_sem, 0, NCook);
    availableOvens = NOven;
    sem_init(&packers_sem, 0, NPacker);
    sem_init(&deliveryMen_sem, 0, NDeliverer);

    for (uint i = 0; i < NCust; ++i)
    {
        packerDone[i] = 0;
        pthread_create(&order[i], NULL, handleOrder, (void *)getOid(i));

        // wait for next customer to come in
        sleep(randRange(TOrderlow, TOrderhigh));
    }

    for (uint i = 0; i < NCust; ++i)
    {
        pthread_join(order[i], NULL);
    }

    // free memory, destroy mutexes/semaphores/conditions
    pthread_mutex_destroy(&statisticsLock);
    pthread_mutex_destroy(&printfLock);
    pthread_mutex_destroy(&availableOvensLock);
    pthread_mutex_destroy(&packerDoneLock);
    sem_destroy(&cooks_sem);
    sem_destroy(&packers_sem);
    sem_destroy(&deliveryMen_sem);
    free(order);
    free(packerDone);

    // print statistics
    printf("Total income: %d,\n", totalIncome);
    printf("Number of plain pizzas sold: %d,\n", plainPizzasSold);
    printf("Number of special pizzas sold: %d,\n", specialPizzasSold);
    printf("Successfull orders: %d,\n", orderCount - failedOrders);
    printf("Failed orders: %d,\n", failedOrders);
    printf("Average service time: %d,\n", totalServiceTime / (orderCount - failedOrders));
    printf("Max service time: %d,\n", maxServiceTime);
    printf("Average cooling time: %d,\n", totalCoolingTime / (orderCount - failedOrders));
    printf("Max cooling time: %d.\n", maxCoolingTime);

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
