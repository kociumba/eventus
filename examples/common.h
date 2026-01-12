#ifndef COMMON_H
#define COMMON_H

#include <chrono>
#include <thread>

void ev_sleep(int64_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

#endif /* COMMON_H */
