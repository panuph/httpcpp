#include "httpcpp.h"

#include <iostream>

using namespace std;

// client: curl "http://127.0.0.1:8850/a/10"
class HttpRequestHandlerA : public HttpRequestHandler {
    public:
        HttpResponse* get(HttpRequest* const request,
                          const vector<string>& args) {
            cout << "-----------------------------------" << endl;
            cout << "Handler A receives:" << endl;
            cout << "method: " << request->get_method() << endl;
            cout << "path  : " << request->get_path() << endl;
            cout << "body  : " << request->get_body() << endl;
            for (int i = 0; i < args.size(); i++) {
                cout << "arg  : " << args[i] << endl;
            }
            return new HttpResponse(200, "A");
        }
};

// client: curl "http://127.0.0.1:8850/b/10/" -d "abcxyz"
class HttpRequestHandlerB : public HttpRequestHandler {
    public:
        HttpResponse* post(HttpRequest* const request, 
                           const vector<string>& args) {
            cout << "-----------------------------------" << endl;
            cout << "Handler B receives:" << endl;
            cout << "method: " << request->get_method() << endl;
            cout << "path  : " << request->get_path() << endl;
            cout << "body  : " << request->get_body() << endl;
            for (int i = 0; i < args.size(); i++) {
                cout << "arg  : " << args[i] << endl;
            }
            return new HttpResponse(200, "B");
        }
};

class HttpResponseHandlerC : public HttpResponseHandler {
    public:
        void on_receive(HttpResponse* const response) {
            cout << "-----------------------------------" << endl;
            cout << "Handler C receives:" << endl;
            cout << "code  : " << response->get_code() << endl;
            cout << "body  : " << response->get_body() << endl;
        }
};

int main() {
    // server
    AsyncHttpServer* server = new AsyncHttpServer(8850);
    server->add_handler("/a/([[:digit:]]+)", new HttpRequestHandlerA());
    server->add_handler("/b/([[:digit:]]+)", new HttpRequestHandlerB());
    // client
    AsyncHttpClient* client = new AsyncHttpClient();
    client->fetch("127.0.0.1", 8850, "GET", "/a/10", "aaa", 
        new HttpResponseHandlerC());
    client->fetch("127.0.0.1", 8850, "POST", "/b/10", "bbb", 
        new HttpResponseHandlerC());
    // start
    IOLoop::instance()->start();
}
