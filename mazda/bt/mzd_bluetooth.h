#pragma once

#include <string>

// This defines a method to retrieve the bluetooth MAC address.
// The actual implementations are in ubuntu/mazda directories due
// to the hardware requirements.

std::string get_bluetooth_mac_address();