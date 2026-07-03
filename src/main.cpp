#include "AppConfig.h"

#include "tudou/http/HttpRequest.h"
#include "tudou/http/HttpResponse.h"
#include "tudou/http/HttpServer.h"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    try {
        const std::vector<std::string> args(argv + 1, argv + argc);
        const filelink::AppConfig config = filelink::parse_app_config(args);

        HttpServer server(config.listenAddress, config.port, config.ioThreads);
        server.add_get_route("/health", [](const HttpRequest&, HttpResponse& response) {
            response = HttpResponse::plain_text(200, "OK", R"({"status":"ok"})");
            response.set_header("Content-Type", "application/json");
        });
        server.start();
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "filelink-server: " << error.what() << '\n';
        return 1;
    }
}
