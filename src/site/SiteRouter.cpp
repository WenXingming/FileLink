#include "site/SiteRouter.h"

#include "ApiResponseView.h"

#include <cctype>
#include <exception>

namespace filelink {

void SiteRouter::register_routes() {
    server_.add_get_route("/", [this](const HttpRequest& request, HttpResponse& response) {
        handle_index(request, response);
    });
    server_.add_get_route("/index.html", [this](const HttpRequest& request, HttpResponse& response) {
        handle_index(request, response);
    });
    server_.add_get_route("/health", [this](const HttpRequest& request, HttpResponse& response) {
        handle_health(request, response);
    });
    server_.add_prefix_route("/static/", [this](const HttpRequest& request, HttpResponse& response) {
        handle_static(request, response);
    });
}

void SiteRouter::handle_index(const HttpRequest&, HttpResponse& response) {
    try {
        response = ApiResponseView::file(static_file_service_.get_asset_content("/index.html"), ".html");
    }
    catch (const std::exception&) {
        response = ApiResponseView::error(404, "index.html not found");
    }
}

void SiteRouter::handle_health(const HttpRequest&, HttpResponse& response) {
    response = ApiResponseView::health_check();
}

void SiteRouter::handle_static(const HttpRequest& request, HttpResponse& response) {
    try {
        const std::string path = request.get_path();
        std::string extension;
        const std::size_t dot_position = path.find_last_of('.');
        if (dot_position != std::string::npos) {
            extension = path.substr(dot_position);
            for (char& character : extension) {
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }
        }
        response = ApiResponseView::file(static_file_service_.get_asset_content(path), extension);
    }
    catch (const std::invalid_argument& error) {
        response = ApiResponseView::error(403, error.what());
    }
    catch (const std::exception& error) {
        response = ApiResponseView::error(404, error.what());
    }
}

} // namespace filelink
