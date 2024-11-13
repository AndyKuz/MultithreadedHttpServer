#include "rwlock.h"
#pragma once

typedef struct KeyValue {
    char key[130];
    char value[130];
} KeyValue;

typedef struct HeaderFieldList {
    int numPairs;
    KeyValue *pairs;
    int contentLength;
    int requestID;
} HeaderFieldList;

typedef struct RequestLine {
    char method[10];
    char uri[66];
    char version[10];
} RequestLine;

typedef struct Request {
    RequestLine requestLine;
    HeaderFieldList headerFieldList;
    char leftOverMessageBody[3000];
    int numBytesLeftOverMessageBody;
} Request;

// --------------------------URI LOCK ARRAY FUNCTIONS/STRUCT-----------------------------

typedef struct rwlock_array_elem {
    rwlock_t *read_write_lock;
    char uri[1000];
    struct rwlock_array_elem *next;
} rwlock_array_elem_t;

typedef struct rwlock_array {
    rwlock_array_elem_t *head;
    rwlock_array_elem_t *tail;
    int length;
} rwlock_array_t;

rwlock_array_t *rwlock_array_append(rwlock_array_t *arr, char *uri);
