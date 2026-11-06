// @class EGlobalManager after EGlobalManagerClient
#pragma once

struct EGlobalManager {
  static constexpr int kMaxClients = 64;
  struct Entry {
    EGlobalManagerClient* client;
    int priority;
  };
  static void Register(EGlobalManagerClient* client, int priority);
  static int Startup();
  static void Shutdown();

  static Entry m_clients[kMaxClients];
  static int m_nClients;
  static int m_nStartedUpClients;
  static int m_startupComplete;
  static int m_shutdownComplete;
};
