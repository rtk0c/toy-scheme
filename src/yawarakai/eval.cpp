module;
#include "util.hpp"

module yawarakai;

import std;

using namespace std::literals;

namespace yawarakai {

namespace {
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
    double res = 0.0;
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
    double res = 0.0;
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
    bool is_first = true;
    Sexp prev;
    for (auto& param : iterate(params, env)) {
        Sexp curr = eval(param, env);

        if (is_first) {
            is_first = false;
            prev = std::move(curr);
            continue;
        }

        if (curr.get_type() != prev.get_type())
            return Sexp(false);

        bool eq = false;
        switch (curr.get_type()) {
            using enum Sexp::Type;
#define CASE(the_type) case the_type: eq = curr.get<the_type>() == prev.get<the_type>(); break;
            case TYPE_NIL:
                // All nil's are equal
                break;
            CASE(TYPE_NUM);
            CASE(TYPE_BOOL);
            CASE(TYPE_STRING);
            case TYPE_SYMBOL:
                eq = curr.get<TYPE_SYMBOL>().name == prev.get<TYPE_SYMBOL>().name;
                break;
            CASE(TYPE_REF);
            CASE(TYPE_BUILTIN_PROC);
            CASE(TYPE_USER_PROC);
#undef CASE
        }
        if (!eq)
            return Sexp(false);

        prev = std::move(curr);
    }

    return Sexp(true);
}

template <typename Op>
Sexp builtin_binary_op(const Sexp& params, Environment& env) {
    bool is_first = true;
    double prev;
    Op op{};
    for (auto& param : iterate(params, env)) {
        auto v = eval(param, env);
        if (!v.is<double>())
            throw EvalException("parameters must be numerical"s);

        double curr = v.as<double>();
        if (!is_first) {
            bool success = op(prev, curr);
            if (!success)
                return Sexp(false);
        }
        is_first = false;
        prev = curr;
    }

    return Sexp(true);
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
    auto& curr_scope = env.curr_scope->bindings;

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

            auto p = make_user_proc(*decl_params, *body, env);
            p->name = proc_name;

            env.curr_scope->bindings.insert_or_assign(proc_name, Sexp(*p));
        } break;

        default:
            throw EvalException("(define) expected symbol or func-declaration as 1st element"s);
    }

    return Sexp();
}

Sexp builtin_lambda(const Sexp& params, Environment& env) {
    const Sexp* decl_params;
    const Sexp* body;
    list_get_prefix(params, {&decl_params}, &body, env);

    auto p = make_user_proc(*decl_params, *body, env);

    return Sexp(*p);
}

Sexp builtin_set(const Sexp& params, Environment& env) {
    using enum Sexp::Type;

    const Sexp* binding;
    const Sexp* value;
    list_get_prefix(params, {&binding, &value}, nullptr, env);

    if (binding->get_type() != TYPE_SYMBOL)
        throw EvalException("(set!) expected symbol as 1st argument"s);

    env.set_binding(
        binding->get<TYPE_SYMBOL>().name,
        eval(*value, env));

    return Sexp();
}

// (let ((id val-expr) ...) body ...)
// (let* ((id val-expr) ...) body ...)
Sexp do_let_unnamed(const Sexp& binding_forms, const Sexp& body, Environment& env, bool prebind_scope) {
    auto [scope, _] = env.heap.allocate<CallFrame>();
    scope->prev = env.curr_scope;

    DEFER_RESTORE_VALUE(env.curr_scope);
    if (prebind_scope)
        env.curr_scope = scope;

    // Eval each let-binding-form
    for (auto& form : iterate(binding_forms, env)) {
        const Sexp* id;
        const Sexp* val_expr;
        list_get_prefix(form, {&id, &val_expr}, nullptr, env);

        if (!id->is<Symbol>())
            throw EvalException("(let) id must be a symbol");

        scope->bindings.try_emplace(
            id->as<Symbol>().name,
            eval(*val_expr, env));
    }

    if (!prebind_scope)
        env.curr_scope = scope;

    return eval_many(body.as<MemoryLocation>(), env);
}

// (let proc-id ((id val-expr) ...) body ...)
Sexp do_let_named(const Symbol& sym, const Sexp& binding_forms, const Sexp& body, Environment& env) {
    using enum Sexp::Type;

    auto [scope, _] = env.heap.allocate<CallFrame>();
    scope->prev = env.curr_scope;

    DEFER_RESTORE_VALUE(env.curr_scope);
    env.curr_scope = scope;

    // Extract parameter ids, and bind val-exprs after evaluating them
    std::vector<std::string> proc_args;
    for (auto& form : iterate(binding_forms, env)) {
        const Sexp* id;
        const Sexp* val_expr;
        list_get_prefix(form, {&id, &val_expr}, nullptr, env);

        if (id->get_type() != TYPE_SYMBOL)
            throw EvalException("(let) id must be a symbol"s);
        auto& id_name = id->get<TYPE_SYMBOL>().name;

        proc_args.push_back(id_name);
        scope->bindings.try_emplace(id_name, eval(*val_expr, env));
    }

    auto [proc, DISCARD] = env.heap.allocate<UserProc>(UserProc{
        .closure_frame = env.curr_scope,
        .arguments = std::move(proc_args),
        .body = body.get<TYPE_REF>(),
    });
    scope->bindings.try_emplace(sym.name, Sexp(*proc));

    return eval_many(body.get<TYPE_REF>(), env);
}

Sexp do_let(const Sexp& params, Environment& env, bool prebind_scope) {
    const Sexp* arg_1st;
    const Sexp* arg_rest;
    list_get_prefix(params, {&arg_1st}, &arg_rest, env);

    auto [scope, _] = env.heap.allocate<CallFrame>();
    scope->prev = env.curr_scope;

    if (auto* sym = arg_1st->get_if<Symbol>()) {
        const Sexp* binding_forms;
        const Sexp* body;
        list_get_prefix(*arg_rest, {&binding_forms}, &body, env);

        return do_let_named(*sym, *binding_forms, *body, env);
    } else {
        const Sexp* binding_forms = arg_1st;
        const Sexp* body = arg_rest;
        return do_let_unnamed(*binding_forms, *body, env, prebind_scope);
    }
}

Sexp builtin_let_basic(const Sexp& params, Environment& env) { return do_let(params, env, false); }
Sexp builtin_let_star(const Sexp& params, Environment& env) { return do_let(params, env, true); }

Sexp builtin_progn(const Sexp& params, Environment& env) {
    return eval_many(params.as<MemoryLocation>(), env);
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
    ITEM("lambda"sv, builtin_lambda),
    ITEM("set!"sv, builtin_set),
    ITEM("let"sv, builtin_let_basic),
    ITEM("let*"sv, builtin_let_star),
};
#undef ITEM
} // namespace

Sexp eval_user_proc(const UserProc& proc, const Sexp& params, Environment& env) {
    using enum Sexp::Type;

    auto [s, _] = env.heap.allocate<CallFrame>();
    s->prev = proc.closure_frame;

    auto it_decl = proc.arguments.begin();
    auto it_value = SexpListIterator(params, env);
    int n_args = 0;
    while (it_decl != proc.arguments.end() && !it_value.is_end()) {
        auto& arg_name = *it_decl;
        // NOTE: we are still evaluating in the parent CallFrame, but merely storing the result in the current CallFrame
        auto arg_value = eval(*it_value, env);
        s->bindings.try_emplace(arg_name, std::move(arg_value));

        ++it_decl;
        ++it_value;
        n_args += 1;
    }

    if (it_decl != proc.arguments.end())
        throw EvalException(std::format("too few arguments provided to proc, expected {} but found {}", proc.arguments.size(), n_args));

    DEFER_RESTORE_VALUE(env.curr_scope);
    env.curr_scope = s;

    return eval_many(proc.body, env);
}

Sexp eval(const Sexp& sexp, Environment& env) {
    using enum Sexp::Type;

    switch (sexp.get_type()) {
        case TYPE_REF: {
            auto& cons_cell = env.lookup(sexp.as<MemoryLocation>());
            auto& sym = cons_cell.car;
            auto& params = cons_cell.cdr;

            if (sym.get_type() != TYPE_SYMBOL)
                throw EvalException("(proc-call ...) form must begin with a symbol"s);
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

            throw EvalException(std::format("proc '{}' not found", proc_name));
        } break;

        case TYPE_SYMBOL: {
            auto& name = sexp.as<Symbol>().name;

            if (auto binding = env.lookup_binding(name)) {
                return *binding;
            }
            if (auto iter = BUILTINS.find(name); iter != BUILTINS.end()) {
                return Sexp(iter->second);
            }

            throw EvalException(std::format("variable '{}' not bound", name));
        } break;

        // For literal x, (eval x) => x
        default: return sexp;
    }
}

Sexp eval_maybe_many(const Sexp& forms, Environment& env) {
    if (!forms.is<MemoryLocation>())
        return eval(forms, env);
    else
        return eval_many(forms.as<MemoryLocation>(), env);
}

Sexp eval_many(MemoryLocation forms, Environment& env) {
    using enum Sexp::Type;

    const ConsCell* curr = &env.lookup(forms);
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

}
