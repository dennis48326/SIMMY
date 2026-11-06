// Startup probe: links Dawn (webgpu_dawn.dll) only.
#include <webgpu/webgpu.h>
#include <cstdio>
int main() {
  std::printf("probe_dawn start\n");
  WGPUInstance instance = wgpuCreateInstance(nullptr);
  std::printf("probe_dawn ok, instance %p\n", static_cast<void*>(instance));
  return 0;
}
