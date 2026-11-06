// @class EEvent after ESemaphore
// A semaphore used as a one-shot signal (all of its methods were inline). The engine's frame
// handshake is one: the renderer releases it when a frame has been handed to the display.
#pragma once

struct EEvent : public ESemaphore {};
