// This file should only only macros
#pragma once

// https://www.foonathan.net/2020/05/fold-tricks/
#define FOLD_ITER(expr) (expr, ...);
#define FOLD_ITER_BACKWARDS(expr) do { int dummy; (dummy = ... = ((expr), 0)); } while (0);
