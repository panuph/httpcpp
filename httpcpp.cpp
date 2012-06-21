#include <errno.h>
#include <fcntl.h>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include "httpcpp.h"

HttpRequest::HttpRequest(const int cfd, const string method, const string path,
                         const string body, AsyncHttpServer* const server) {
    this->cfd = cfd;
    this->method = method;
    this->path = path;
    this->body = body;
    this->server = server;
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

HttpRequest* HttpRequest::from_sequence(const string& sequence, const int cfd,
                                        AsyncHttpServer* const server) {
    // only support GET and POST
    size_t p0 = sequence.find("\r\n\r\n");
    if (p0 != string::npos) {
        p0 += 4;
        size_t p1 = sequence.find(" ");
        string method = sequence.substr(0, p1);
        if (method.compare("GET") == 0) {
            size_t p2 = sequence.find(" ", ++p1);
            string path = sequence.substr(p1, p2 - p1);
            return new HttpRequest(cfd, method, path, "", server);
        } else if (method.compare("POST") == 0) {
            size_t p3 = sequence.find("Content-Length:") + 15; 
            size_t p4 = sequence.find("\r\n", p3);
            int length = atoi(sequence.substr(p3, p4 - p3).data());
            if (sequence.size() >= p0 + length) {
                size_t p2 = sequence.find(" ", ++p1);
                string path = sequence.substr(p1, p2 - p1);
                string body = sequence.substr(p0, length);
                return new HttpRequest(cfd, method, path, body, server);
            }   
        } 
    }   
    return NULL;
}

HttpResponse::HttpResponse(const int code, const string body) {
    this->code = code;
    this->body = body;
}

const int& HttpResponse::get_code() {
    return this->code;
}

const string& HttpResponse::get_body() {
    return this->body;
}

const string HttpResponse::to_sequence() {
    stringstream packet;
    if (this->code == 200) {
        packet << "HTTP/1.0 200 OK\r\n";
    } else if (this->code == 204) {
        packet << "HTTP/1.0 204 No Content\r\n";
    } else if (this->code == 404) {
        packet << "HTTP/1.0 404 Not Found\r\n";
    } else if (this->code == 500) {
        packet << "HTTP/1.0 500 Internal Server Error\r\n";
    } else {
        throw out_of_range("");
    }
    packet << "Content-Length: " << this->body.size() << "\r\n\r\n";
    packet << body;
    return packet.str();
}

HttpResponse* HttpResponse::from_sequence(const string& sequence) {
    // Content-Length is always expected
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

AsyncHttpClient::AsyncHttpClient(IOLoop* const loop) {
    if (loop == NULL) {
        this->loop = IOLoop::instance();
    } else {
        this->loop = loop;
    }
}

void AsyncHttpClient::on_readable(int fd) {
    char buffer[BUFFER_SIZE];
    bool done = false;
    while (true) {
        ssize_t n = read(fd, buffer, BUFFER_SIZE);
        if (n > 0) {     
            this->read_buffers[fd].append(buffer);
        } else if (n == 0) {    // socket close 
            HttpResponse* response = 
                HttpResponse::from_sequence(this->read_buffers[fd]);
            if (response != NULL) {
                this->handlers[fd]->on_receive(response);
                delete response;
                done = true;
                break;
            }
        } else {
            if (errno != EAGAIN) {
                done = true;
            } 
            break;
        }
    }
    if (done) {
        this->read_buffers.erase(fd);
        this->write_buffers.erase(fd);
        this->handlers.erase(fd);
        close(fd);
    }
}

void AsyncHttpClient::on_writable(int fd) {
    bool error = false;
    while (true) {
        size_t size = this->write_buffers[fd].size();
        ssize_t n = write(fd, this->write_buffers[fd].data(), size);
        if (n > 0) {
            this->write_buffers[fd] = this->write_buffers[fd].substr(n);
        } else {
            if (errno == EAGAIN) {
            } else {
                if (this->write_buffers[fd].size() == 0) {
                    this->loop->set_read_handler(fd, this);
                    this->read_buffers[fd] = string();
                } else {
                    error = true;
                }
            }
            break;
        }
    }
    if (error) {
        this->read_buffers.erase(fd);
        this->write_buffers.erase(fd);
        this->handlers.erase(fd);
        close(fd);
    }
}

void AsyncHttpClient::on_error(int fd) {
    this->read_buffers.erase(fd);
    this->write_buffers.erase(fd);
    this->handlers.erase(fd);
    close(fd);
}

int AsyncHttpClient::do_get(const string& host, const int& port, 
                            const string& path, 
                            HttpResponseHandler* const handler) {
    int fd;
    struct sockaddr_in addr;
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_aton(host.data(), &addr.sin_addr) <= 0) {
        return -1;
    }
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return -1;
    }
    string packet = "GET " + path + " HTTP/1.0\r\n\r\n";
    this->write_buffers[fd] = packet;
    this->handlers[fd] = handler;
    this->loop->set_write_handler(fd, this);
    return 0;
}

AsyncHttpServer::AsyncHttpServer(const int port, HttpRequestHandler* const handler, 
                                 IOLoop* const loop) {
    this->handler = handler;
    if (loop == NULL) {
        this->loop = IOLoop::instance();
    } else {
        this->loop = loop;
    }
    // create a non-blocking socket
    this->fd = socket(AF_INET, SOCK_STREAM, 0); 
    int optval = 1;
    setsockopt(this->fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    struct sockaddr_in s_addr;
    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port);
    s_addr.sin_addr.s_addr = INADDR_ANY;
    bind(this->fd, (struct sockaddr*)&s_addr, sizeof(s_addr));
    listen(this->fd, LISTEN_BACKLOG); 
    this->loop->set_read_handler(this->fd, this);
}

AsyncHttpServer::~AsyncHttpServer() {
    delete this->handler;
}

void AsyncHttpServer::on_readable(int fd) {
    if (fd == this->fd) {   // one or more new connections
        while (true) {
            struct sockaddr_in addr;
            socklen_t addr_len = sizeof(addr);
            int cfd = accept(fd, (struct sockaddr*)&addr, &addr_len);
            if (cfd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;  
                } else {
                    // what should we do here?
                }
            } else {
                this->loop->set_read_handler(cfd, this);
                this->read_buffers[fd] = string();
            }
        }

    } else {                // an existing connection
        char buffer[BUFFER_SIZE];
        bool error = false;
        while (true) {
            ssize_t n = read(fd, buffer, BUFFER_SIZE);
            if (n > 0) {            
                this->read_buffers[fd].append(buffer);
            } else if (n == 0) {    // socket close 
                error = true;
                break;
            } else {                
                if (errno != EAGAIN) {
                    error = true;
                } else {
                    HttpRequest* request = HttpRequest::from_sequence(this->read_buffers[fd], fd, this);
                    if (request != NULL) {
                        HttpResponse* response = this->handler->on_receive(request);
                        delete request;
                        this->write_buffers[fd] = response->to_sequence();
                        delete response;
                        this->loop->set_write_handler(fd, this); 
                        break;
                    }
                }
                break;
            }
        }
        if (error) {
            this->read_buffers.erase(fd);
            this->write_buffers.erase(fd);
            close(fd);
        }
    }
}

void AsyncHttpServer::on_writable(int fd) {
    bool done = false;
    while (true) {
        size_t size = this->write_buffers[fd].size();
        ssize_t n = write(fd, this->write_buffers[fd].data(), size);
        if (n > 0) {
            this->write_buffers[fd] = this->write_buffers[fd].substr(n);
        } else {
            if (errno != EAGAIN) {
                done = true;
            } else {
                if (this->write_buffers[fd].size() == 0) {
                    done = true;
                }
            }
            break;
        }
    }
    if (done) {
        this->read_buffers.erase(fd);
        this->write_buffers.erase(fd);
        this->loop->unset_handler(fd);
        close(fd);
    }
}

void AsyncHttpServer::on_error(int fd) {
}

IOLoop* IOLoop::loop = new IOLoop();

IOLoop::IOLoop() {
    this->fd = epoll_create(EPOLL_SIZE);
}

IOLoop::~IOLoop() {
    this->handlers.clear();
}

void IOLoop::set_read_handler(const int& fd, IOHandler* const handler) {
    int flags = fcntl(fd, F_GETFL, 0); 
    flags = flags | O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(this->fd, EPOLL_CTL_DEL, fd, NULL);
    epoll_ctl(this->fd, EPOLL_CTL_ADD, fd, &event);
    this->handlers[fd] = handler;
}

void IOLoop::set_write_handler(const int& fd, IOHandler* const handler) {
    int flags = fcntl(fd, F_GETFL, 0); 
    flags = flags | O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLOUT | EPOLLET;
    epoll_ctl(this->fd, EPOLL_CTL_DEL, fd, NULL);
    epoll_ctl(this->fd, EPOLL_CTL_ADD, fd, &event);
    this->handlers[fd] = handler;
}

void IOLoop::unset_handler(const int& fd) {
    epoll_ctl(this->fd, EPOLL_CTL_DEL, fd, NULL);
    this->handlers.erase(fd);
} 

void IOLoop::start() {
    struct epoll_event* events = (struct epoll_event*)malloc(
        sizeof(struct epoll_event) * MAX_EVENTS);
    while (true) {
        int n = epoll_wait(this->fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
                close(fd);
                this->handlers[fd]->on_error(fd);
                this->handlers.erase(fd);
            } 
            if (events[i].events & EPOLLOUT) {
                this->handlers[fd]->on_writable(fd);
            }
            if (events[i].events & EPOLLIN) {
                this->handlers[fd]->on_readable(fd);
            } 
        }
    }
}

IOLoop* IOLoop::instance() {
    return IOLoop::loop;
}
