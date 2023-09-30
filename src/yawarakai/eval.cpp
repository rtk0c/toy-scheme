module yawarakai;

import std;

#include "util.hpp"

using namespace std::literals;

namespace yawarakai {

Sexp builtin_add(const Sexp& params, Environment& env) {
    double res = 0.0;
    for (const Sexp& param : iterate(params, env)) {
        res += eval(param, env).as_or_error<double>("Error: + cannot accept non-numerical parameters"sv); 
    }

    return Sexp(res);
}

Sexp builtin_sub(const Sexp& params, Environment& env) {
    auto& first_cons = env.lookup(params.as<MemoryLocation>());

    double res = first_cons.car.as<double>();
    for (const Sexp& param : iterate(first_cons.cdr, env)) {
        res -= eval(param, env).as_or_error<double>("Error: - cannot accept non-numerical parameters"sv); 
    }

    return Sexp(res);
}

Sexp builtin_mul(const Sexp& params, Environment& env) {
    double res = 1.0;
    for (const Sexp& param : iterate(params, env)) {
        res *= eval(param, env).as_or_error<double>("Error: * cannot accept non-numerical parameters"sv); 
    }

    return Sexp(res);
}

Sexp builtin_div(const Sexp& params, Environment& env) {
    auto& first_cons = env.lookup(params.as<MemoryLocation>());

    double res = first_cons.car.as<double>();
    for (const Sexp& param : iterate(first_cons.cdr, env)) {
        res /= eval(param, env).as_or_error<double>("Error: / cannot accept non-numerical parameters"sv); 
    }

    return Sexp(res);
}

Sexp builtin_if(const Sexp& params, Environment& env) {
    const Sexp* cond;
    const Sexp* true_case;
    const Sexp* false_case;
    list_get_prefix(params, {&cond, &true_case, &false_case}, env);

    Sexp cond_val = eval(*cond, env);
    if (cond_val.is<bool>() && cond_val.as<bool>()) {
        return eval(*true_case, env);
    } else {
        return eval(*false_case, env);
    }
}

Sexp builtin_eq(const Sexp& params, Environment& env) {
    const Sexp* a;
    const Sexp* b;
    list_get_prefix(params, {&a, &b}, env);

    return {}; // TODO
}

template <typename Op>
Sexp builtin_binary_op(const Sexp& params, Environment& env) {
    using enum Sexp::Type;

    const Sexp* a_lit;
    const Sexp* b_lit;
    list_get_prefix(params, {&a_lit, &b_lit}, env);

    auto a = eval(*a_lit, env);
    auto b = eval(*b_lit, env);

    if (a.get_type() == TYPE_NUM && b.get_type() == TYPE_NUM) {
        auto res = Op{}(
            a.as<double>(),
            b.as<double>());
        return Sexp(res);
    } else {
        throw "Error: invalid types";
    }
}

Sexp builtin_car(const Sexp& params, Environment& env) { return car(eval(list_nth_elm(params, 0, env), env), env); }
Sexp builtin_cdr(const Sexp& params, Environment& env) { return cdr(eval(list_nth_elm(params, 0, env), env), env); }
Sexp builtin_cons(const Sexp& params, Environment& env) {
    const Sexp* a;
    const Sexp* b;
    list_get_prefix(params, {&a, &b}, env);
    return cons(
        eval(*a, env),
        eval(*b, env),
        env);
}

Sexp builtin_is_null(const Sexp& params, Environment& env) {
    auto& v = list_nth_elm(params, 0, env);
    return eval(v, env).get_type() == Sexp::TYPE_NIL;
}

Sexp builtin_quote(const Sexp& params, Environment& env) {
    return list_nth_elm(params, 0, env);
}

Sexp builtin_define(const Sexp& params, Environment& env) {
    // TODO

    return Sexp{};
}

#define ITEM(name, func) { name, BuiltinProc{ name, func } }
const std::map<std::string_view, BuiltinProc> BUILTINS{
    ITEM("+"sv, builtin_add),
    ITEM("-"sv, builtin_sub),
    ITEM("*"sv, builtin_mul),
    ITEM("/"sv, builtin_div),
    ITEM("if"sv, builtin_if),
    ITEM("="sv, builtin_eq),
    ITEM("<"sv, builtin_binary_op<std::less<>>),
    ITEM("<="sv, builtin_binary_op<std::less_equal<>>),
    ITEM(">"sv, builtin_binary_op<std::greater<>>),
    ITEM(">="sv, builtin_binary_op<std::greater_equal<>>),
    ITEM("car"sv, builtin_car),
    ITEM("cdr"sv, builtin_cdr),
    ITEM("cons"sv, builtin_cons),
    ITEM("null?"sv, builtin_is_null),
    ITEM("quote"sv, builtin_quote),
    ITEM("define"sv, builtin_define),
};
#undef ITEM

Sexp eval(const Sexp& sexp, Environment& env) {
    using enum Sexp::Type;

    switch (sexp.get_type()) {
        case TYPE_REF: {
            auto& cons_cell = env.lookup(sexp.as<MemoryLocation>());
            auto& sym = cons_cell.car;
            auto& params = cons_cell.cdr;

            if (sym.get_type() != TYPE_SYMBOL) throw "Invalid eval format";
            const auto& proc_name = sym.as<Symbol>().name;

            if (auto iter = BUILTINS.find(proc_name); iter != BUILTINS.end()) {
                auto& builtin_proc = iter->second;
                return builtin_proc.fn(params, env);
            }
        } break;

        case TYPE_SYMBOL: {
            // TODO eval variable
        } break;

        // For literal x, (eval x) => x
        default: return sexp;
    }
}

}
