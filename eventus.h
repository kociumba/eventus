// eventus.h - v0.1.0 - kociumba 2026
//
// INFO:
//  To use eventus you don't need to define an implementation macro but there are configuration macros
//  which you can define before including eventus:
//      - #define EVENTUS_THREAD_SAFE     - makes eventus thread safe(duh) but somewhat hinders performance
//      - #define EVENTUS_BUS_METHODS     - includes methods on the main bus type, for oop like usage
//      - #define EVENTUS_NO_BUS_GC       - disables gc of empty events and types in the bus
//      - #define EVENTUS_NO_THREADING    - excludes threaded publish methods
//      - #define EVENTUS_SHORT_NAMESPACE - shortens the eventus:: namespace to ev::
//      - #define EVENTUS_DEBUG_LOG       - enables debug logging for the bus
//
// NOTE: when EVENTUS_NO_THREADING is not defined, EVENTUS_THREAD_SAFE is automatically defined due
//  to requiering thread safety in the threading code
//
//  The library is mostly designed to perform all the performance intensive operations like sorting
//  subscribers or garbage collection on subscription, this frees up publishing to be as performant
//  as possible
//
//  If your standard library and compiler supports std::jthread eventus will also include threaded
//  publishing functions, these use a thread pool that is automatically scaled to the undelaying
//  hardware, the functions postfixed with _threaded simply execute publishiing on a remote thread,
//  still executing subscribers concurently on it, while the functions with _async, run each subscriber
//  in a separate thread from the pool
//
//  Basic usage example:
//      struct my_event { int value; };
//
//      eventus::bus b;
//      eventus::subscribe<my_event>(&b, [](my_event* e) {
//          std::printf("Event received: %d\n", e->value);
//          return true; // continue propagation
//      });
//
//      eventus::publish(&b, my_event{42});
//
// LICENSE
//
//   See end of file for license information.

#ifndef EVENTUS_H
#define EVENTUS_H

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// =-               I N C L U D E S               -=
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include <version>

#include <algorithm>
#include <atomic>
#include <concepts>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// =-   M A C R O   C H E C K S / D E F I N E S   -=
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#if defined(EVENTUS_DEBUG_LOG)
#include <chrono>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#define ev_log(bus_ptr, ...) \
    if ((bus_ptr)->_log) { (bus_ptr)->_log(ev_log_data(__VA_ARGS__)); }

#ifdef __GNUC__
#include <cxxabi.h>
#endif

#else
#define ev_log(log_func, ...)
#endif

#define eventus_function std::move_only_function
#if !defined(__cpp_lib_move_only_function)
#warning \
    "c++23 std::move_only_function was not found, eventus will use std::function instead this may impact performance and behaviour"
#undef eventus_function
#define eventus_function std::function
#endif

#if !defined(__cpp_lib_concepts) || __cpp_lib_concepts < 202002LL
#error "eventus requires a full c++20 concepts implementation"
#endif

#if defined(__clang__)
#if !__has_feature(cxx_rtti)
#define EVENTUS_NO_RTTI
#endif
#elif defined(__GNUC__)
#if !defined(__GXX_RTTI)
#define EVENTUS_NO_RTTI
#endif
#elif defined(_MSC_VER)
#if !defined(_CPPRTTI)
#define EVENTUS_NO_RTTI
#endif
#endif

#if defined(EVENTUS_NO_RTTI)
#error "eventus requires RTTI, which is not enabled in this compilation"
#endif

#if defined(__cpp_lib_jthread) && !defined(EVENTUS_NO_THREADING)
#include <condition_variable>
#include <queue>
#include <thread>
#define EVENTUS_HAS_JTHREAD
#define EVENTUS_THREAD_SAFE  // we need mutex and thread safety if are doing threaded
#else
#warning \
    "std::jthread is not availible on your compiler or STL, threading related functionality will not be enabled"
#endif

// keeping this empty allows us to not stub the mutex with void* when thread safety is off
#define mutex_scope(mutex)

#if defined(EVENTUS_THREAD_SAFE)
#include <mutex>
#undef mutex_scope
#define mutex_scope(mutex) std::lock_guard lock##__LINE__(mutex)
#endif

#if defined(EVENTUS_SHORT_NAMESPACE)
namespace ev {
#else
namespace eventus {
#endif

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// =-          S U B S C I B E R   A P I          -=
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

enum ev_status {
    OK,
    EVENT_TYPE_NOT_REGISTERED,
    NO_SUBSCRIBERS_FOR_EVENT_TYPE,
    NO_SUBSCRIBER_WITH_ID,
};

// use to get a string representation of an eventus state value
inline const char* status_string(ev_status s) {
    switch (s) {
        case OK:
            return "OK";
        case EVENT_TYPE_NOT_REGISTERED:
            return "EVENT_TYPE_NOT_REGISTERED";
        case NO_SUBSCRIBERS_FOR_EVENT_TYPE:
            return "NO_SUBSCRIBERS_FOR_EVENT_TYPE";
        case NO_SUBSCRIBER_WITH_ID:
            return "NO_SUBSCRIBER_WITH_ID";
        default:
            return "invalid eventus::ev_status value";
    }
}

#if defined(EVENTUS_DEBUG_LOG)
enum ev_log_level {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL,
};

struct ev_log_data {
    ev_log_level level;
    std::string msg;
    bool has_event_type_info = false;
    std::type_index event_type = typeid(void);
    bool has_sub_id = false;
    int64_t id = -1;

    ev_log_data(ev_log_level l,
                std::string msg,
                std::type_index event_t = typeid(void),
                int64_t id = -1)
        : level(l), msg(std::move(msg)), event_type(event_t), id(id) {
        has_event_type_info = event_type != typeid(void);
        has_sub_id = id != -1;
    }

    std::string get_event_type_name() const {
        if (!has_event_type_info) { return "N/A"; }

        const char* mangled_name = event_type.name();

#ifdef __GNUC__
        int status = 0;
        std::unique_ptr<char, void (*)(void*)> res{
            abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status), std::free};
        return (status == 0) ? std::string(res.get()) : std::string(mangled_name);
#else
        return std::string(mangled_name);
#endif
    }

    // prforms {...} formatting on the log data, only supports specific named placeholders
    std::string format() const {
        std::string result = msg;

        if (has_event_type_info) {
            std::string event_t_name = get_event_type_name();
            size_t pos = 0;
            while ((pos = result.find("{event}", pos)) != std::string::npos) {
                result.replace(pos, 7, event_t_name);
                pos += event_t_name.length();
            }
        }

        if (has_sub_id) {
            std::string id_str = std::to_string(id);
            size_t pos = 0;
            while ((pos = result.find("{id}", pos)) != std::string::npos) {
                result.replace(pos, 4, id_str);
                pos += id_str.length();
            }
        }

        return result;
    }
};
#endif

using invoke_fn = bool (*)(void* callback, void* event);

template <typename EventT>
bool invoke_typed(void* callback, void* event) {
    auto* cb = static_cast<eventus_function<bool(EventT*)>*>(callback);
    return (*cb)(static_cast<EventT*>(event));
}

struct subscriber {
    void* callback_storage;
    invoke_fn invoker;
    void (*deleter)(void*);
    int64_t id;
    int32_t priority = 0;

    subscriber() = default;

    // results in template errors in subscribe calls, replaced by the make factory function
    template <typename EventT, typename F>
        requires std::invocable<F, EventT*>
    [[deprecated]] subscriber(F&& f, int64_t id, int32_t priority = 0)
        : id(id), priority(priority) {
        using CallbackType = eventus_function<bool(EventT*)>;
        callback_storage = new CallbackType(std::forward<F>(f));

        invoker = &invoke_typed<EventT>;
        deleter = [](void* ptr) { delete static_cast<CallbackType*>(ptr); };
    }

    template <typename EventT, typename F>
        requires std::invocable<F, EventT*>
    static subscriber make(F&& f, int64_t id, int32_t priority = 0) {
        subscriber s;
        s.id = id;
        s.priority = priority;

        using CallbackType = eventus_function<bool(EventT*)>;
        s.callback_storage = new CallbackType(std::forward<F>(f));
        s.invoker = &invoke_typed<EventT>;
        s.deleter = [](void* ptr) { delete static_cast<CallbackType*>(ptr); };

        return s;
    }

    ~subscriber() {
        if (callback_storage && deleter) { deleter(callback_storage); }
    }

    subscriber(subscriber&& other) noexcept
        : callback_storage(other.callback_storage),
          invoker(other.invoker),
          deleter(other.deleter),
          id(other.id),
          priority(other.priority) {
        other.callback_storage = nullptr;
        other.deleter = nullptr;
    }

    subscriber& operator=(subscriber&& other) noexcept {
        if (this != &other) {
            if (callback_storage && deleter) { deleter(callback_storage); }
            callback_storage = other.callback_storage;
            invoker = other.invoker;
            deleter = other.deleter;
            id = other.id;
            priority = other.priority;
            other.callback_storage = nullptr;
            other.deleter = nullptr;
        }
        return *this;
    }

    subscriber(const subscriber&) = delete;
    subscriber& operator=(const subscriber&) = delete;

    bool invoke(void* event) const { return invoker(callback_storage, event); }
};

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// =-        A P I   D E F I N I T I O N S        -=
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

struct bus;

template <typename EventT, typename F>
    requires std::invocable<F, EventT*>
int64_t subscribe(bus* b, F&& func, int32_t priority = 0);

template <typename... EventTs, typename F>
std::vector<int64_t> subscribe_multi(bus* b, F&& func, int32_t priority = 0);

template <typename EventT>
ev_status unsubscribe(bus* b, int64_t id);

inline ev_status unsubscribe(bus* b, int64_t id);

template <typename EventT>
ev_status unsubscribe_event(bus* b);

inline ev_status unsubscribe_all(bus* b);

template <typename EventT>
ev_status publish(bus* b, EventT data);

template <typename... EventTs>
ev_status publish_multi(bus* b, EventTs... data);

#if defined(EVENTUS_HAS_JTHREAD)
template <typename EventT>
ev_status publish_threaded(bus* b, EventT data);

template <typename... EventTs>
ev_status publish_threaded_multi(bus* b, EventTs... data);

template <typename EventT>
ev_status publish_async(bus* b, EventT data);

template <typename... EventT>
ev_status publish_async_multi(bus* b, EventT... data);
#endif

#if defined(EVENTUS_DEBUG_LOG)
using log_func = eventus_function<void(ev_log_data)>;

inline void ev_default_log_func(ev_log_data data) {
    std::string buf;
    bool abort = false;
    buf.reserve(256);

    buf += "[";
    buf += std::format(
        "{:%Y-%m-%d %H:%M:%S}",
        std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()));
    buf += "] ";

    switch (data.level) {
        case DEBUG:
            buf += "[DEBU] : ";
            break;
        case INFO:
            buf += "[INFO] : ";
            break;
        case WARNING:
            buf += "[WARN] : ";
            break;
        case ERROR:
            buf += "[ERRO] : ";
            break;
        case FATAL:
            buf += "[FATA] : ";
            abort = true;
            break;
    }

    buf += data.format();

    std::cout << buf << std::endl;

    if (abort) std::abort();
}

void set_logger(bus* b, log_func func = ev_default_log_func);
#endif

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// =-              A P I   I M P L S              -=
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#if defined(EVENTUS_HAS_JTHREAD)
namespace detail {
struct thread_pool {
    std::vector<std::jthread> workers;
    std::queue<eventus_function<void()>> tasks;
    std::mutex queue_mu;
    std::condition_variable cv;
    std::atomic<bool> stop{false};

    thread_pool(size_t threads = std::max(1u, std::thread::hardware_concurrency())) {
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this](std::stop_token st) {
                while (!st.stop_requested() && !stop) {
                    eventus_function<void()> task;
                    {
                        std::unique_lock lock(queue_mu);
                        cv.wait(lock, [this, &st] {
                            return stop || !tasks.empty() || st.stop_requested();
                        });
                        if ((stop || st.stop_requested()) && tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    ~thread_pool() {
        stop = true;
        cv.notify_all();
    }

    void enqueue(eventus_function<void()> task) {
        {
            mutex_scope(queue_mu);
            tasks.push(std::move(task));
        }
        cv.notify_one();
    }
};
}  // namespace detail
#endif

struct bus {
    std::unordered_map<std::type_index, std::vector<subscriber>> subs;
    std::atomic<int64_t> id_counter;

    bus() = default;

#if defined(EVENTUS_THREAD_SAFE)
    std::recursive_mutex mu;
#endif

#if defined(EVENTUS_HAS_JTHREAD)
    detail::thread_pool pool;

    explicit bus(size_t threads) : pool(threads) {};
#endif

#if defined(EVENTUS_DEBUG_LOG)
    log_func _log = ev_default_log_func;
#endif

#if defined(EVENTUS_BUS_METHODS)
    template <typename EventT, typename F>
        requires std::invocable<F, EventT*>
    int64_t subscribe(F&& func, int32_t priority = 0) {
        return eventus::subscribe<EventT, F>(this, std::forward<F>(func), priority);
    }

    template <typename... EventTs, typename F>
    std::vector<int64_t> subscribe_multi(F&& func, int32_t priority = 0) {
        return eventus::subscribe_multi<EventTs...>(this, std::forward<F>(func), priority);
    }

    template <typename EventT>
    e_status unsubscribe(int64_t id) {
        return eventus::unsubscribe<EventT>(this, id);
    }

    template <typename EventT>
    e_status unsubscribe_event() {
        return eventus::unsubscribe_event<EventT>(this);
    }

    e_status unsubscribe_all() { return eventus::unsubscribe_all(this); }

    template <typename EventT>
    e_status publish(EventT data) {
        return eventus::publish<EventT>(this, std::move(data));
    }

    template <typename... EventTs>
    e_status publish_multi(EventTs... data) {
        return eventus::publish_multi(this, std::move(data)...);
    }

#if defined(EVENTUS_HAS_JTHREAD)
    template <typename EventT>
    e_status publish_threaded(EventT data) {
        return eventus::publish_threaded<EventT>(this, std::move(data));
    }

    template <typename... EventTs>
    e_status publish_threaded_multi(EventTs... data) {
        return eventus::publish_threaded_multi(this, std::move(data)...);
    }

    template <typename EventT>
    e_status publish_async(EventT data) {
        return eventus::publish_async(this, std::move(data));
    }

    template <typename... EventT>
    e_status publish_async_multi(bus* b, EventT... data) {
        return eventus::publish_async_multi(this, std::move(data)...);
    }
#endif

#if defined(EVENTUS_DEBUG_LOG)
    void set_logger(log_func func = ev_default_log_func) {
        eventus::set_logger(this, std::move(func));
    }
#endif
#endif
};

// holds implementation details but is not hidden, you can freely use these
namespace detail {

// performs garbage collection on the bus
inline void gc(bus* b) {
#if !defined(EVENTUS_NO_BUS_GC)
    mutex_scope(b->mu);

    for (auto it = b->subs.begin(); it != b->subs.end();) {
        if (it->second.empty()) {
            ev_log(b, DEBUG, "removed empty event: {event} from bus", it->first);
            it = b->subs.erase(it);
        } else {
            ++it;
        }
    }
#endif
}

}  // namespace detail

// subscribe to an event EventT, priority determines subscriber execution order on publish
template <typename EventT, typename F>
    requires std::invocable<F, EventT*>
int64_t subscribe(bus* b, F&& func, int32_t priority) {
    mutex_scope(b->mu);

    int64_t id = b->id_counter.fetch_add(1);
    auto& vec = b->subs[typeid(EventT)];

    vec.push_back(subscriber::make<EventT>(std::forward<F>(func), id, priority));

    std::sort(vec.begin(), vec.end(), [](const subscriber& a, const subscriber& b) {
        return a.priority > b.priority;
    });

    detail::gc(b);
    ev_log(b, INFO, "Successfully subscribed to {event} with id: {id}", typeid(EventT), id);

    return id;
}

// subscribe to an event EventT..., priority determines subscriber execution order on publish
template <typename... EventTs, typename F>
std::vector<int64_t> subscribe_multi(bus* b, F&& func, int32_t priority) {
    std::vector<int64_t> ids;
    ids.reserve(sizeof...(EventTs));

    (ids.push_back(subscribe<EventTs>(b, func, priority)), ...);

    return ids;
}

// unsubscribes a subscriber using the provided id from the specified event
template <typename EventT>
ev_status unsubscribe(bus* b, int64_t id) {
    mutex_scope(b->mu);

    auto it = b->subs.find(typeid(EventT));
    if (it == b->subs.end()) {
        ev_log(b,
               ERROR,
               "Event: {event} is not registered in the bus, can not unsubscribe id: {id} from "
               "nonexistant event",
               typeid(EventT),
               id);

        return EVENT_TYPE_NOT_REGISTERED;
    }

    auto& vec = it->second;
    auto sub_it =
        std::find_if(vec.begin(), vec.end(), [id](const subscriber& s) { return s.id == id; });

    if (sub_it != vec.end()) {
        vec.erase(sub_it);

        detail::gc(b);
        ev_log(b, INFO, "Successfully unsubscribed from {event} with id: {id}", typeid(EventT), id);

        return OK;
    }

    ev_log(b, WARNING, "No subscriber with id: {id} registered to {event}", typeid(EventT), id);

    return NO_SUBSCRIBER_WITH_ID;
}

// unsubscribes based only on an id, not to be used in performance critical applications
inline ev_status unsubscribe(bus* b, int64_t id) {
    mutex_scope(b->mu);

    for (auto it = b->subs.begin(); it != b->subs.end(); ++it) {
        auto& vec = it->second;

        auto new_end = std::remove_if(
            vec.begin(), vec.end(), [id](const subscriber& s) { return s.id == id; });

        if (new_end != vec.end()) {
            vec.erase(new_end, vec.end());

            detail::gc(b);
            ev_log(b, INFO, "Successfully unsubscribed from {event} with id: {id}", it->first, id);

            return OK;
        }
    }

    ev_log(b, WARNING, "No subscriber with id: {id} in the bus", typeid(void), id);

    return NO_SUBSCRIBER_WITH_ID;
}

// unsubscribes all subscribers from the specified event
template <typename EventT>
ev_status unsubscribe_event(bus* b) {
    mutex_scope(b->mu);

    auto it = b->subs.find(typeid(EventT));
    if (it == b->subs.end()) {
        ev_log(b, ERROR, "Event: {event} is not registered in the bus", typeid(EventT));

        return EVENT_TYPE_NOT_REGISTERED;
    }

    b->subs.erase(it);

    detail::gc(b);
    ev_log(
        b, INFO, "Successfully unsubscribed all subscribers from event: {event}", typeid(EventT));

    return OK;
}

// clears the whole bus unsubscribing all subscribers
inline ev_status unsubscribe_all(bus* b) {
    mutex_scope(b->mu);
    b->subs.clear();
    ev_log(b, DEBUG, "Successfully cleared the bus");
    return OK;
}

// publishes an event of type EventT, executing subscribers on the current thread
template <typename EventT>
ev_status publish(bus* b, EventT data) {
    mutex_scope(b->mu);

    auto it = b->subs.find(typeid(EventT));
    if (it == b->subs.end()) {
        ev_log(b, ERROR, "Event: {event} is not registered in the bus", typeid(EventT));

        return EVENT_TYPE_NOT_REGISTERED;
    }

    auto& vec = it->second;
    if (vec.empty()) {
        ev_log(b, WARNING, "No subscribers registered to {event}", typeid(EventT));

        return NO_SUBSCRIBERS_FOR_EVENT_TYPE;
    }

    for (auto& sub : vec) {
        if (!sub.invoke(&data)) { break; }
    }

    ev_log(b, INFO, "Successfully published event: {event}", typeid(EventT));

    return OK;
}

// publishes an events of types EventT..., executing subscribers on the current thread
template <typename... EventTs>
ev_status publish_multi(bus* b, EventTs... data) {
    ev_status last_status = OK;
    ((last_status = publish(b, data)), ...);
    return last_status;
}

#if defined(EVENTUS_HAS_JTHREAD)
// publishes an event of type EventT, executing subscribers on a remote thread
template <typename EventT>
ev_status publish_threaded(bus* b, EventT data) {
    b->pool.enqueue([b, data = std::move(data)]() mutable { publish(b, std::move(data)); });
    return OK;
}

// publishes events of types EventT..., executing subscribers on a remote thread
template <typename... EventTs>
ev_status publish_threaded_multi(bus* b, EventTs... data) {
    (publish_threaded(b, std::move(data)), ...);
    return OK;
}

// publishes an event of type EventT, executing each subscriber on a remote thread
// this disregards subscriber priority and loses benefits the less work subscribers do,
// this also does not stop event propagation on subscriber error
template <typename EventT>
ev_status publish_async(bus* b, EventT data) {
    mutex_scope(b->mu);

    auto it = b->subs.find(typeid(EventT));
    if (it == b->subs.end()) {
        ev_log(b, ERROR, "Event: {event} is not registered in the bus", typeid(EventT));

        return EVENT_TYPE_NOT_REGISTERED;
    }

    auto& vec = it->second;
    if (vec.empty()) {
        ev_log(b, WARNING, "No subscribers registered to {event}", typeid(EventT));

        return NO_SUBSCRIBERS_FOR_EVENT_TYPE;
    }

    // TODO: see if we may want to copy subs here
    if (vec.size() == 1) {
        b->pool.enqueue([&sub = vec[0], data = std::move(data)]() mutable { sub.invoke(&data); });
    } else {
        auto shared_data = std::make_shared<EventT>(std::move(data));
        for (auto& sub : vec) {
            b->pool.enqueue([&sub, shared_data]() { sub.invoke(shared_data.get()); });
            // NOTE: 'break on false' logic is no longer possible here
        }
    }

    ev_log(b, INFO, "Successfully published event: {event}", typeid(EventT));

    return OK;
}

// publishes events of types EventT..., executing each subscriber on a remote thread
// this disregards subscriber priority and loses benefits the less work subscribers do,
// this also does not stop event propagation on subscriber error
template <typename... EventT>
ev_status publish_async_multi(bus* b, EventT... data) {
    (publish_async(b, std::move(data)), ...);
    return OK;
}

#endif

#if defined(EVENTUS_DEBUG_LOG)
void set_logger(bus* b, log_func func) {
    mutex_scope(b->mu);
    b->_log = std::move(func);
}
#endif

}  // namespace eventus

#endif /* EVENTUS_H */

/*
------------------------------------------------------------------------------
This software is available under the MIT license.
------------------------------------------------------------------------------
MIT License

Copyright (c) 2026 kociumba

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
