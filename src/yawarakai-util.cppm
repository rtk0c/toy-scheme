module;
#include <cassert>

export module yawarakai:util;

import std;

export namespace yawarakai {

// https://stackoverflow.com/a/52393977

template <typename T, typename... Args>
struct Concatenator;

template <typename... Args0, typename... Args1>
struct Concatenator<std::variant<Args0...>, Args1...> {
    using Type = std::variant<Args0..., Args1...>;
};

template <typename TVariant, typename... Ts>
using ExtendVariant = Concatenator<TVariant, Ts...>::Type;

template <typename TFunction>
struct ScopeGuard {
    TFunction _func;
    bool _canceled = false;

    ScopeGuard(TFunction&& f) : _func{ std::forward<TFunction>(f) } {}
    ~ScopeGuard() {
        if (!_canceled) {
            _func();
        }
    }

    void cancel() { _canceled = true; }
};

template <typename T>
struct CurrentValueRestorer {
    T* _slot;
    T _prev;

    CurrentValueRestorer(T& slot) : _slot{ &slot }, _prev{ slot } {}
    void operator()() { *_slot = std::move(_prev); }
};

// A general sentinel type
struct CommonSentinel {};

template <typename TPayload, typename TIter>
struct Iterable {
    using Iterator = TIter;
    using Sentinel = TIter::Sentinel;

    TPayload payload;

    Iterator begin() const { return Iterator(std::move(payload)); }
    Sentinel end() const { return Sentinel(); }
};

template <std::integral T>
bool is_power_of_two(T v) {
    return v != 0 && (v & (v - 1)) == 0;
}

uintptr_t shift_down_and_align(uintptr_t start, size_t size, size_t alignment) {
    assert(is_power_of_two(alignment));

    auto res_unaligned = start - size;
    return res_unaligned & ~(alignment - 1);
}

}
