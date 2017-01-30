#include "ub_bluetooth.h"

#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <array>

std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
        if (fgets(buffer.data(), 128, pipe.get()) != NULL)
            result += buffer.data();
    }
    return result;
}

std::string get_bluetooth_mac_address() {
    const auto output = exec("hcitool dev");
    // This is naive and simple, but works well for testing.
    const auto start_of_mac = output.find_last_of('\t');
    if (start_of_mac == std::string::npos) return "";
    return output.substr(start_of_mac);
}