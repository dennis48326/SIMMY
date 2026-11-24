// SimmyLauncher: this PC's LAN addresses and a typed address's check (see net_info.hpp).
#include "net_info.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

// The system's network headers last (windows.h macros).
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace launcher {

namespace {

std::string Lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

// a.b.c.d -> its four parts; false if not a dotted IPv4 address.
bool ParseIpv4(const std::string& s, unsigned part[4]) {
  int n = 0;
  size_t i = 0;
  while (n < 4) {
    if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    unsigned v = 0;
    size_t digits = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
      v = v * 10 + static_cast<unsigned>(s[i++] - '0');
      if (++digits > 3) return false;
    }
    if (v > 255) return false;
    part[n++] = v;
    if (n < 4) {
      if (i >= s.size() || s[i] != '.') return false;
      ++i;
    }
  }
  return i == s.size();
}

bool Private(const unsigned p[4]) {
  return p[0] == 10 || (p[0] == 192 && p[1] == 168) || (p[0] == 172 && p[1] >= 16 && p[1] <= 31);
}

bool LooksVirtual(const std::string& adapter) {
  static const char* const kWords[] = {"virtual", "vmware", "vbox", "virtualbox", "hyper-v", "vethernet", "docker",
                                       "veth",    "br-",    "virbr", "wsl",        "lxc",     "lxd"};
  const std::string a = Lower(adapter);
  for (const char* w : kWords)
    if (a.find(w) != std::string::npos) return true;
  return false;
}

// The address of the interface the default route uses: a UDP socket "connected" to a public address
// (no packet is sent). Empty when there is no route (an offline LAN: the adapter list still has it).
std::string DefaultRouteAddress() {
  std::string out;
#ifdef _WIN32
  SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == INVALID_SOCKET) return out;
#else
  int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s < 0) return out;
#endif
  sockaddr_in to{};
  to.sin_family = AF_INET;
  to.sin_port = htons(53);
  inet_pton(AF_INET, "8.8.8.8", &to.sin_addr);
  if (connect(s, reinterpret_cast<sockaddr*>(&to), static_cast<int>(sizeof to)) == 0) {
    sockaddr_in me{};
#ifdef _WIN32
    int len = static_cast<int>(sizeof me);
#else
    socklen_t len = static_cast<socklen_t>(sizeof me);
#endif
    if (getsockname(s, reinterpret_cast<sockaddr*>(&me), &len) == 0) {
      char buf[INET_ADDRSTRLEN] = {};
      if (inet_ntop(AF_INET, &me.sin_addr, buf, sizeof buf)) out = buf;
    }
  }
#ifdef _WIN32
  closesocket(s);
#else
  close(s);
#endif
  return out == "0.0.0.0" ? std::string() : out;
}

#ifdef _WIN32
std::string Utf8(const wchar_t* w) {
  if (!w || !*w) return std::string();
  const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
  if (n <= 1) return std::string();
  std::string out(static_cast<size_t>(n - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w, -1, &out[0], n, nullptr, nullptr);
  return out;
}
#endif

void AdapterAddresses(std::vector<LanAddress>& out) {
#ifdef _WIN32
  // (GetAdaptersAddresses: the buffer is asked for again if it was too small; 8-byte aligned storage.)
  ULONG size = 16 * 1024;
  std::vector<unsigned long long> buf;
  const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
  ULONG rc = ERROR_BUFFER_OVERFLOW;
  for (int tries = 0; tries < 3 && rc == ERROR_BUFFER_OVERFLOW; ++tries) {
    buf.assign((size + 7) / 8 + 1, 0);
    size = static_cast<ULONG>(buf.size() * 8);
    rc = GetAdaptersAddresses(AF_INET, flags, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()), &size);
  }
  if (rc != NO_ERROR) return;
  for (IP_ADAPTER_ADDRESSES* a = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()); a; a = a->Next) {
    if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
    const std::string name = Utf8(a->FriendlyName);
    const std::string desc = Utf8(a->Description);
    for (IP_ADAPTER_UNICAST_ADDRESS* u = a->FirstUnicastAddress; u; u = u->Next) {
      if (!u->Address.lpSockaddr || u->Address.lpSockaddr->sa_family != AF_INET) continue;
      const sockaddr_in* in = reinterpret_cast<const sockaddr_in*>(u->Address.lpSockaddr);
      char text[INET_ADDRSTRLEN] = {};
      if (!inet_ntop(AF_INET, &in->sin_addr, text, sizeof text)) continue;
      LanAddress l;
      l.address = text;
      l.adapter = name;
      l.virt = LooksVirtual(name) || LooksVirtual(desc);
      out.push_back(l);
    }
  }
#else
  ifaddrs* list = nullptr;
  if (getifaddrs(&list) != 0) return;
  for (ifaddrs* i = list; i; i = i->ifa_next) {
    if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET) continue;
    if (!(i->ifa_flags & IFF_UP) || (i->ifa_flags & IFF_LOOPBACK)) continue;
    char text[INET_ADDRSTRLEN] = {};
    if (!inet_ntop(AF_INET, &reinterpret_cast<const sockaddr_in*>(i->ifa_addr)->sin_addr, text, sizeof text)) continue;
    LanAddress l;
    l.address = text;
    l.adapter = i->ifa_name ? i->ifa_name : "";
    l.virt = LooksVirtual(l.adapter);
    out.push_back(l);
  }
  freeifaddrs(list);
#endif
}

}  // namespace

std::vector<LanAddress> lan_ipv4_addresses() {
#ifdef _WIN32
  WSADATA wsa;
  const bool wsaUp = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#endif
  std::vector<LanAddress> all;
  AdapterAddresses(all);
  const std::string primary = DefaultRouteAddress();
#ifdef _WIN32
  if (wsaUp) WSACleanup();
#endif
  if (!primary.empty() && std::none_of(all.begin(), all.end(), [&](const LanAddress& l) { return l.address == primary; })) {
    LanAddress l;
    l.address = primary;
    all.insert(all.begin(), l);
  }
  std::vector<LanAddress> out;
  for (LanAddress& l : all) {
    unsigned p[4];
    if (!ParseIpv4(l.address, p) || p[0] == 127 || p[0] == 0 || (p[0] == 169 && p[1] == 254)) continue;
    if (std::any_of(out.begin(), out.end(), [&](const LanAddress& o) { return o.address == l.address; })) continue;
    l.primary = l.address == primary;
    out.push_back(l);
  }
  auto rank = [](const LanAddress& l) {
    unsigned p[4];
    ParseIpv4(l.address, p);
    return (l.primary ? 0 : 4) + (l.virt ? 2 : 0) + (Private(p) ? 0 : 1);
  };
  std::stable_sort(out.begin(), out.end(), [&](const LanAddress& a, const LanAddress& b) { return rank(a) < rank(b); });
  if (out.size() > 4) out.resize(4);
  return out;
}

std::string check_address(const std::string& raw, std::string& host, int& port) {
  std::string t;
  for (char c : raw)
    if (c != ' ' && c != '\t') t += c;
  host.clear();
  if (t.empty()) return "Type the host's address first";
  std::string h = t;
  const size_t colon = t.rfind(':');
  if (colon != std::string::npos) {
    if (t.find(':') != colon) return "IPv6 addresses aren't supported: use the host's IPv4 address";
    h = t.substr(0, colon);
    const std::string p = t.substr(colon + 1);
    char* end = nullptr;
    const long v = std::strtol(p.c_str(), &end, 10);
    if (p.empty() || *end || v < 1 || v > 65535) return "The port after the colon must be a number from 1 to 65535";
    port = static_cast<int>(v);
  }
  if (h.empty()) return "Type the host's address before the colon";
  unsigned part[4];
  bool digitsAndDots = true;
  for (char c : h) digitsAndDots = digitsAndDots && (std::isdigit(static_cast<unsigned char>(c)) || c == '.');
  if (digitsAndDots) {
    if (!ParseIpv4(h, part)) return "An address has four numbers from 0 to 255, like 192.168.1.20";
    if (part[0] == 0) return "That address can't be reached; ask your friend for theirs";
  } else {
    // A host name (a LAN name, or a dynamic-DNS name): letters, digits, '-' and '.'.
    for (char c : h)
      if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '.')) return "An address uses only letters, digits, dots and dashes";
    if (h.front() == '.' || h.back() == '.' || h.find("..") != std::string::npos) return "That name has a misplaced dot";
  }
  host = h;
  return "";
}

}  // namespace launcher
