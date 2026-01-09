// Eventus version: 0.0.1
//
// To use eventus you don't need to define an implementation macro but there are configuration macros
// which you can define before including eventus:
//  - #define EVENTUS_THREAD_SAFE     - makes eventus thread safe(duh) but somewhat hinders performance
//  - #define EVENTUS_BUS_METHODS     - includes methods on the main bus type, for oop like usage
//  - #define EVENTUS_NO_BUS_GC       - disables gc of empty events and types in the bus
//  - #define EVENTUS_NO_THREADING    - excludes threaded publish methods
//  - #define EVENTUS_SHORT_NAMESPACE - shortens the eventus:: namespace to ev::
//
// The library is mostly designed to perform all the performance intensive operations like sorting
// subscribers or garbage collection on subscription, this frees up publishing to be as performant
// as possible
//
// If your standard library and compiler supports std::jthread eventus will also include basic
// threaded publishing functions, these are very naive implementations spawning new threads for each
// subscriber launched, you can disable these (see above, about config macros), if you choose to use
// these you should link with your systems threads library
//
// Basic usage example:
//  struct my_event { int value; };
//
//  eventus::bus b;
//  eventus::subscribe<my_event>(&b, [](my_event* e) {
//      std::printf("Event received: %d\n", e->value);
//      return true; // continue propagation
//  });
//
//  eventus::publish(&b, my_event{42});

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
#include <typeindex>
#include <unordered_map>
#include <vector>

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// =-   M A C R O   C H E C K S / D E F I N E S   -=
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// keeping this empty allows us to not stub the mutex with void* when thread safety is off
#define mutex_scope(mutex)

#if defined(EVENTUS_THREAD_SAFE)
#include <mutex>
#undef mutex_scope
#define mutex_scope(mutex) std::lock_guard lock##__LINE__(mutex)
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
#include <thread>
#define EVENTUS_HAS_JTHREAD
#endif

#if defined(EVENTUS_SHORT_NAMESPACE)
namespace ev {
#else
namespace eventus {
#endif

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// =-          S U B S C I B E R   A P I          -=
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

enum e_status {
    OK,
    EVENT_TYPE_NOT_REGISTERED,
    NO_SUBSCRIBERS_FOR_EVENT_TYPE,
    NO_SUBSCRIBER_WITH_ID,
};

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
        if (callback_storage && deleter) {
            deleter(callback_storage);
        }
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
            if (callback_storage && deleter) {
                deleter(callback_storage);
            }
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
e_status unsubscribe(bus* b, int64_t id);

inline e_status unsubscribe(bus* b, int64_t id);

template <typename EventT>
e_status unsubscribe_event(bus* b);

inline e_status unsubscribe_all(bus* b);

template <typename EventT>
e_status publish(bus* b, EventT data);

template <typename... EventTs>
e_status publish_multi(bus* b, EventTs... data);

#if defined(EVENTUS_HAS_JTHREAD)
template <typename EventT>
e_status publish_threaded(bus* b, EventT data);

template <typename... EventTs>
e_status publish_threaded_multi(bus* b, EventTs... data);
#endif

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// =-              A P I   I M P L S              -=
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

struct bus {
    std::unordered_map<std::type_index, std::vector<subscriber>> subs;
    std::atomic<int64_t> id_counter;

#if defined(EVENTUS_THREAD_SAFE)
    std::recursive_mutex mu;
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
#endif
#endif
};

// holds implementation details but is not hidden, you can freely use these
namespace detail {

// performs garbage collection on the bus,
inline void gc(bus* b) {
#if !defined(EVENTUS_NO_BUS_GC)
    mutex_scope(b->mu);

    for (auto it = b->subs.begin(); it != b->subs.end();) {
        if (it->second.empty()) {
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
e_status unsubscribe(bus* b, int64_t id) {
    mutex_scope(b->mu);

    auto it = b->subs.find(typeid(EventT));
    if (it == b->subs.end()) {
        return EVENT_TYPE_NOT_REGISTERED;
    }

    auto& vec = it->second;
    auto sub_it =
        std::find_if(vec.begin(), vec.end(), [id](const subscriber& s) { return s.id == id; });

    if (sub_it != vec.end()) {
        vec.erase(sub_it);
        return OK;
    }

    return NO_SUBSCRIBERS_FOR_EVENT_TYPE;
}

// unsubscribes based only on an id, not to be used in performance critical applications
inline e_status unsubscribe(bus* b, int64_t id) {
    mutex_scope(b->mu);

    for (auto it = b->subs.begin(); it != b->subs.end();) {
        auto& vec = it->second;

        auto new_end = std::remove_if(
            vec.begin(), vec.end(), [id](const subscriber& s) { return s.id == id; });

        if (new_end != vec.end()) {
            vec.erase(new_end, vec.end());
            return OK;
        }
    }

    return NO_SUBSCRIBER_WITH_ID;
}

// unsubscribes all subscribers from the specified event
template <typename EventT>
e_status unsubscribe_event(bus* b) {
    mutex_scope(b->mu);

    auto it = b->subs.find(typeid(EventT));
    if (it == b->subs.end()) {
        return EVENT_TYPE_NOT_REGISTERED;
    }

    b->subs.erase(it);
    return OK;
}

// clears the whole bus unsubscribing all subscribers
inline e_status unsubscribe_all(bus* b) {
    mutex_scope(b->mu);
    b->subs.clear();
    return OK;
}

// publishes an event of type EventT, executing subscribers on the current thread
template <typename EventT>
e_status publish(bus* b, EventT data) {
    mutex_scope(b->mu);

    auto it = b->subs.find(typeid(EventT));
    if (it == b->subs.end()) {
        return EVENT_TYPE_NOT_REGISTERED;
    }

    auto& vec = it->second;
    if (vec.empty()) {
        return NO_SUBSCRIBERS_FOR_EVENT_TYPE;
    }

    for (auto& sub : vec) {
        if (!sub.invoke(&data)) {
            break;
        }
    }

    return OK;
}

// publishes an events of types EventT..., executing subscribers on the current thread
template <typename... EventTs>
e_status publish_multi(bus* b, EventTs... data) {
    e_status last_status = OK;
    ((last_status = publish(b, data)), ...);
    return last_status;
}

#if defined(EVENTUS_HAS_JTHREAD)
// publishes an event of type EventT, executing subscribers on a remote threads
template <typename EventT>
e_status publish_threaded(bus* b, EventT data) {
    std::jthread([b, data = std::move(data)]() mutable { publish(b, std::move(data)); }).detach();
    return OK;
}

// publishes an events of types EventT..., executing subscribers on a remote threads
template <typename... EventTs>
e_status publish_threaded_multi(bus* b, EventTs... data) {
    (publish_threaded(b, std::move(data)), ...);
    return OK;
}
#endif

}  // namespace eventus

#endif /* EVENTUS_H */
