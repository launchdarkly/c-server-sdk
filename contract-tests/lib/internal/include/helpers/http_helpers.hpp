#pragma once

#include <httplib.h>
#include <string>

namespace ld {

void write_server_error(httplib::Response &res, const std::string &error);

void write_client_error(httplib::Response &res, const std::string &error);

struct Error {
    int code;
    std::string msg;
};

Error make_client_error(std::string msg);
Error make_server_error(std::string msg);

} // namespace ld
