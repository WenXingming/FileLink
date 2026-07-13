#include "RequestAuthenticator.h"

#include "AuthService.h"

#include <cctype>

namespace filelink {

namespace {

std::string trim_whitespace(const std::string& value) {
    std::size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }
    return value.substr(first, last - first);
}

bool read_session_token(const HttpRequest& request, std::string& session_token) {
    const std::string& cookies = request.get_header("Cookie");
    bool found = false;
    std::size_t begin = 0;

    while (begin <= cookies.size()) {
        const std::size_t end = cookies.find(';', begin);
        const std::string cookie = trim_whitespace(cookies.substr(begin, end - begin));
        const std::size_t equals = cookie.find('=');
        if (equals != std::string::npos && trim_whitespace(cookie.substr(0, equals)) == "filelink_session") {
            if (found) {
                return false;
            }
            session_token = trim_whitespace(cookie.substr(equals + 1));
            found = true;
        }

        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }

    return found && !session_token.empty();
}

} // namespace

RequestAuthResult RequestAuthenticator::authenticate(const HttpRequest& request,
    AuthenticatedUser& out_user) const {
    std::string session_token;
    if (!this->session_token(request, session_token)) {
        return RequestAuthResult::Unauthorized;
    }

    switch (auth_service_.current_user(session_token, out_user)) {
    case CurrentUserResult::Success:
        return RequestAuthResult::Authenticated;
    case CurrentUserResult::InvalidSession:
        return RequestAuthResult::Unauthorized;
    case CurrentUserResult::SystemError:
        return RequestAuthResult::SystemError;
    }
    return RequestAuthResult::SystemError;
}

bool RequestAuthenticator::session_token(const HttpRequest& request, std::string& out_token) const {
    return read_session_token(request, out_token);
}

} // namespace filelink
