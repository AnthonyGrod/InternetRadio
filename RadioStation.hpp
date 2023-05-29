#include <string>

class RadioStation {
public:
    std::string name;
    std::string ip_address;
    int port;
    size_t last_received_lookup_seconds;

    RadioStation(std::string name, std::string ip_address, int port) {
        this->name = name;
        this->ip_address = ip_address;
        this->port = port;
        this->last_received_lookup_seconds = time(NULL);
    }

    RadioStation() {
        this->name = "";
        this->ip_address = "";
        this->port = 0;
        this->last_received_lookup_seconds = 0;
    }

    bool operator==(const RadioStation &other) const {
        return this->name == other.name && this->ip_address == other.ip_address && this->port == other.port;
    }
};