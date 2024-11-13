#include "post_request.h"
#include "asgn2_helper_funcs.h"
#include "rwlock.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

extern rwlock_array_t *lock_array;
extern pthread_mutex_t mutex;

/*
    - Executes the PUT method with all relevant data in request
    - Fills relevant fields in response struct
*/
int putMethod(int sock, Request *request, rwlock_t *curr_lock) {
    int successStatusCode = 200;
    char *successStatusCodePhrase = "OK";
    struct stat buffer;

    writer_lock(curr_lock); // WRITER LOCKING
    //-fprintf(stderr, "       WRITER NOW LOCKINGGG!!!!\n");
    int exists = stat(request->requestLine.uri, &buffer);
    if (exists != 0) { // checks if file exists already
        successStatusCode = 201;
        successStatusCodePhrase = "Created";
    }

    int f = open(request->requestLine.uri, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (f == -1) {
        if (errno == EACCES || errno == EISDIR || errno == ENOENT) {
            dprintf(sock, "HTTP/1.1 403 Forbidden\r\nContent-Length: %d\r\n\r\nForbidden\n", 10);
            fprintf(stderr, "PUT,/%s,403,%d\n", request->requestLine.uri,
                request->headerFieldList.requestID);
        } else {
            dprintf(sock,
                "HTTP/1.1 500 Internal Server Error\r\nContent-Length: %d\r\n\r\nInternal Server "
                "Error\n",
                22);
            fprintf(stderr, "PUT,/%s,500,%d\n", request->requestLine.uri,
                request->headerFieldList.requestID);
        }
        writer_unlock(curr_lock);
        return 1;
    }

    int bytesWritten;
    int totalBytesWritten = 0;
    while ((bytesWritten = write(f, request->leftOverMessageBody + totalBytesWritten,
                request->numBytesLeftOverMessageBody - totalBytesWritten))
           != 0) {
        totalBytesWritten += bytesWritten;
    }
    if (bytesWritten == -1) {
        dprintf(sock,
            "HTTP/1.1 500 Internal Server Error\r\nContent-Length: %d\r\nInternal Server Error\n",
            22);
        fprintf(stderr, "PUT,/%s,500,%d\n", request->requestLine.uri,
            request->headerFieldList.requestID);
        close(f);
        writer_unlock(curr_lock);
        return 1;
    }

    bytesWritten = pass_n_bytes(
        sock, f, request->headerFieldList.contentLength - request->numBytesLeftOverMessageBody);

    if (bytesWritten == -1) {
        dprintf(sock,
            "HTTP/1.1 500 Internal Server Error\r\nContent-Length: %d\r\nInternal Server Error\n",
            22);
        fprintf(stderr, "PUT,/%s,500,%d\n", request->requestLine.uri,
            request->headerFieldList.requestID);
        close(f);
        writer_unlock(curr_lock);
        return 1;
    }

    dprintf(sock, "HTTP/1.1 %d %s\r\nContent-Length: %lu\r\n\r\n%s\n", successStatusCode,
        successStatusCodePhrase, strlen(successStatusCodePhrase) + 1, successStatusCodePhrase);
    fprintf(stderr, "PUT,/%s,%d,%d\n", request->requestLine.uri, successStatusCode,
        request->headerFieldList.requestID);
    close(f);
    //-fprintf(stderr, "       WRITER NOW UNLOCKING!!!\n");
    writer_unlock(curr_lock); // WRITER UNLOCKING
    return 0;
}

/*
    - Executes GET method with all relevant data in request
    - Fills relevant fields in response struct
*/
int getMethod(int sock, Request *request, rwlock_t *curr_lock) {
    int rc;
    struct stat st;
    int contentLength;

    reader_lock(curr_lock); // READER LOCKING
    //-fprintf(stderr, "READER GOT LOCK!!!!! for sock %d, requestID %d\n", sock, request->headerFieldList.requestID);

    int f = open(request->requestLine.uri, O_RDONLY);
    if (f == -1) {
        if (errno == EACCES) { // Permission Denied
            dprintf(sock, "HTTP/1.1 403 Forbidden\r\nContent-Length: %d\r\n\r\nForbidden\n", 10);
            fprintf(stderr, "GET,/%s,403,%d\n", request->requestLine.uri,
                request->headerFieldList.requestID);
        } else if (errno == ENOENT) { // File Doesn't Exist
            dprintf(sock, "HTTP/1.1 404 Not Found\r\nContent-Length: %d\r\n\r\nNot Found\n", 10);
            fprintf(stderr, "GET,/%s,404,%d\n", request->requestLine.uri,
                request->headerFieldList.requestID);
        } else {
            dprintf(sock,
                "HTTP/1.1 500 Internal Server Error \r\nContent-Length: %d\r\n\r\nInternal Server "
                "Error\n",
                22);
            fprintf(stderr, "GET,/%s,500,%d\n", request->requestLine.uri,
                request->headerFieldList.requestID);
        }
        //-fprintf(stderr, "READER UNLOCK!!!\n");
        reader_unlock(curr_lock);
        return 1;
    }

    // check if file is directory
    stat(request->requestLine.uri, &st);
    if (S_ISDIR(st.st_mode)) {
        dprintf(sock, "HTTP/1.1 403 Forbidden\r\nContent-Length: %d\r\n\r\nForbidden\n", 10);
        fprintf(stderr, "GET,/%s,403,%d\n", request->requestLine.uri,
            request->headerFieldList.requestID);
        close(f);
        reader_unlock(curr_lock);
        return 1;
    }

    fstat(f, &st); // gets uri file info
    contentLength = st.st_size; // gets uri file size in bytes

    dprintf(sock, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", contentLength);
    fprintf(
        stderr, "GET,/%s,200,%d\n", request->requestLine.uri, request->headerFieldList.requestID);
    rc = pass_n_bytes(f, sock, contentLength);
    if (rc == -1) {
        dprintf(sock,
            "HTTP/1.1 500 Internal Server Error \r\nContent-Length: %d\r\n\r\nInternal Server "
            "Error\n",
            22);
        fprintf(stderr, "GET,/%s,500,%d\n", request->requestLine.uri,
            request->headerFieldList.requestID);
        close(f);
        reader_unlock(curr_lock);
        return 1;
    }
    close(f);
    reader_unlock(curr_lock);
    return 0;
}

int processRequest(int sock, Request *request) {
    int rc;
    rwlock_t *curr_lock;
    int lock_found = 0;

    pthread_mutex_lock(&mutex); // MUTEX LOCKING
    //-fprintf(stderr, "               MUTEX LOCKED\n");
    rwlock_array_elem_t *curr_elem = lock_array->head;

    if (lock_array->length != 0) {
        //-fprintf(stderr, "           actual uri %s | lock array head -> %s | num elems -> %d\n", request->requestLine.uri, curr_elem->uri, lock_array->length);
    }
    for (int i = 0; i < lock_array->length; i++) { // search for preexisting lock for uri
        if (strcmp(curr_elem->uri, request->requestLine.uri) == 0) {
            //-fprintf(stderr,             "EXISTING LOCK FOUND!\n");
            lock_found = 1;
            curr_lock = curr_elem->read_write_lock;
            break;
        }
        curr_elem = curr_elem->next;
        if (i != lock_array->length) {
            //-fprintf(stderr, "           next uri -> %s\n", curr_elem->uri);
        }
    }

    if (!lock_found) { // need to add a lock for the uri
        lock_array = rwlock_array_append(lock_array, request->requestLine.uri);
        curr_lock = lock_array->tail->read_write_lock;
    }
    //-fprintf(stderr, "               MUTEX UNLOCKED\n");
    pthread_mutex_unlock(&mutex); // MUTEX UNLOCKING

    if (strcmp(request->requestLine.method, "GET") == 0) {
        rc = getMethod(sock, request, curr_lock);
        if (rc == 1) {
            return 1;
        }
    } else if (strcmp(request->requestLine.method, "PUT") == 0) {
        rc = putMethod(sock, request, curr_lock);
        if (rc == 1) {
            return 1;
        }
    } else {
        dprintf(
            sock, "HTTP/1.1 501 Not Implemented\r\nContent-Length: %d\r\nNot Implemented\n", 16);
        fprintf(stderr, ".................METHOD NOT IMPLEMENTED\n");
        return 1;
    }
    return 0;
}
