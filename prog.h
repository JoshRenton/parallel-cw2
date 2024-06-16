#include <pthread.h>

// The struct that contains the variables needed by the threads
struct args {
    pthread_mutex_t* row_mutexes;
    int* size;
    double* input;
    double* precision;
    int* stop;
    pthread_mutex_t* stop_lock;
};