#ifndef TORNADO_H
#define TORNADO_H

#define LISTEN_BACKLOG  5
#define BUFFER_SIZE     4096
#define EPOLL_SIZE      128
#define MAX_EVENTS      256

#include <map>
#include <string>
using namespace std;

class IOLoop;
class AsyncHttpServer;
class HttpRequestHandler;
class HttpResponseHandler;

class HttpRequest {
    private:
        int cfd;
        string method;
        string path;
        string body;
        AsyncHttpServer* server;
    public:
        /* Constructor. */
        HttpRequest(const int cfd, const string method, const string path, 
                    const string body="", AsyncHttpServer* const server=NULL);
        /* Returns the method. */
        const string& get_method();
        /* Returns the path. */
        const string& get_path();
        /* Returns the body. */
        const string& get_body();
        /* Returns a request if successful, NULL otherwise. The caller MUST 
        delete the request when no longer used. */
        static HttpRequest* from_sequence(const string& sequence, const int cfd, 
                                          AsyncHttpServer* const server=NULL);
        friend class HttpRequestHandler;
};

class HttpResponse {
    private:
        int code;
        string body;
    public:
        /* Constructor. */
        HttpResponse(const int code, const string body);
        /* Returns the code. */
        const int& get_code();
        /* Returns the body. */
        const string& get_body();
        /* Returns the sequence of the resonse. */
        const string to_sequence();
        /* Returns a response if successful, NULL otherwise. The caller MUST
        delete the response when no longer used. */
        static HttpResponse* from_sequence(const string& sequence);
        friend class HttpResponseHandler;
};

class HttpRequestHandler {
    public:
        /* Called when a http request is available. The response returned is
        sent to the client by the server. NULL or exception would result in
        500 Internal Sever Error. */
        virtual HttpResponse* on_receive(HttpRequest* const request) = 0;
};

class HttpResponseHandler {
    public:
        /* Called when a http response is available. */
        virtual void on_receive(HttpResponse* const response) = 0;
};

class IOHandler {
    public:
        /* Called when network data from the file descriptor is available. */
        virtual void on_readable(int fd) = 0;
        /* Called when network buffer of the file descriptor is available. */
        virtual void on_writable(int fd) = 0;
        /* Called when the file descriptor is closed due to error. */
        virtual void on_error(int fd) = 0;
};

class AsyncHttpClient : public IOHandler {
    private:
        IOLoop* loop;
        map<int, string> read_buffers;
        map<int, string> write_buffers;
        map<int, HttpResponseHandler*> handlers;
    public:
        /* Constructor. */
        AsyncHttpClient(IOLoop* const loop=NULL);
        /* Called when network data from the file descriptor is available. */
        void on_readable(int fd);
        /* Called when network buffer of the file descriptor is available. */
        void on_writable(int fd);
        /* Called when the file descriptor is closed due to error. */
        void on_error(int fd);
        /* Makes a GET request and handles the response by the handler. Returns
        0 if successful, -1 otherwise. */
        int do_get(const string& host, const int& port, const string& path, 
                   HttpResponseHandler* const handler);
};

class AsyncHttpServer : public IOHandler {
    private:
        int fd;
        HttpRequestHandler* handler;
        IOLoop* loop;
        map<int, string> read_buffers;
        map<int, string> write_buffers;
    public:
        /* Constructor. */
        AsyncHttpServer(const int port, HttpRequestHandler* const handler, 
                        IOLoop* const loop=NULL);
        /* Destructor. */
        ~AsyncHttpServer();
        /* Called when network data from the file descriptor is available. */
        void on_readable(int fd);
        /* Called when network buffer of the file descriptor is available. */
        void on_writable(int fd);
        /* Called when the file descriptor is closed due to error. */
        void on_error(int fd);
};

class IOLoop {
    private:
        int fd;
        map<int, IOHandler*> handlers;
        static IOLoop* loop;
    public:
        /* Constructor. */
        IOLoop();
        /* Destructor. */
        ~IOLoop();
        /* Sets the handler for read events on the file descriptor. This
        replaces all events set previously for the file descriptor. */
        void set_read_handler(const int& fd, IOHandler* const handler);
        /* Sets the handler for write events on the file descriptor. This
        replaces all events set previously for the file descriptor. */
        void set_write_handler(const int& fd, IOHandler* const handler);
        /* Unsets the handler for all events on the file descriptor. */
        void unset_handler(const int& fd);
        /* Starts the I/O loop forever. */
        void start();
        /* Returns the singleton instance of the I/O loop. */
        static IOLoop* instance();
};

#endif
