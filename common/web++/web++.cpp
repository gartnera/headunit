#include "web++.hpp"
#include <condition_variable>
#include <mutex>
#include <chrono>


namespace  {
using namespace std;
using namespace WPP;
struct MimeList {
    map<string, string> mime;
    MimeList() {
        // Mime
        mime["atom"] = "application/atom+xml";
        mime["hqx"] = "application/mac-binhex40";
        mime["cpt"] = "application/mac-compactpro";
        mime["mathml"] = "application/mathml+xml";
        mime["doc"] = "application/msword";
        mime["bin"] = "application/octet-stream";
        mime["dms"] = "application/octet-stream";
        mime["lha"] = "application/octet-stream";
        mime["lzh"] = "application/octet-stream";
        mime["exe"] = "application/octet-stream";
        mime["class"] = "application/octet-stream";
        mime["so"] = "application/octet-stream";
        mime["dll"] = "application/octet-stream";
        mime["dmg"] = "application/octet-stream";
        mime["oda"] = "application/oda";
        mime["ogg"] = "application/ogg";
        mime["pdf"] = "application/pdf";
        mime["ai"] = "application/postscript";
        mime["eps"] = "application/postscript";
        mime["ps"] = "application/postscript";
        mime["rdf"] = "application/rdf+xml";
        mime["smi"] = "application/smil";
        mime["smil"] = "application/smil";
        mime["gram"] = "application/srgs";
        mime["grxml"] = "application/srgs+xml";
        mime["air"] = "application/vnd.adobe.apollo-application-installer-package+zip";
        mime["mif"] = "application/vnd.mif";
        mime["xul"] = "application/vnd.mozilla.xul+xml";
        mime["xls"] = "application/vnd.ms-excel";
        mime["ppt"] = "application/vnd.ms-powerpoint";
        mime["rm"] = "application/vnd.rn-realmedia";
        mime["wbxml"] = "application/vnd.wap.wbxml";
        mime["wmlc"] = "application/vnd.wap.wmlc";
        mime["wmlsc"] = "application/vnd.wap.wmlscriptc";
        mime["vxml"] = "application/voicexml+xml";
        mime["bcpio"] = "application/x-bcpio";
        mime["vcd"] = "application/x-cdlink";
        mime["pgn"] = "application/x-chess-pgn";
        mime["cpio"] = "application/x-cpio";
        mime["csh"] = "application/x-csh";
        mime["dcr"] = "application/x-director";
        mime["dir"] = "application/x-director";
        mime["dxr"] = "application/x-director";
        mime["dvi"] = "application/x-dvi";
        mime["spl"] = "application/x-futuresplash";
        mime["gtar"] = "application/x-gtar";
        mime["hdf"] = "application/x-hdf";
        mime["js"] = "application/x-javascript";
        mime["latex"] = "application/x-latex";
        mime["sh"] = "application/x-sh";
        mime["shar"] = "application/x-shar";
        mime["swf"] = "application/x-shockwave-flash";
        mime["sit"] = "application/x-stuffit";
        mime["sv4cpio"] = "application/x-sv4cpio";
        mime["sv4crc"] = "application/x-sv4crc";
        mime["tar"] = "application/x-tar";
        mime["tcl"] = "application/x-tcl";
        mime["tex"] = "application/x-tex";
        mime["man"] = "application/x-troff-man";
        mime["me"] = "application/x-troff-me";
        mime["ms"] = "application/x-troff-ms";
        mime["xml"] = "application/xml";
        mime["xsl"] = "application/xml";
        mime["xhtml"] = "application/xhtml+xml";
        mime["xht"] = "application/xhtml+xml";
        mime["dtd"] = "application/xml-dtd";
        mime["xslt"] = "application/xslt+xml";
        mime["zip"] = "application/zip";
        mime["mp3"] = "audio/mpeg";
        mime["mpga"] = "audio/mpeg";
        mime["mp2"] = "audio/mpeg";
        mime["m3u"] = "audio/x-mpegurl";
        mime["wav"] = "audio/x-wav";
        mime["pdb"] = "chemical/x-pdb";
        mime["xyz"] = "chemical/x-xyz";
        mime["bmp"] = "image/bmp";
        mime["cgm"] = "image/cgm";
        mime["gif"] = "image/gif";
        mime["ief"] = "image/ief";
        mime["jpg"] = "image/jpeg";
        mime["jpeg"] = "image/jpeg";
        mime["jpe"] = "image/jpeg";
        mime["png"] = "image/png";
        mime["svg"] = "image/svg+xml";
        mime["wbmp"] = "image/vnd.wap.wbmp";
        mime["ras"] = "image/x-cmu-raster";
        mime["ico"] = "image/x-icon";
        mime["pnm"] = "image/x-portable-anymap";
        mime["pbm"] = "image/x-portable-bitmap";
        mime["pgm"] = "image/x-portable-graymap";
        mime["ppm"] = "image/x-portable-pixmap";
        mime["rgb"] = "image/x-rgb";
        mime["xbm"] = "image/x-xbitmap";
        mime["xpm"] = "image/x-xpixmap";
        mime["xwd"] = "image/x-xwindowdump";
        mime["css"] = "text/css";
        mime["html"] = "text/html";
        mime["htm"] = "text/html";
        mime["txt"] = "text/plain";
        mime["asc"] = "text/plain";
        mime["rtx"] = "text/richtext";
        mime["rtf"] = "text/rtf";
        mime["tsv"] = "text/tab-separated-values";
        mime["wml"] = "text/vnd.wap.wml";
        mime["wmls"] = "text/vnd.wap.wmlscript";
        mime["etx"] = "text/x-setext";
        mime["mpg"] = "video/mpeg";
        mime["mpeg"] = "video/mpeg";
        mime["mpe"] = "video/mpeg";
        mime["flv"] = "video/x-flv";
        mime["avi"] = "video/x-msvideo";
        mime["movie"] = "video/x-sgi-movie";
    }
};

void list_dir(Request& req, Response& res) {
    unsigned char isFile = 0x8, isFolder = 0x4;
    struct dirent *dir;
    int status;
    struct stat st_buf;

    static MimeList mlist;
    auto& mime = mlist.mime;

    char* actual_path;
    char* base_path = realpath(req.params.c_str(), NULL);
    string new_path = "";
    actual_path = realpath(req.params.c_str(), NULL);

    if(req.query.find("open") != req.query.end()) {
        new_path += req.query["open"];
        strcat(actual_path, new_path.c_str());
    }

    // prevent directory traversal
    char* effective_path = realpath(actual_path, NULL);
    if ((effective_path != NULL) && (strncmp(base_path, effective_path, strlen(base_path)) != 0)) {
        free(actual_path);
        actual_path = base_path;
        new_path = "";
    }
    free(effective_path);
    effective_path = NULL;

    status = stat(actual_path, &st_buf);

    if (status != 0)  {
        res.code = 404;
        res.phrase = "Not Found";
        res.type = "text/plain";
        res.body << "Not found";
    } else if (S_ISREG (st_buf.st_mode)) {
        size_t ext_pos = string(actual_path).find_last_of(".");

        map<string, string>::iterator ext = mime.find(string(actual_path).substr(ext_pos + 1));

        if(ext != mime.end()) {
            res.type = ext->second;
        } else {
            res.type = "application/octet-stream";
        }

        ifstream ifs(actual_path);
        res.body << ifs.rdbuf();
    } else if (S_ISDIR (st_buf.st_mode)) {
        DIR* dir_d = opendir(actual_path);

        if (dir_d == NULL) throw WPP::Exception("Unable to open / folder");

        auto& out = res.body;
        out << "<title>" << new_path << "</title>" << endl;
        out << "<table>";

        while((dir = readdir(dir_d))) {
            out << "<tr><td><a href=\"" << req.path << "?open=" << new_path << "/" << dir->d_name << """\">";

            if (dir->d_type == isFolder) {
                out << "[" << dir->d_name << "]";
            } else {
                out << " " << dir->d_name << "";
            }

            out << "</a></td></tr>";
        }

        out << "</table>";

        closedir(dir_d);
    }

    if (actual_path != base_path) {
        free(actual_path);
    }
    free(base_path);
}

}

namespace WPP {



Response::Response() {
    code = 200;
    phrase = "OK";
    type = "text/html";
    body << "";

    // set current date and time for "Date: " header
    char buffer[100];
    time_t now = time(0);
    struct tm tstruct = *gmtime(&now);
    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S %Z", &tstruct);
    date = buffer;
}


void Server::split(string str, string separator, int max, vector<string>* results){
    int i = 0;
    size_t found = str.find_first_of(separator);

    while(found != string::npos){
        if(found > 0){
            results->push_back(str.substr(0, found));
        }
        str = str.substr(found+1);
        found = str.find_first_of(separator);

        if(max > -1 && ++i == max) break;
    }

    if(str.length() > 0){
        results->push_back(str);
    }
}

string Server::trim(string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());

    return s;
}

void Server::parse_headers(char* headers, Request* req, Response* res) {
    // Parse request headers
    int i = 0;
    char * pch;
    char* next_token;
    for (pch = strtok_r(headers, "\n", &next_token); pch; pch = strtok_r(NULL, "\n", &next_token ))
    {
        if(i++ == 0)  {
            vector<string> R;
            string line(pch);
            this->split(line, " ", 3, &R);

//            cout << R.size() << endl;

            if(R.size() != 3) {
//                throw error
            }

            req->method = R[0];
            req->path = R[1];

            size_t pos = req->path.find('?');

            // We have GET params here
            if(pos != string::npos)  {
                vector<string> Q1;
                this->split(req->path.substr(pos + 1), "&", -1, &Q1);

                for(vector<string>::size_type q = 0; q < Q1.size(); q++) {
                    vector<string> Q2;
                    this->split(Q1[q], "=", -1, &Q2);

                    if(Q2.size() == 2) {
                        req->query[Q2[0]] = Q2[1];
                    }
                }

                req->path = req->path.substr(0, pos);
            }
        } else {
            vector<string> R;
            string line(pch);
            this->split(line, ": ", 2, &R);

            if(R.size() == 2) {
                req->headers[R[0]] = R[1];

                // Yeah, cookies!
                if(R[0] == "Cookie") {
                    vector<string> C1;
                    this->split(R[1], "; ", -1, &C1);

                    for(vector<string>::size_type c = 0; c < C1.size(); c++) {
                        vector<string> C2;
                        this->split(C1[c], "=", 2, &C2);

                        req->cookies[C2[0]] = C2[1];
                    }
                }
            }
        }
    }
}

void Server::get(string path, Route::CallbackSig callback) {
    Route r = {
         path,
         "GET",
         callback
    };

    ROUTES.push_back(r);
}

void Server::post(string path, Route::CallbackSig callback) {
    Route r = {
         path,
         "POST",
         callback
    };

    ROUTES.push_back(r);
}

void Server::all(string path, Route::CallbackSig callback) {
    Route r = {
         path,
         "ALL",
         callback
    };

    ROUTES.push_back(r);
}

void Server::get(string path, string loc) {
    Route r = {
         path,
         "GET",
         &list_dir,
         loc
    };

    ROUTES.push_back(r);
}

void Server::post(string path, string loc) {
    Route r = {
         path,
         "POST",
         &list_dir,
         loc
    };

    ROUTES.push_back(r);
}

void Server::all(string path, string loc) {
    Route r = {
         path,
         "ALL",
         &list_dir,
         loc
    };

    ROUTES.push_back(r);
}

bool Server::match_route(Request* req, Response* res) {
    for (vector<Route>::size_type i = 0; i < ROUTES.size(); i++) {
        if(ROUTES[i].path == req->path && (ROUTES[i].method == req->method || ROUTES[i].method == "ALL")) {
            req->params = ROUTES[i].params;

            ROUTES[i].callback(*req, *res);

            return true;
        }
    }

    return false;
}

std::condition_variable quitcv;
std::mutex quitmutex;

void Server::main_loop() {

    struct sockaddr_in cli_addr;
    socklen_t clilen;
    clilen = sizeof(cli_addr);

    while(true) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(listen_socket, &fds);
        FD_SET(cancelPipeRead, &fds);

        int selret = select(std::max(listen_socket, cancelPipeRead) + 1, &fds, nullptr, nullptr, nullptr);
        if (selret == 0)
        {
            continue;
        }
        else if (selret < 0 || FD_ISSET(cancelPipeRead, &fds))
        {
            break;
        }

        int newsc = accept(listen_socket, (struct sockaddr *) &cli_addr, &clilen);

        if (newsc < 0) {
            continue;
        }

        // handle new connection
        Request req;
        Response res;

        const size_t BUFSIZE = 8192;
        char headers[BUFSIZE + 1];
        long ret = read(newsc, headers, BUFSIZE);
        if(ret > 0 && ret < BUFSIZE) {
            headers[ret] = 0;
        } else {
            headers[0] = 0;
        }

        this->parse_headers(headers, &req, &res);

        if(!this->match_route(&req, &res)) {
            res.code = 404;
            res.phrase = "Not Found";
            res.type = "text/plain";
            res.body << "Not found";
        }

        std::string body_string =  res.body.str();
        std::ostringstream header_buffer;

        // build http response
        header_buffer << "HTTP/1.0 " << res.code << " " << res.phrase << "\r\n";

        // append headers
        header_buffer << "Server:" << SERVER_NAME << " " << SERVER_VERSION << "\r\n";
        header_buffer << "Date:" << res.date << "\r\n";
        header_buffer << "Content-Type:" << res.type << "\r\n";
        header_buffer << "Content-Length:" << body_string.size() << "\r\n";

        for (const auto& header : res.headers)
        {
            header_buffer << header.first << ":" << header.second << "\r\n";
        }

        // append extra crlf to indicate start of body
        header_buffer << "\r\n";
        std::string header_string = header_buffer.str();

        ssize_t t;
        t = write(newsc, header_string.c_str(), header_string.size());
        t = write(newsc, body_string.c_str(), body_string.size());

        close(newsc);

        std::unique_lock <std::mutex> lk(quitmutex);
        if (quitcv.wait_for(lk, std::chrono::milliseconds{0}) == std::cv_status::no_timeout) {
            break;
        }
    }
}

bool Server::start(int port) {
    if (cancelPipeWrite < 0) {
        int pipefds[2];
        pipe(pipefds);
        cancelPipeRead = pipefds[0];
        cancelPipeWrite = pipefds[1];

        listen_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_socket < 0) {
            return false;
        }

        int trueval = 1;
        setsockopt(listen_socket,SOL_SOCKET,SO_REUSEADDR,&trueval,sizeof(trueval));

        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(port);

        if (::bind(listen_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
            return false;
        }

        listen(listen_socket, 5);

        server_thread = std::thread([this](){
                main_loop();
        });
    }

    return true;
}

void Server::stop(){
    quitcv.notify_all();
}

Server::Server() {

}

Server::~Server() {
    if (cancelPipeWrite >= 0) {
        write(cancelPipeWrite, &cancelPipeWrite, 1);
        if (server_thread.joinable())
        {
            server_thread.join();
        }
        close(cancelPipeWrite);
        close(cancelPipeRead);
        shutdown(listen_socket, SHUT_RDWR);
        close(listen_socket);
    }
}

}
