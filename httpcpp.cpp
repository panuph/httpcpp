#include <errno.h>
#include <fcntl.h>
#include <regex.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include <sstream>
#include <iostream>
#include <stdexcept>

#include "httpcpp.h"

// HttpRequest

HttpRequest* HttpRequest::from_sequence(const string& sequence) {
    size_t p0 = sequence.find("\r\n\r\n");
    if (p0 != string::npos) {
        p0 += 4;
        size_t p1 = sequence.find(" ");
        string method = sequence.substr(0, p1);
        size_t p2 = sequence.find(" ", ++p1);
        string path = sequence.substr(p1, p2 - p1);
        size_t p3 = sequence.find("Content-Length:");
        if (p3 != string::npos) {
            p3 += 15;
            size_t p4 = sequence.find("\r\n", p3);
            int length = atoi(sequence.substr(p3, p4 - p3).data());
            if (sequence.size() >= p0 + length) {
                string body = sequence.substr(p0, length);
                return new HttpRequest(method, path, body);
            } 
        } else {
            return new HttpRequest(method, path);
        } 
    }
    return NULL;
}

HttpRequest::HttpRequest(const string& method, const string& path, 
                         const string& body) {
    this->method = method;
    this->path = path;
    this->body = body;
}

const string& HttpRequest::get_method() {
    return this->method;
}

const string& HttpRequest::get_path() {
    return this->path;
}

const string& HttpRequest::get_body() {
    return this->body;
}

// HttpResponse

const string HttpResponse::to_sequence(int code, const string& body) {
    stringstream packet;
    string reason;
    switch (code) {
        case 100: reason = "Continue"; break;
        case 101: reason = "Switching Protocols"; break;
        case 200: reason = "OK"; break;
        case 201: reason = "Created"; break;
        case 202: reason = "Accepted"; break;
        case 203: reason = "Non-Authoritative Information"; break;
        case 204: reason = "No Content"; break;
        case 205: reason = "Reset Content"; break;
        case 206: reason = "Partial Content"; break;
        case 300: reason = "Multiple Choices"; break;
        case 301: reason = "Moved Permanently"; break;
        case 302: reason = "Found"; break;
        case 303: reason = "See Other"; break;
        case 304: reason = "Not Modified"; break;
        case 305: reason = "Use Proxy"; break;
        case 307: reason = "Temporary Redirect"; break;
        case 400: reason = "Bad Request"; break;
        case 401: reason = "Unauthorized"; break;
        case 403: reason = "Forbidden"; break;
        case 404: reason = "Not Found"; break;
        case 405: reason = "Method Not Allowed"; break;
        case 406: reason = "Not Acceptable"; break;
        case 407: reason = "Proxy Authentication Required"; break;
        case 408: reason = "Request Timeout"; break;
        case 409: reason = "Conflict"; break;
        case 410: reason = "Gone"; break;
        case 411: reason = "Length Required"; break;
        case 412: reason = "Precondition Failed"; break;
        case 413: reason = "Request Entity Too Large"; break;
        case 414: reason = "Request-URI Too Long"; break;
        case 415: reason = "Unsupported Media Type"; break;
        case 416: reason = "Requested Range Not Satisfiable"; break;
        case 417: reason = "Expectation Failed"; break;
        case 500: reason = "Internal Server Error"; break;
        case 501: reason = "Not Implemented"; break;
        case 502: reason = "Bad Gateway"; break;
        case 503: reason = "Service Unavailable"; break;
        case 504: reason = "Gateway Timeout"; break;
        case 505: reason = "HTTP Version Not Supported"; break;
        default: code = 500; reason = "Internal Server Error"; break;
    }
    packet << "HTTP/1.0 " << code << " " << reason << "\r\n";
    packet << "Content-Length: " << body.size() << "\r\n\r\n";
    packet << body;
    return packet.str();
}

HttpResponse* HttpResponse::from_sequence(const string& sequence) {
    // the algorithm only works if Content-Length exists
    size_t p0 = sequence.find("\r\n\r\n");
    if (p0 != string::npos) {
        p0 += 4;
        size_t p1 = sequence.find("Content-Length:") + 15; 
        size_t p2 = sequence.find("\r\n", p1);
        int length = atoi(sequence.substr(p1, p2 - p1).data());
        if (sequence.size() >= p0 + length) {
            size_t p1 = sequence.find(" ");
            size_t p2 = sequence.find(" ", ++p1);
            int code = atoi(sequence.substr(p1, p2 - p1).data());
            string body = sequence.substr(p0, length);
            return new HttpResponse(code, body);
        }
    }
    return NULL;
}

HttpResponse::HttpResponse(const int& code, const string& body) {
    this->code = code;
    this->body = body;
}

const int& HttpResponse::get_code() {
    return this->code;
}

const string& HttpResponse::get_body() {
    return this->body;
}

// HttpRequestHandler

void HttpRequestHandler::reply(HttpRequest* const request, const int& code, 
    const string& body) {
    if (request->done) {
        throw runtime_error("Reply to reqeust is already done");
    } else {
        request->server->reply(request->fd, code, body);
        request->done = true;
    }
}

void HttpRequestHandler::get(HttpRequest* const request,
    const vector<string>& args) {
    this->reply(request, 405);
}

void HttpRequestHandler::post(HttpRequest* const request,
    const vector<string>& args) {
    this->reply(request, 405);
}

// IOHandler

void IOHandler::clear_buffers(const int& fd) {
    this->read_buffers.erase(fd);
    this->write_buffers.erase(fd);
}

// AsyncHttpClient

void AsyncHttpClient::on_read(const int& fd) {
    char buffer[BUFFER_SIZE];
    bool done = false;
    bool error = false;
    while (true) {
        ssize_t n = read(fd, buffer, BUFFER_SIZE);
        if (n > 0) { 
            this->read_buffers[fd].append(buffer, n);
        } else if (n == 0) { 
            // somehow it gets n=0 instead of n=-1 with errno=EAGAIN
            HttpResponse* response = 
                HttpResponse::from_sequence(this->read_buffers[fd]);
            if (response != NULL) {
                this->handlers[fd]->handle(response);
                delete response;
            } else {
                error = true;
            }
            done = true;
            // delete the handler to de-allocate the memory
            delete this->handlers[fd];
            break;
        } else {
            if (errno == EAGAIN) {
                // try again later
            } else {
                done = true;
            } 
            break;
        }
    }
    if (done) {
        this->on_close(fd);
    }
    if (error) {
        throw runtime_error("AsyncHttpClient read error");
    }
}

void AsyncHttpClient::on_write(const int& fd) {
    bool error = false;
    int n_is_zero = 0;
    while (true) {
        size_t size = this->write_buffers[fd].size();
        ssize_t n = write(fd, this->write_buffers[fd].data(), size);
        if (n > 0) {
            this->write_buffers[fd].erase(0, n);
        } else if (n == 0) {
            // somehow it gets n=0 instead of n=-1 with errno=EAGAIN
            n_is_zero++;
            if (this->write_buffers[fd].size() == 0) {
                // prepare the read buffer
                this->clear_buffers(fd);
                this->read_buffers[fd] = string();
                this->loop->set_handler(fd, this);
                break;
            } else {
                if (n_is_zero == 3) {
                    error = true;
                    break;
                }
            } 
        } else {
            if (errno == EAGAIN) {
                // try again later
            } else {
                error = true;
            }
            break;
        }
    }
    if (error) {
        this->on_close(fd);
    }
}

void AsyncHttpClient::on_close(const int& fd) {
    this->clear_buffers(fd);
    this->handlers.erase(fd);
    close(fd);
}

AsyncHttpClient::AsyncHttpClient(IOLoop* const loop) {
    // set the IO loop
    if (loop == NULL) {
        this->loop = IOLoop::instance();
    } else {
        this->loop = loop;
    }
}

void AsyncHttpClient::fetch(const string& host, const int& port, 
    const string& method, const string& path, const string& body, 
    HttpResponseHandler* const handler) {
    int fd;
    struct sockaddr_in addr;
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        throw runtime_error(strerror(errno));
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_aton(host.data(), &addr.sin_addr) <= 0) {
        throw runtime_error(strerror(errno));
    }
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        throw runtime_error(strerror(errno));
    }
    stringstream packet;
    packet << method << " " << path << " HTTP/1.0\r\n" <<
        "Content-Length: " << body.size() << "\r\n\r\n" << body;
    // set the write buffer and the handler.
    this->clear_buffers(fd);
    this->write_buffers[fd] = packet.str();
    this->handlers[fd] = handler;
    this->loop->set_handler(fd, this, 'w');
}

// AsyncHttpServer

HttpRequestHandler* AsyncHttpServer::find_handler(const string& path) {
    vector<pair<string, HttpRequestHandler*> >::iterator it;
    for (it = this->handlers.begin(); it != this->handlers.end(); it++) {
        regex_t preg;
        if (regcomp(&preg, (*it).first.data(), REG_EXTENDED | REG_NOSUB) == 0) {
            if (regexec(&preg, path.data(), 0, NULL, 0) == 0) {
                regfree(&preg);
                return (*it).second;
            }
            regfree(&preg);
        }
    }
    return NULL;
}

vector<string> AsyncHttpServer::get_arguments(const string& path) {
    vector<string> args;
    vector<pair<string, HttpRequestHandler*> >::iterator it;
    for (it = this->handlers.begin(); it != this->handlers.end(); it++) {
        regex_t preg;
        if (regcomp(&preg, (*it).first.data(), REG_EXTENDED) == 0) {
            size_t nmatch = MAX_NMATCH;
            regmatch_t pmatch[nmatch];
            if (regexec(&preg, path.data(), nmatch, pmatch, 0) == 0) {
                for (int i = 1; i < nmatch; i++) {
                    if (pmatch[i].rm_so == -1) {
                        break;
                    }
                    int n = pmatch[i].rm_eo - pmatch[i].rm_so;
                    args.push_back(string(path.data() + pmatch[i].rm_so, n));
                }
                regfree(&preg);
                break;
            }
            regfree(&preg);
        }
    }
    return args;
}

void AsyncHttpServer::reply(const int& fd, const int& code, 
    const string& body) {
    this->clear_buffers(fd);
    this->write_buffers[fd] = HttpResponse::to_sequence(code, body);
}

void AsyncHttpServer::on_read(const int& fd) {
    if (fd == this->fd) {   
        // read on listening socket, keep accepting
        while (true) {
            struct sockaddr_in addr;
            socklen_t addr_len = sizeof(addr);
            int cfd = accept(fd, (struct sockaddr*)&addr, &addr_len);
            if (cfd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;  
                } else {
                    throw runtime_error(strerror(errno));
                }
            } else {
                // prepare the read buffer for the accepted socket
                this->clear_buffers(fd);
                this->read_buffers[fd] = string();
                this->loop->set_handler(cfd, this);
            }
        }

    } else {                
        // read on existing socket, keep reading until EAGAIN
        char buffer[BUFFER_SIZE];
        bool error = false;
        while (true) {
            ssize_t n = read(fd, buffer, BUFFER_SIZE);
            if (n > 0) {            
                this->read_buffers[fd].append(buffer, n);
            } else if (n == 0) {    
                // socket close 
                error = true;
                break;
            } else { 
                if (errno != EAGAIN) {
                    error = true;
                } else {
                    // no more data, try if request is available
                    HttpRequest* request = 
                        HttpRequest::from_sequence(this->read_buffers[fd]);
                    if (request != NULL) {
                        // find a handler to handle the request
                        HttpRequestHandler* handler = 
                            this->find_handler(request->path);
                        if (handler != NULL) {
                            vector<string> args = 
                                this->get_arguments(request->path);
                            request->server = this;
                            request->fd = fd;
                            request->done = false;
                            if (request->method.compare("GET") == 0) {
                                handler->get(request, args);
                            } else if (request->method.compare("POST") == 0) {
                                handler->post(request, args);
                            } else {
                                handler->reply(request, 405);
                            }
                        } else {
                            this->reply(fd, 404);
                        }
                        if (!request->done) {
                            this->reply(fd, 500);
                        }
                        delete request;
                        this->loop->set_handler(fd, this, 'w'); 
                        break;
                    }
                }
                break;
            }
        }
        if (error) {
            this->on_close(fd);
        }
    }
}

void AsyncHttpServer::on_write(const int& fd) {
    bool done = false;
    bool error = false;
    int n_is_zero = 0;
    while (true) {
        size_t size = this->write_buffers[fd].size();
        ssize_t n = write(fd, this->write_buffers[fd].data(), size);
        if (n > 0) {
            this->write_buffers[fd].erase(0, n); 
        } else if (n == 0) {
            // somehow it gets n=0 instead of n=-1 with errno=EAGAIN
            n_is_zero++;
            if (this->write_buffers[fd].size() == 0) {
                done = true;
                break;
            } else {
                if (n_is_zero == 3) {
                    error = true;
                    break;
                }
            }
        } else {
            if (errno == EAGAIN) {
                // try again later
            } else {
                error = true;
            }
            break;
        }
    }
    if (done || error) {
        this->on_close(fd);
    }
    if (error) {
        throw runtime_error("AsyncHttpServer write error");
    }
}

void AsyncHttpServer::on_close(const int& fd) {
    this->clear_buffers(fd);
    this->loop->unset_handler(fd);
    close(fd);
}

AsyncHttpServer::AsyncHttpServer(const int& port, IOLoop* const loop) {
    // set the IO loop
    if (loop == NULL) {
        this->loop = IOLoop::instance();
    } else {
        this->loop = loop;
    }
    // create a socket, bind and listen to the port
    if ((this->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        throw runtime_error(strerror(errno));
    }
    int opt = 1;
    if (setsockopt(this->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw runtime_error(strerror(errno));
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(this->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        throw runtime_error(strerror(errno));
    }
    if (listen(this->fd, LISTEN_BACKLOG) < 0) {
        throw runtime_error(strerror(errno));
    } 
    // set itself as the read handler for the socket
    this->loop->set_handler(this->fd, this);
}

AsyncHttpServer::~AsyncHttpServer() {
    vector<pair<string, HttpRequestHandler*> >::iterator it;
    for (it = this->handlers.begin(); it != this->handlers.end(); it++) {
        delete (*it).second;
    }
    this->read_buffers.clear();
    this->write_buffers.clear(); 
    this->handlers.clear();
}

void AsyncHttpServer::add_handler(const string& pattern, 
    HttpRequestHandler* const handler) {
    this->handlers.push_back(make_pair(pattern, handler));
}

HttpRequestHandler* AsyncHttpServer::remove_handler(const string& pattern) {
    HttpRequestHandler* removed = NULL;
    vector<pair<string, HttpRequestHandler*> >::iterator it;
    for (it = this->handlers.begin(); it != this->handlers.end(); it++) {
        if ((*it).first.compare(pattern) == 0) {
            this->handlers.erase(it);
            removed = (*it).second;
            break;
        }
    }
    return removed;
}

// IOLoop

IOLoop* IOLoop::loop = new IOLoop();

IOLoop::IOLoop() {
    this->fd = epoll_create(EPOLL_SIZE);
}

IOHandler* IOLoop::set_handler(const int& fd, IOHandler* const handler, 
    char mode) {
    // set the socket non-blocking
    int flags; 
    if ((flags = fcntl(fd, F_GETFL, 0)) < 0) {
        throw runtime_error(strerror(errno));
    }
    flags = flags | O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) {
        throw runtime_error(strerror(errno));
    }
    // add the socket to epoll
    struct epoll_event event;
    event.data.fd = fd;
    if (mode == 'r') {
        event.events = EPOLLIN | EPOLLET;
    } else {
        event.events = EPOLLOUT | EPOLLET;
    }
    // unset the previous handler if any and set the new one 
    IOHandler* previous = this->unset_handler(fd);
    if (epoll_ctl(this->fd, EPOLL_CTL_ADD, fd, &event) < 0) {
        throw runtime_error(strerror(errno));
    }
    this->handlers[fd] = handler;
    return previous;
}

IOHandler* IOLoop::unset_handler(const int& fd) {
    if (epoll_ctl(this->fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
        if (errno != ENOENT) { 
            throw runtime_error(strerror(errno));
        }
    }
    if (this->handlers.count(fd) == 0) {
        return NULL;
    } else {
        IOHandler* found = this->handlers[fd];
        this->handlers.erase(fd);
        return found;
    }
} 

void IOLoop::start() {
    // at the moment run forever unless an error occurs
    struct epoll_event* events = (struct epoll_event*)malloc(
        sizeof(struct epoll_event) * MAX_EVENTS);
    while (true) {
        int n;
        if ((n = epoll_wait(this->fd, events, MAX_EVENTS, -1)) < 0) {
            throw runtime_error(strerror(errno));
        }
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
                this->handlers[fd]->on_close(fd);
                this->unset_handler(fd);
                close(fd);
            } 
            else if (events[i].events & EPOLLOUT) {
                this->handlers[fd]->on_write(fd);
            }
            else if (events[i].events & EPOLLIN) {
                this->handlers[fd]->on_read(fd);
            } 
        }
    }
}

IOLoop* IOLoop::instance() {
    return IOLoop::loop;
}
