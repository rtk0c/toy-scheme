module yawarakai;

import std;

#include "util.hpp"

using namespace std::literals;

namespace yawarakai {

Sexp builtin_add(const Sexp& params, Environment& env) {
    double res = 0.0;
    for (auto& param : iterate(params, env)) {
        auto v = eval(param, env);
        if (!v.is<double>()) throw EvalException("+ cannot accept non-numerical parameters"s);

        res += v.as<double>();
    }

    return Sexp(res);
}

Sexp builtin_sub(const Sexp& params, Environment& env) {
    double res;
    int param_cnt = 0;
    for (auto& param : iterate(params, env)) {
        auto v = eval(param, env);
        if (!v.is<double>()) throw EvalException("- cannot accept non-numerical parameters"s);

        if (param_cnt == 0)
            res = v.as<double>();
        else
            res -= v.as<double>();

        param_cnt += 1;
    }

    // Unary minus
    if (param_cnt == 1) {
        res = -res;
    }

    return Sexp(res);
}

Sexp builtin_mul(const Sexp& params, Environment& env) {
    double res = 1.0;
    for (auto& param : iterate(params, env)) {
        auto v = eval(param, env);
        if (!v.is<double>()) throw EvalException("* cannot accept non-numerical parameters"s);

        res *= v.as<double>();
    }

    return Sexp(res);
}

Sexp builtin_div(const Sexp& params, Environment& env) {
    double res;
    bool is_first = true;
    for (auto& param : iterate(params, env)) {
        auto v = eval(param, env);
        if (!v.is<double>()) throw EvalException("/ cannot accept non-numerical parameters"s);

        if (is_first) {
            is_first = false;
            res = v.as<double>();
        } else {
            res /= v.as<double>();
        }
    }

    return Sexp(res);
}

Sexp builtin_sqrt(const Sexp& params, Environment& env) {
    const Sexp* p;
    list_get_everything(params, {&p}, env);

    auto v = eval(*p, env);
    if (!v.is<double>()) throw EvalException("sqrt cannot accept non-numerical parameters"s);

    double x = v.as<double>();
    double res = std::sqrt(x);

    return Sexp(res);
}

Sexp builtin_if(const Sexp& params, Environment& env) {
    const Sexp* cond;
    const Sexp* true_case;
    const Sexp* false_case;
    list_get_everything(params, {&cond, &true_case, &false_case}, env);

    Sexp cond_val = eval(*cond, env);
    if (cond_val.is<bool>() && cond_val.as<bool>()) {
        return eval(*true_case, env);
    } else {
        return eval(*false_case, env);
    }
}

Sexp builtin_eq(const Sexp& params, Environment& env) {
    const Sexp* a_lit;
    const Sexp* b_lit;
    list_get_everything(params, {&a_lit, &b_lit}, env);

    auto a = eval(*a_lit, env);
    auto b = eval(*b_lit, env);

    if (a.get_type() != b.get_type())
        return Sexp(false);

    switch (a.get_type()) {
        using enum Sexp::Type;
#define CASE(the_type) case the_type: return a.get<the_type>() == b.get<the_type>()
        case TYPE_NIL: return true;
        CASE(TYPE_NUM);
        CASE(TYPE_BOOL);
        CASE(TYPE_STRING);
        case TYPE_SYMBOL: return a.get<TYPE_SYMBOL>().name == b.get<TYPE_SYMBOL>().name;
        CASE(TYPE_REF);
        CASE(TYPE_BUILTIN_PROC);
        CASE(TYPE_USER_PROC);
#undef CASE
    }

    std::unreachable();
}

template <typename Op>
Sexp builtin_binary_op(const Sexp& params, Environment& env) {
    using enum Sexp::Type;

    const Sexp* a_lit;
    const Sexp* b_lit;
    list_get_everything(params, {&a_lit, &b_lit}, env);

    auto a = eval(*a_lit, env);
    auto b = eval(*b_lit, env);

    if (a.get_type() == TYPE_NUM && b.get_type() == TYPE_NUM) {
        auto res = Op{}(
            a.as<double>(),
            b.as<double>());
        return Sexp(res);
    } else {
        throw EvalException("parameters must be numerical"s);
    }
}

Sexp builtin_car(const Sexp& params, Environment& env) { return car(eval(list_nth_elm(params, 0, env), env), env); }
Sexp builtin_cdr(const Sexp& params, Environment& env) { return cdr(eval(list_nth_elm(params, 0, env), env), env); }
Sexp builtin_cons(const Sexp& params, Environment& env) {
    const Sexp* a;
    const Sexp* b;
    list_get_everything(params, {&a, &b}, env);
    return cons(
        eval(*a, env),
        eval(*b, env),
        env);
}

Sexp builtin_is_null(const Sexp& params, Environment& env) {
    const Sexp* v;
    list_get_everything(params, {&v}, env);
    return eval(*v, env).get_type() == Sexp::TYPE_NIL;
}

Sexp builtin_quote(const Sexp& params, Environment& env) {
    const Sexp* v;
    list_get_everything(params, {&v}, env);
    return *v;
}

Sexp builtin_define(const Sexp& params, Environment& env) {
    auto& curr_scope = env.scopes.back().bindings;

    const Sexp* declaration;
    const Sexp* body;
    list_get_prefix(params, {&declaration}, &body, env);

    switch (declaration->get_type()) {
        using enum Sexp::Type;

        // Defining a value
        case TYPE_SYMBOL: {
            auto& name = declaration->as<Symbol>().name;

            const Sexp* val;
            list_get_everything(*body, {&val}, env);

            curr_scope.insert_or_assign(name, eval(*val, env));
        } break;

        // Defining a function
        case TYPE_REF: {
            const Sexp* decl_name;
            const Sexp* decl_params;
            list_get_prefix(*declaration, {&decl_name}, &decl_params, env);

            if (decl_name->get_type() != TYPE_SYMBOL)
                throw EvalException("proc name must be a symbol"s);
            auto& proc_name = decl_name->as<Symbol>().name;

            std::vector<std::string> proc_args;
            for (const Sexp& param : iterate(*decl_params, env)) {
                if (param.get_type() != TYPE_SYMBOL)
                    throw EvalException("proc parameter must be a symbol"s);
                proc_args.push_back(param.get<TYPE_SYMBOL>().name);
            }

            if (body->get_type() != TYPE_REF)
                throw EvalException("proc body must have 1 or more forms"s);

            env.user_proc_pool.push_back(UserProc{
                .name = proc_name,
                .arguments = std::move(proc_args),
                .body = body->get<TYPE_REF>(),
            });
            env.scopes.back().bindings.insert_or_assign(
                proc_name,
                Sexp(env.user_proc_pool.back()));
        } break;

        default:
            throw EvalException("(define) name must be a symbol"s);
    }

    return Sexp();
}

#define ITEM(name, func) { name, BuiltinProc{ name, func } }
const std::map<std::string_view, BuiltinProc> BUILTINS{
    ITEM("+"sv, builtin_add),
    ITEM("-"sv, builtin_sub),
    ITEM("*"sv, builtin_mul),
    ITEM("/"sv, builtin_div),
    ITEM("sqrt"sv, builtin_sqrt),
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

Sexp eval_user_proc(const UserProc& proc, const Sexp& params, Environment& env) {
    using enum Sexp::Type;

    env.push_scope();
    DEFER { env.pop_scope(); };

    auto it_decl = proc.arguments.begin();
    auto it_value = SexpListIterator(params, env);
    while (it_decl != proc.arguments.end() && !it_value.is_end()) {
        auto& arg_name = *it_decl;
        auto arg_value = eval(*it_value, env);
        env.scopes.back().bindings.insert_or_assign(arg_name, std::move(arg_value));

        ++it_decl;
        ++it_value;
    }

    const ConsCell* curr = &env.lookup(proc.body);
    while (true) {
        bool has_next = curr->cdr.get_type() == TYPE_REF;

        // Last form in proc body is returned
        if (!has_next)
            return eval(curr->car, env);

        eval(curr->car, env);
        curr = &env.lookup(curr->cdr.as<MemoryLocation>());
    }

    std::unreachable();
}

Sexp eval(const Sexp& sexp, Environment& env) {
    using enum Sexp::Type;

    auto& curr_scope = env.scopes.back().bindings;
    switch (sexp.get_type()) {
        case TYPE_REF: {
            auto& cons_cell = env.lookup(sexp.as<MemoryLocation>());
            auto& sym = cons_cell.car;
            auto& params = cons_cell.cdr;

            if (sym.get_type() != TYPE_SYMBOL)
                throw EvalException("Invalid eval format"s);
            const auto& proc_name = sym.as<Symbol>().name;

            if (auto user_proc = env.lookup_binding(proc_name);
                user_proc && user_proc->get_type() == TYPE_USER_PROC)
            {
                return eval_user_proc(*user_proc->get<TYPE_USER_PROC>(), params, env);
            }
            if (auto iter = BUILTINS.find(proc_name); iter != BUILTINS.end()) {
                auto& builtin_proc = iter->second;
                return builtin_proc.fn(params, env);
            }

            throw EvalException("proc not found"s);
        } break;

        case TYPE_SYMBOL: {
            auto& name = sexp.as<Symbol>().name;

            if (auto binding = env.lookup_binding(name)) {
                return *binding;
            }
            if (auto iter = BUILTINS.find(name); iter != BUILTINS.end()) {
                return Sexp(iter->second);
            }

            throw EvalException("variable not bound"s);
        } break;

        // For literal x, (eval x) => x
        default: return sexp;
    }
}

}
