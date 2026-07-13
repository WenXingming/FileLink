#pragma once

#include "tudou/http/HttpRequest.h"

#include <string>

namespace filelink {

class AuthService;
struct AuthenticatedUser;

enum class RequestAuthResult {
    Authenticated,
    Unauthorized,
    SystemError
};

class RequestAuthenticator {
public:
    explicit RequestAuthenticator(AuthService& auth_service) : auth_service_(auth_service) {}

    RequestAuthResult authenticate(const HttpRequest& request, AuthenticatedUser& out_user) const;
    bool session_token(const HttpRequest& request, std::string& out_token) const;

private:
    AuthService& auth_service_;
};

} // namespace filelink
