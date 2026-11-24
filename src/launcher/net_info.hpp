// SimmyLauncher: what the Network play page shows about this PC: its LAN IPv4 addresses (to tell the
// friend who joins), and a light check of a typed address.
#pragma once
#include <string>
#include <vector>

namespace launcher {

struct LanAddress {
  std::string address;  // "192.168.1.20"
  std::string adapter;  // the network adapter's name ("Ethernet", "Wi-Fi", "wlan0"), may be empty
  bool primary = false; // the adapter the PC's default route uses (where the internet is)
  bool virt = false;    // looks like a virtual machine's or container's adapter (listed last)
};

// This PC's IPv4 addresses, most useful first: the default route's, then private LAN ranges (192.168.x,
// 10.x, 172.16-31.x), then others; loopback and link-local (169.254.x) left out. At most 4.
// Windows: GetAdaptersAddresses; elsewhere getifaddrs; plus a UDP "connect" (nothing is sent) for the
// default route's address on both.
std::vector<LanAddress> lan_ipv4_addresses();

// A typed host address: an IPv4 address or a host name, optionally ":port". Returns "" when it looks
// usable (host and port filled in), else a short reason for the player.
std::string check_address(const std::string& text, std::string& host, int& port);

}  // namespace launcher
