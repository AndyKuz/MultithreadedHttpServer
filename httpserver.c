#include "asgn2_helper_funcs.h"
#include "protocol.h"
#include "queue.h"
#include "rwlock.h"
#include "httpserver.h"
#include "process_request.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <string.h>

pthread_mutex_t mutex;
sem_t q_elems;
queue_t *q;
rwlock_array_t *lock_array;

rwlock_array_t *init_rwlock_array() {
    rwlock_array_t *arr;
    arr = malloc(sizeof(rwlock_array_t));

    arr->length = 0;
    arr->head = NULL;
    arr->tail = NULL;
    return arr;
}

rwlock_array_t *rwlock_array_append(rwlock_array_t *arr, char *uri) {
    rwlock_array_elem_t *new_lock = malloc(sizeof(rwlock_array_elem_t));
    strcpy(new_lock->uri, uri);
    new_lock->read_write_lock = rwlock_new(N_WAY, 1);
    new_lock->next = NULL;

    if (arr->length == 0) {
        arr->head = new_lock;
        arr->tail = new_lock;

    } else if (arr->length == 1) {
        arr->tail = new_lock;
        arr->head->next = new_lock;
    } else {
        arr->tail->next = new_lock;
        arr->tail = new_lock;
    }
    arr->length++;
    return arr;
}

void *worker_thread(void *args) {
    (void) args;
    int openSock;
    while (1) {
        //-fprintf(stderr, "   Worker waiting\n");
        sem_wait(&q_elems);
        //-fprintf(stderr, "   Worker done waiting!\n");
        queue_pop(q, (void **) &openSock);
        //-fprintf(stderr, "               GOT OPENSOCK %d\n", openSock);
        //-fprintf(stderr, "   Worker popped elem\n");
        read_request(openSock);
        //-fprintf(stderr, "   Worker done with read request\n");
    }
    pthread_exit(NULL);
}

int process_args(int argc, char **argv, int *num_threads, int *port_num) {
    int opt;
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
        case 't':
            if (isdigit(*optarg) == 0) {
                fprintf(stderr, "Num_threads not a valid number\n");
                return 1;
            }
            *num_threads = atoi(optarg);
            break;
        default: fprintf(stderr, "Error default process args\n"); return 1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Port number not specified\n");
        return 1;
    }

    if (isdigit(*argv[optind]) == 0) {
        fprintf(stderr, "Port number is not a valid number\n");
        return 1;
    }
    *port_num = atoi(argv[optind]);

    if (*port_num < 1 || *port_num > 65535) {
        fprintf(stderr, "Invalid Port\n");
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    int rc;
    int num_threads = 4;
    int port_num = 0;

    // processes args and validates them
    if ((rc = process_args(argc, argv, &num_threads, &port_num)) == 1) {
        return 1;
    }

    q = queue_new(num_threads);
    pthread_t id[num_threads];

    // init mutex
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        fprintf(stderr, "httpserver.c: Failed to init mutex\n");
        exit(1);
    }

    // init semaphore
    if (sem_init(&q_elems, 0, 0) == -1) {
        fprintf(stderr, "httpserver.c: Failed to init semaphore q_elems\n");
        exit(1);
    }

    // init rwlock_array
    lock_array = init_rwlock_array();

    // creates worker threads
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&id[i], NULL, worker_thread, NULL);
    }

    Listener_Socket sock;
    int openSock;

    // setup connection
    if ((listener_init(&sock, port_num)) == -1) { // sets socket to listen to port
        fprintf(stderr, "Invalid Port\n");
        return 1;
    }

    while (1) {
        openSock = listener_accept(&sock); // waits until connection found on socket
        //-fprintf(stderr, "DISPATCH GOT CONNECTION\n");
        queue_push(q, (void *) (intptr_t) openSock);
        //-fprintf(stderr, "PUSHED %d\n", openSock);
        //-fprintf(stderr, "DISPATCH PUSHED CONNECTION\n");
        //pthread_cond_signal(&worker_go);
        sem_post(&q_elems);
        //-fprintf(stderr, "DISPATCH SEM_POSTED AND IS NOW DONE\n");
    }
}
