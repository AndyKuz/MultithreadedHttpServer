# Assignment 4 Directory

This is the final project for CSE 130. In this project, we combined all previous assignments to create a multithreaded HTTP server. The server uses a thread-safe queue, allowing multiple clients to interact with it while maintaining the correct order of request processing (linearization). It also logs all requests sent to the server and the server's responses in chronological order as an "audit log." The server is configured to handle only `PUT` and `GET` requests.

## How to Run

- `make all`: Compiles the necessary executables.
- `./httpserver [-t threads] <port>`: Runs the HTTP server with the specified number of threads and port.

To send requests, you can use various methods, with `curl` being one of the simplest.

## File Descriptions

### asgn2_helper_funcs.h/asgn2_helper_funcs.a
Contains function prototypes provided by the course to assist in data sending/receiving between the host and client.

### httpserver.c/httpserver.h
The main files for hosting the multithreaded HTTP server. These files initialize the thread-safe queue, the worker threads, and set up the server-client connection. After receiving and parsing requests, functions in `post_request.c` are called to process `PUT` or `GET` requests.

### post_request.c/post_request.h
Once the server reads the HTTP request, it passes relevant information to functions in `post_request.c` for execution. `post_request.c` handles both `PUT` and `GET` requests, including error handling and returning the appropriate HTTP error codes.

### protocol.h
Defines constants and regular expression patterns used consistently in correct `PUT` and `GET` HTTP requests.

### queue.h
Contains function prototypes for a queue data structure that was implemented in a previous assignment, including functions like `push` and `pop`.

### rwlock.h
Contains function prototypes for a reader-writer lock created in a previous assignment, with functions such as `reader_lock`, `reader_unlock`, `writer_lock`, and `writer_unlock`.
