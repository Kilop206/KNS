#pragma once

#include <map>
#include <cstdint>
#include <string>

namespace kns { class SimulationEngine; }
namespace gui {
class TranslationService;
class NetworkServicesPanel {
public:
    // True when the user queued traffic; the application can enable Run/Step.
    bool render(kns::SimulationEngine& engine, int device, TranslationService& translations);
    void reset();
private:
    int device_ = -1;
    int kind_ = 0;
    int port_ = 80;
    int destination_ = 0;
    int request_kind_ = 0;
    int request_port_ = 80;
    int http_status_ = 200;
    char name_[65] = "web";
    char key_[513] = "/";
    char body_[4097] = "Hello from KNS";
    char address_[16] = "192.0.2.1";
    char query_[513] = "/";
    char command_[8193] = {};
    std::string selected_;
    std::string editing_service_;
    std::uint64_t editing_revision_ = 0;
    int editing_port_ = 80;
    double editing_delay_ = 0;
    std::string status_;
    std::map<int, std::string> terminal_;
    bool scroll_terminal_ = false;
};
} // namespace gui
