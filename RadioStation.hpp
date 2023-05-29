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

    static bool compareByName(const RadioStation& a, const RadioStation& b) {
        return a.name < b.name;
    }

    static bool isNameValid(std::string name) {
        if (name.empty() || name.front() == ' ' || name.back() == ' ')
            return false;
        for (const char c : name)
            if (c < 32 || c > 127)
                return false;
        return true;
    }

    static bool isValidMulticastIPv4(std::string ipAddress) {
        // Check if the address is in the multicast range (224.0.0.0 - 239.255.255.255)
        if (ipAddress.length() < 7 || ipAddress.length() > 15) {
            return false;
        }
        unsigned long ip = 0;
        int count = 0;
        size_t start = 0;
        size_t end = ipAddress.find('.');
        while (end != std::string::npos) {
            if (count > 3)
                return false;
            unsigned long part = std::stoul(ipAddress.substr(start, end - start));
            if (part > 255)
                return false;
            ip = (ip << 8) | part;
            start = end + 1;
            end = ipAddress.find('.', start);
            count++;
        }
        if (count != 3)
            return false;
        unsigned long part = std::stoul(ipAddress.substr(start, end));
        if (part > 255)
            return false;
        ip = (ip << 8) | part;
        return (ip >= 0xE0000000 && ip <= 0xEFFFFFFF);
    }

    static bool isValidIPv4Address(const std::string& ipAddress) {
        if (ipAddress == "localhost")
            return true;
        if (ipAddress.length() < 7 || ipAddress.length() > 15)
            return false;
        unsigned long ip = 0;
        int count = 0;
        size_t start = 0;
        size_t end = ipAddress.find('.');
        while (end != std::string::npos) {
            if (count > 3)
                return false;
            unsigned long part = std::stoul(ipAddress.substr(start, end - start));
            if (part > 255)
                return false;
            ip = (ip << 8) | part;
            start = end + 1;
            end = ipAddress.find('.', start);
            count++;
        }
        if (count != 3)
            return false;
        unsigned long part = std::stoul(ipAddress.substr(start, end));
        if (part > 255)
            return false;

        ip = (ip << 8) | part;
        return true;
    }
};