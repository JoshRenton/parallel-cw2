#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "prog.h"

// Create random array of doubles between 0 and 1
void create_random_array(double* array, int array_length) {
    for (long int index = 0; index < array_length; index++) {
        array[index] = (double)rand() / (double)RAND_MAX;
    }
}

// Create test array with 1.0 on all borders
void create_test_array(double* array, int size) {
    for (int row = 0; row < size; row++) {
        for (int column = 0; column < size; column++) {
            int index = row * size + column;
            if (row == 0 || row == size - 1) {
                array[index] = 1;
            }
            else if (column == 0 || column == size - 1) {
                array[index] = 1;
            }
            else {
                array[index] = 0;
            }
        }
    } 
}

// Copies the first array into the second array
void copy_array(double* array, double* array_copy, int size) {
    int array_length = size * size;

    for (int i = 0; i < array_length; i++) {
        array_copy[i] = array[i];
    }
}

// Returns the absolute value of a double
double absolute(double value) {
    if (value < 0.0) {
        return 0.0 - value;
    }
    else {
        return value;
    }
}

// Prints the provided array of doubles
void print_double_array(double* array, int size) {
    printf("[");
    for (int row = 0; row < size; row ++) {
        for (int column = 0; column < size; column++) {
            printf("%f, ", array[row * size + column]);
        }
        printf("\n");
    }
    printf("]");
}

// Set a shared stop value to 1
void setStop(int* stop) {
    *stop = 1;
}

void* run_thread(void* args) {
    struct args* arg = (struct args*) args;
    double* input = arg->input;
    int size = *(arg->size);
    double precision = *(arg->precision);

    /* 
        Each thread has a copy of it's iteration through the array
        This is necessary as the main array is constantly written to
        in order to facilitate parallelisation 
    */
    double* input_copy = malloc(size * size * sizeof(double));

    /*
        Each thread also has a copy of the previous row that it worked on.
        This is also necessary to prevent the newly written values in the 
        previous row from influencing the next rows calculations.
    */
    double prev_row[size - 2];

    copy_array(input, input_copy, size);

    int iterations = 1;

    /* 
        When stop is set to 1 by one of the threads, 
        all threads will stop after their current cycle
    */
    while (*(arg->stop) != 1) {
        // Each thread has their own local stop variable
        int stop_thread = 1;
        int has_mutex = 0;
        for (int row = 1; row < size - 1; row++) {
            // Don't try to get the mutex again if it already has it
            if (has_mutex == 0) {
                pthread_mutex_lock(&(arg->row_mutexes[row - 1]));
            }
            pthread_mutex_lock(&(arg->row_mutexes[row]));
            /*
                This (has_mutex) stops the thread from trying 
                to reacquire the current rows mutex
                on each iteration after the first.
            */
            has_mutex = 1;
            for (int column = 1; column < size - 1; column++) {
                int index = row * size + column;
                double sum_of_neighbours;
                if (row == 1) {
                    sum_of_neighbours = input[index - 1] + input[index + 1]
                     + input[index - size] + input[index + size];
                } else {
                    sum_of_neighbours = input[index - 1] + input[index + 1]
                     + prev_row[column - 1] + input[index + size];
                }

                double prev_value = input[index];
                prev_row[column - 1] = prev_value;
                
                input_copy[index] = (double) (sum_of_neighbours / 4.0);

                // If the change is not precise enough, set stop_thread to 0
                if (absolute(prev_value - input_copy[index]) > precision) {
                    stop_thread = 0;
                }
            }
            /*
                Write to input only after finishing 
                row to ensure new values don't mess up calculations
            */
            for (int column = 1; column < size - 1; column++) {
                int index = row * size + column;
                input[index] = input_copy[index];
            }
            pthread_mutex_unlock(&(arg->row_mutexes[row - 1]));
            // Only give up the current row mutex if at the end of an iteration
            if (row == size - 2) {
                pthread_mutex_unlock(&(arg->row_mutexes[row]));
            }
        }
        // Lock while modifying the shared stop variable
        pthread_mutex_lock(arg->stop_lock);
        /*
            If this iteration has the necessary precision, 
            print out the result and set stop to 1
        */
        // if ((stop_thread == 1) && *(arg->stop) != 1) {
        //     setStop(arg->stop);
        // }
        // 
        if ((iterations == 3) && *(arg->stop) != 1) {
            setStop(arg->stop);
            print_double_array(input_copy, size);
        }
        pthread_mutex_unlock(arg->stop_lock);
        iterations += 1;
    }

    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        return 0;
    }

    int threads = atoi(argv[1]);
    pthread_t thread_ids[threads];

    int size = atoi(argv[2]);

    int array_length = size * size;

    double* input = malloc(array_length * sizeof(double));

    srand(3);

    create_test_array(input, size);

    // create_random_array(input, array_length);

    pthread_mutex_t row_mutexes[size];

    // Initialise the row mutexes
    for (int row = 0; row < size; row++) {
        pthread_mutex_t row_mutex;
        pthread_mutex_init(&row_mutex, NULL);
        row_mutexes[row] = row_mutex;
    }

    // Initialise the lock for the shared stop
    pthread_mutex_t stop_lock;
    pthread_mutex_init(&stop_lock, NULL);

    double precision = 0.01;
    int stop = 0;

    struct args args;

    args.row_mutexes = row_mutexes;
    args.input = input;
    args.size = &size;
    args.precision = &precision;
    args.stop = &stop;
    args.stop_lock = &stop_lock;

    // Create the threads
    for (int thread = 0; thread < threads; thread++) {
        pthread_create(&thread_ids[thread], NULL, run_thread, (void*) &args);
    }

    // Wait for all threads to join
    for (int thread = 0; thread < threads; thread++) {
        pthread_join(thread_ids[thread], NULL);
    }
}