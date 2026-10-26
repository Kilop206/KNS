#pragma once

#include <cstdlib>

#include <httplib.h>

namespace kns::app::http {

inline void configureClientTrust(httplib::Client& client)
{
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    const char* file = std::getenv("SSL_CERT_FILE");
    const char* directory = std::getenv("SSL_CERT_DIR");
    if ((file && *file) || (directory && *directory)) {
        client.set_ca_cert_path(file ? file : "", directory ? directory : "");
    }
#else
    (void)client;
#endif
}

} // namespace kns::app::http
