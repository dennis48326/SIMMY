// Port of base/e_globalmanager.cpp: starts engine singletons in priority order, stops them in reverse.
#include "simmy/types_gen.hpp"

EGlobalManager::Entry EGlobalManager::m_clients[EGlobalManager::kMaxClients];
int EGlobalManager::m_nClients = 0;
int EGlobalManager::m_nStartedUpClients = 0;
int EGlobalManager::m_startupComplete = 0;
int EGlobalManager::m_shutdownComplete = 0;

// 802C86D0
void EGlobalManager::Register(EGlobalManagerClient* client, int priority) {
  if (m_nClients >= kMaxClients) {
    SIMMY_STUB("EGlobalManager::Register: too many clients", 0x802C86D0);
    return;
  }
  m_clients[m_nClients].client = client;
  m_clients[m_nClients].priority = priority;
  ++m_nClients;
}

// 802C86FC
int EGlobalManager::Startup() {
  if (m_startupComplete) return 1;
  m_startupComplete = 1;
  m_shutdownComplete = 0;
  // Selection sort by priority (lowest first), as in the original.
  for (int i = 0; i < m_nClients; ++i)
    for (int j = i + 1; j < m_nClients; ++j)
      if (m_clients[j].priority < m_clients[i].priority) {
        Entry tmp = m_clients[i];
        m_clients[i] = m_clients[j];
        m_clients[j] = tmp;
      }
  for (; m_nStartedUpClients < m_nClients; ++m_nStartedUpClients) {
    if (!m_clients[m_nStartedUpClients].client->ManagedStartup()) {
      m_startupComplete = 0;
      return 0;
    }
  }
  return 1;
}

// 802C8838
void EGlobalManager::Shutdown() {
  if (m_shutdownComplete) return;
  m_shutdownComplete = 1;
  m_startupComplete = 0;
  for (; m_nStartedUpClients > 0; --m_nStartedUpClients)
    m_clients[m_nStartedUpClients - 1].client->ManagedShutdown();
}
