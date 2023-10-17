// This file should only only macros
#pragma once

// https://www.foonathan.net/2020/05/fold-tricks/
#define FOLD_ITER(expr) (expr, ...);
#define FOLD_ITER_BACKWARDS(expr) do { int dummy; (dummy = ... = ((expr), 0)); } while (0);

#define CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) CONCAT_IMPL(a, b)

#define UNIQUE_NAME(prefix) CONCAT(prefix, __COUNTER__)
#define DISCARD UNIQUE_NAME(_discard)
#define DEFER ScopeGuard UNIQUE_NAME(_scope_guard) = [&]()
