export module yawarakai:util;

import std;

export namespace yawarakai {

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
