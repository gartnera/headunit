//## The MIT License

//Copyright (c) Alex Movsisyan

//Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the 'Software'), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

//The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

//THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

//https://github.com/konteck/wpp

//Modified to be more C++11-ish

#include <dirent.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <functional>
#include <thread>

#define SERVER_NAME "Web++"
#define SERVER_VERSION "1.1.0"


namespace WPP {
    using namespace std;
    class Request {
        public:
            Request() {

            }
            std::string method;
            std::string path;
            std::string params;
            map<string, string> headers;
            map<string, string> query;
            map<string, string> cookies;
    };

    class Response {
        public:
            Response();
            int code;
            string phrase;
            string type;
            string date;
            ostringstream body;
            map<string, string> headers;
    };

    class Exception : public std::exception {
        public:
            Exception() : pMessage() {}
            Exception(const char* pStr) : pMessage(pStr) {}
            Exception(const string& pStr) : pMessage(pStr) {}
            const char* what() const throw () { return pMessage.c_str(); }
        private:
            string pMessage;
    };

    struct Route {
        string path;
        string method;
        typedef std::function<void(Request&, Response&)> CallbackSig;
        CallbackSig callback;
        string params;
    };

    class Server {
        public:
            void get(string, Route::CallbackSig);
            void post(string, Route::CallbackSig);
            void all(string, Route::CallbackSig);
            void get(string, string);
            void post(string, string);
            void all(string, string);
            bool start(int port = 80);
            void stop();

            Server();
            ~Server();
        private:
            void main_loop();
            void parse_headers(char*, Request*, Response*);
            bool match_route(Request*, Response*);
            string trim(string);
            void split(string, string, int, vector<string>*);
            std::vector<Route> ROUTES;
            int cancelPipeRead = -1, cancelPipeWrite = -1;
            int listen_socket = -1;
            std::thread server_thread;
    };

}
