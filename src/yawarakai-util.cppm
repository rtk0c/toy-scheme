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

}
