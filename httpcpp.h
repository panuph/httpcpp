#ifndef HTTPCPP_H
#define HTTPCPP_H

#define LISTEN_BACKLOG  5
#define BUFFER_SIZE     2048
#define EPOLL_SIZE      64
#define MAX_EVENTS      128
#define MAX_NMATCH      16

#include <map>
#include <string>
#include <vector>
#include <utility>

using namespace std;

class IOLoop;
class AsyncHttpServer;
class HttpRequestHandler;
class HttpResponseHandler;

/**
 * HttpRequest provides access to data of an HTTP reqeust. In general cases, 
 * AsyncHttpServer creates objects of this class automatically and provides 
 * them in methods of HttpReqestHandler, which you inherit in order to build 
 * your own handler.
 *
 * TODO: (1) Code from_sequence() to parse more data, like headers, and make
 *           them available via getting methods, e.g. get_headers("XYZ").
 */
class HttpRequest {
    friend class AsyncHttpServer;
    friend class HttpRequestHandler;
    private:
        string method;      
        string path;        
        string body; 
        AsyncHttpServer* server;
        int fd;
        bool done;
    protected:
        /**
         * Parses the sequence and returns a request if successful or NULL if 
         * not. The caller MUST delete the request when no longer used.
         *
         * @param sequence the sequence to be parsed into an HttpRequest object
         */
        static HttpRequest* from_sequence(const string& sequence);
        /** 
         * Constructor.
         *
         * @param method the method of the request 
         * @param path the path of the request
         * @param body the body of the request
         */
        HttpRequest(const string& method, const string& path, 
            const string& body="");
    public:
        /**
         * Returns the method. 
         */
        const string& get_method();
        /**
         * Returns the path.
         */
        const string& get_path();
        /**
         * Returns the body. 
         */
        const string& get_body();
};

/**
 * HttpResponse provides access to data of an HTTP response. In general cases, 
 * AsyncHttpClient creates objects of this class automatically and provides
 * them in methods of HttpResponseHandler, which you inherit in order to build 
 * your own handler.
 *
 * TODO: (1) Code from_sequence() to parse more data, like headers, and make
 *           them available via getting methods, e.g. get_headers("XYZ").
 *       (2) Make from_sequence() more flexible because it always expects 
 *           Content-Length at the moment.
 */
class HttpResponse {
    friend class AsyncHttpClient;
    friend class AsyncHttpServer;
    private:
        int code;
        string body;
    protected:
        /** 
         * Returns the sequence of the resonse as an HTTP sequence. 
         *
         * @param code the code of the response
         * @param body the body of the response
         */
        static const string to_sequence(int code, const string& body="");
        /**
         * Parses the sequence and returns a response if successful or NULL if 
         * not. The caller MUST delete the response when no longer used. 
         *
         * @param sequence the sequence to be parsed into an HttpResponse object 
         */
        static HttpResponse* from_sequence(const string& sequence);
        /** 
         * Constructor. 
         *
         * @param code the code of the response
         * @param body the body of the response
         */
        HttpResponse(const int& code, const string& body="");
    public:
        /**
         * Returns the code. 
         */
        const int& get_code();
        /**
         * Returns the body. 
         */
        const string& get_body();
};

/**
 * HttpRequestHandler handles HTTP requests on the server side. All handlers of 
 * AsyncHttpServer must inherit this class and should implement the supported 
 * methods accordingly.
 */
class HttpRequestHandler {
    friend class AsyncHttpServer;
    protected:
        /**
         * Replies to the client of the request with the code and the body.
         * 
         * @param request the HTTP request to reply to
         * @param code the code of the response
         * @param body the body of the response
         */
        void reply(HttpRequest* const request, const int& code, 
            const string& body="");
    public:
        /**
         * Destructor.
         */
        virtual ~HttpRequestHandler() {}
        /**
         * Called when a HTTP GET request is available. The caller should
         * always manage to reply the request using method reply().
         *
         * @param request the HTTP request         
         * @param args the arguments associated with the regex of the handler        
         */
        virtual void get(HttpRequest* const request, 
            const vector<string>& args);
        /**
         * Called when a HTTP POST request is available. The caller should 
         * always manage to reply the request using method reply().
         *
         * @param request the HTTP request 
         * @param args the arguments associated with the regex of the handler        
         */
        virtual void post(HttpRequest* const request, 
            const vector<string>& args);
};

/**
 * HttpResponseHandler handles HTTP responses on the client side. All handlers 
 * of AsyncHttpClient must inherit this class and implement method on_receive().
 */
class HttpResponseHandler {
    public:
        /**
         * Destructor.
         */
        virtual ~HttpResponseHandler() {}
        /**
         * Called when an HTTP response is available. 
         *
         * @param response the HTTP response
         */
        virtual void handle(HttpResponse* const response) {}
};

/**
 * IOHandler handles IO events. It is the parent class of AsyncHttpClient and
 * AsyncHttpServer. In general, you should not need to use this class. 
 */
class IOHandler {
    protected:
        map<int, string> read_buffers;
        map<int, string> write_buffers;
        /**
         * Clears all the buffers associated with the fd.
         *
         * @param the associated file descriptor
         */
        void clear_buffers(const int& fd);
    public:
        /**
         * Destructor.
         */
        virtual ~IOHandler() {}
        /**
         * Called when network data from the file descriptor is available. 
         *
         * @param fd the associted file descriptor
         */
        virtual void on_read(const int& fd) = 0;
        /**
         * Called when network buffer of the file descriptor is available. 
         *
         * @param fd the associted file descriptor
         */
        virtual void on_write(const int& fd) = 0;
        /**
         * Called when the file descriptor is closed unexpectedly. 
         *
         * @param fd the associted file descriptor
         */
        virtual void on_close(const int& fd) = 0;
};

/**
 * AsyncHttpClient is an async HTTP client driven by an IO loop.
 */
class AsyncHttpClient : public IOHandler {
    friend class IOLoop;
    private:
        IOLoop* loop;
        map<int, HttpResponseHandler*> handlers;
    protected:
        /**
         * Called when network data from the file descriptor is available. 
         *
         * @param fd the associted file descriptor
         */
        void on_read(const int& fd);
        /**
         * Called when network buffer of the file descriptor is available. 
         *
         * @param fd the associted file descriptor
         */
        void on_write(const int& fd);
        /**
         * Called when the file descriptor is closed unexpectedly. 
         *
         * @param fd the associted file descriptor
         */
        void on_close(const int& fd);
    public:
        /**
         * Constructor.
         *
         * @param loop the IO loop that drives the client
         */
        AsyncHttpClient(IOLoop* const loop=NULL);
       /**
         * Makes a request and handles the response by the handler. Note that,
         * unlike AsyncHttpServer, this class deletes (de-allocate the memory 
         * of) the handler after it is called. Raises an exception if an error
         * occurs.
         *
         * @param host the host (in IP format) of the target server
         * @param port the port of the target server
         * @param method the method of the request
         * @param path the path of the request
         * @param body the body of the request
         * @param handler the handler to call when the response is received
         */
        void fetch(const string& host, const int& port, const string& method,
            const string& path, const string& body, 
            HttpResponseHandler* const handler);
};

/**
 * AsyncHttpServer is an async HTTP server driven by an IO loop. 
 */
class AsyncHttpServer : public IOHandler {
    friend class HttpRequestHandler;
    private:
        int fd;
        IOLoop* loop;
        vector<pair<string, HttpRequestHandler*> > handlers;
    protected:
        /**
         * Returns the handler whose pattern of interest matches the path 
         * and NULL if the handler is not found.
         *
         * @param path the path of the request
         */
        HttpRequestHandler* find_handler(const string& path);
        /**
         * Returns the arguments according to the regex given when adding the
         * associated handler to the server.
         *
         * @param path the path of the request
         */
        vector<string> get_arguments(const string& path);
        /**
         * Writes to the client of the file descriptor the response.
         * 
         * @param fd the associated file descriptor
         * @param code the code of the response
         * @param body the body of the response
         */
        void reply(const int& fd, const int& code, const string& body="");
        /**
         * Called when network data from the file descriptor is available. 
         * 
         * @param fd the associated file descriptor
         */
        void on_read(const int& fd);
        /**
         * Called when network buffer of the file descriptor is available. 
         * 
         * @param fd the associated file descriptor
         */
        void on_write(const int& fd);
        /**
         * Called when the file descriptor is closed unexpectedly. 
         * 
         * @param fd the associated file descriptor
         */
        void on_close(const int& fd);
    public:
        /**
         * Constructor. This creates a socket and add the socket to the loop
         * for notifications of read events.
         *
         * @param port the port to bind and listen to
         * @param loop the IO loop that drives the server
         */
        AsyncHttpServer(const int& port, IOLoop* const loop=NULL);
        /**
         * Destructor. 
         */
        ~AsyncHttpServer();
        /**
         * Adds the handler for requests matching the pattern. Note that, unlike
         * AsyncHttpClient, this class keeps the handler until you explicitly
         * delete (de-allocate the memory of) that handler yourself.
         *
         * @param pattern the pattern associated with the handler
         * @param handler the request handler for reqeusts matching the pattern
         */
        void add_handler(const string& pattern, 
            HttpRequestHandler* const handler);
        /**
         * Removes the first found handler of the pattern and returns the 
         * removed handler or NULL if no handler is removed.
         *
         * @param pattern the pattern associated with the handler
         */
        HttpRequestHandler* remove_handler(const string& pattern);
};

/**
 * IOLoop wraps epoll Edge Triggered and notifies registered handlers of network 
 * events. Examples of handlers are AsyncHttpClient and AsyncHttpServer.
 *
 * TODO: (1) IOLoop runs forever at the moment. Maybe, adding timeout in start().
 */
class IOLoop {
    private:
        int fd;
        map<int, IOHandler*> handlers;
        static IOLoop* loop;
    public:
        /**
         * Constructor. 
         */
        IOLoop();
        /**
         * Sets the handler for either read events or write events on the file 
         * descriptor and returns the previously set handler or NULL if no 
         * handler was previously set. 
         *
         * @param fd the associated file descriptor
         * @param handler the handler to notify of read events
         * @param mode 'r' for read events or else for write events
         */
        IOHandler* set_handler(const int& fd, IOHandler* const handler, 
            char mode='r');
        /**
         * Unsets the handler for all events on the file descriptor and returns
         * the previously set handler or NULL if no handler was previously set.
         *
         * @param fd the associated file descriptor
         */
        IOHandler* unset_handler(const int& fd);
        /**
         * Starts the I/O loop forever. 
         */
        void start();
        /**
         * Returns the singleton instance of the I/O loop. 
         */
        static IOLoop* instance();
};

#endif
