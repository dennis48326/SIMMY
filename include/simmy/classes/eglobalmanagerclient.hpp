// @class EGlobalManagerClient
// Base of every engine singleton that EGlobalManager starts up and shuts down in priority order.
#pragma once

struct EGlobalManagerClient {
  EGlobalManagerClient() = default;
  virtual ~EGlobalManagerClient() = default;
  virtual int ManagedStartup() { return 1; }
  virtual void ManagedShutdown() {}
};
