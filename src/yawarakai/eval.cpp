module;
#include "util.hpp"

module yawarakai;

import std;

using namespace std::literals;

namespace yawarakai {

namespace {
    Sexp wrap_number(double v) {
        // TODO makes this less wack
        if (auto n = static_cast<int32_t>(v); n == v)
            return Sexp(n);
        else
            return Sexp(static_cast<float>(v));
    }

    Sexp builtin_add(Sexp params, Environment& env) {
        double res = 0.0;

        for (auto& param : iterate(params, env)) {
            auto v = eval(param, env);
            switch (v.get_flags()) {
                case SCVAL_FLAG_INT: res += v.as_int(); break;
                case SCVAL_FLAG_FLOAT: res += v.as_float(); break;
                default: throw EvalException("+ cannot accept non-numerical parameters"s);
            }
        }

        return wrap_number(res);
    }

    Sexp builtin_sub(Sexp params, Environment& env) {
        double res = 0.0;
        int param_cnt = 0;
        for (auto& param : iterate(params, env)) {
            auto v = eval(param, env);
            double vf;
            switch (v.get_flags()) {
                case SCVAL_FLAG_INT: vf = v.as_int(); break;
                case SCVAL_FLAG_FLOAT: vf = v.as_float(); break;
                default: throw EvalException("- cannot accept non-numerical parameters"s);
            }

            if (param_cnt == 0)
                res = vf;
            else
                res -= vf;

            param_cnt += 1;
        }

        // Unary minus
        if (param_cnt == 1) {
            res = -res;
        }

        return wrap_number(res);
    }

    Sexp builtin_mul(Sexp params, Environment& env) {
        double res = 1.0;
        for (auto& param : iterate(params, env)) {
            auto v = eval(param, env);
            switch (v.get_flags()) {
                case SCVAL_FLAG_INT: res *= v.as_int(); break;
                case SCVAL_FLAG_FLOAT: res *= v.as_float(); break;
                default: throw EvalException("* cannot accept non-numerical parameters"s);
            }
        }

        return wrap_number(res);
    }

    Sexp builtin_div(Sexp params, Environment& env) {
        double res = 0.0;
        bool is_first = true;
        for (auto& param : iterate(params, env)) {
            auto v = eval(param, env);
            double vf;
            switch (v.get_flags()) {
                case SCVAL_FLAG_INT: vf = v.as_int(); break;
                case SCVAL_FLAG_FLOAT: vf = v.as_float(); break;
                default: throw EvalException("/ cannot accept non-numerical parameters"s);
            }

            if (is_first) {
                is_first = false;
                res = vf;
            } else {
                res /= vf;
            }
        }

        return wrap_number(res);
    }

    Sexp builtin_sqrt(Sexp params, Environment& env) {
        Sexp p;
        list_get_everything(params, { &p }, env);

        auto v = eval(p, env);
        double x;
        switch (v.get_flags()) {
            case SCVAL_FLAG_INT: x = v.as_int(); break;
            case SCVAL_FLAG_FLOAT: x = v.as_float(); break;
            default: throw EvalException("sqrt cannot accept non-numerical parameters"s);
        }

        double res = std::sqrt(x);

        return Sexp(static_cast<float>(res));
    }

    Sexp builtin_if(Sexp params, Environment& env) {
        Sexp cond;
        Sexp true_case;
        Sexp false_case;
        list_get_everything(params, { &cond, &true_case, &false_case }, env);

        Sexp cond_val = eval(cond, env);
        if (cond_val.evalute_bool()) {
            return eval(true_case, env);
        } else {
            return eval(false_case, env);
        }
    }

    Sexp builtin_eq(Sexp params, Environment& env) {
        bool is_first = true;
        Sexp prev;
        for (auto& param : iterate(params, env)) {
            Sexp curr = eval(param, env);

            if (is_first) {
                is_first = false;
                prev = curr;
                continue;
            }

            if (curr._value != prev._value)
                return Sexp(false);

            prev = curr;
        }

        return Sexp(true);
    }

    template <typename Op>
    Sexp builtin_binary_op(Sexp params, Environment& env) {
        bool is_first = true;
        double prev;
        Op op{};
        for (auto& param : iterate(params, env)) {
            auto v = eval(param, env);
            // TODO support both numeric types
            if (!v.is_float())
                throw EvalException("parameters must be numerical"s);

            double curr = v.as_float();
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

    Sexp builtin_car(Sexp params, Environment& env) {
        return car(eval(list_nth_elm(params, 0, env), env));
    }
    Sexp builtin_cdr(Sexp params, Environment& env) {
        return cdr(eval(list_nth_elm(params, 0, env), env));
    }
    Sexp builtin_cons(Sexp params, Environment& env) {
        Sexp a;
        Sexp b;
        list_get_everything(params, { &a, &b }, env);
        return cons(
            eval(a, env),
            eval(b, env),
            env);
    }

    Sexp builtin_is_null(Sexp params, Environment& env) {
        return Sexp(eval(car(params), env).is_nil());
    }

    Sexp builtin_quote(Sexp params, Environment& env) {
        return car(params);
    }

    Sexp builtin_define(Sexp params, Environment& env) {
        auto& curr_scope = env.curr_scope->bindings;

        Sexp declaration;
        Sexp body;
        list_get_prefix(params, { &declaration }, &body, env);

        switch (declaration.get_flags()) {
            // Defining a value
            case SCVAL_FLAG_SYMBOL: {
                auto& name = declaration.as_symbol();

                Sexp val;
                list_get_everything(body, { &val }, env);

                curr_scope.insert_or_assign(&name, eval(val, env));
            } break;

            // Defining a function
            case SCVAL_FLAG_PTR: {
                Sexp decl_name;
                Sexp decl_params;
                list_get_prefix(declaration, { &decl_name }, &decl_params, env);

                if (!decl_name.is_symbol())
                    throw EvalException("proc name must be a symbol"s);
                auto& proc_name = decl_name.as_symbol();

                auto p = make_user_proc(decl_params, body, env);
                p->name = &proc_name;

                env.curr_scope->bindings.insert_or_assign(&proc_name, Sexp(p));
            } break;

            default:
                throw EvalException("(define) expected symbol or func-declaration as 1st element"s);
        }

        return Sexp();
    }

    Sexp builtin_lambda(Sexp params, Environment& env) {
        Sexp decl_params;
        Sexp body;
        list_get_prefix(params, { &decl_params }, &body, env);

        auto p = make_user_proc(decl_params, body, env);

        return Sexp(p);
    }

    Sexp builtin_set(Sexp params, Environment& env) {
        Sexp binding;
        Sexp value;
        list_get_prefix(params, { &binding, &value }, nullptr, env);

        if (!binding.is_symbol())
            throw EvalException("(set!) expected symbol as 1st argument"s);

        env.set_binding(
            binding.as_symbol(),
            eval(value, env));

        return Sexp();
    }

    // (let ((id val-expr) ...) body ...)
    // (let* ((id val-expr) ...) body ...)
    Sexp do_let_unnamed(Sexp binding_forms, Sexp body, Environment& env, bool prebind_scope) {
        auto [scope, _] = env.heap.allocate<CallFrame>();
        scope->prev = HeapPtr(env.curr_scope);

        DEFER_RESTORE_VALUE(env.curr_scope);
        if (prebind_scope)
            env.curr_scope = scope;

        // Eval each let-binding-form
        for (auto& form : iterate(binding_forms, env)) {
            Sexp id;
            Sexp val_expr;
            list_get_prefix(form, { &id, &val_expr }, nullptr, env);

            if (!id.is_symbol())
                throw EvalException("(let) id must be a symbol");
            auto& id_sym = id.as_symbol();

            scope->bindings.try_emplace(&id_sym, eval(val_expr, env));
        }

        if (!prebind_scope)
            env.curr_scope = scope;

        return eval_many(body.as_ptr<ConsCell>().get(), env);
    }

    // (let proc-id ((id val-expr) ...) body ...)
    Sexp do_let_named(const Symbol& proc_name, Sexp binding_forms, Sexp body, Environment& env) {
        auto [scope, _] = env.heap.allocate<CallFrame>();
        scope->prev = HeapPtr(env.curr_scope);

        DEFER_RESTORE_VALUE(env.curr_scope);
        env.curr_scope = scope;

        // Extract parameter ids, and bind val-exprs after evaluating them
        std::vector<const Symbol*> proc_args;
        for (auto& form : iterate(binding_forms, env)) {
            Sexp id;
            Sexp val_expr;
            list_get_prefix(form, { &id, &val_expr }, nullptr, env);

            if (!id.is_symbol())
                throw EvalException("(let) id must be a symbol"s);
            auto& id_sym = id.as_symbol();

            proc_args.push_back(&id_sym);
            scope->bindings.try_emplace(&id_sym, eval(val_expr, env));
        }

        auto [proc, DISCARD] = env.heap.allocate_only<UserProc>();
        new (proc) UserProc{
            .closure_frame = HeapPtr(env.curr_scope),
            .arguments = std::move(proc_args),
            .body = body.as_ptr<ConsCell>(),
        };
        scope->bindings.try_emplace(&proc_name, Sexp(HeapPtr<void>(proc)));

        return eval_many(body.as_ptr<ConsCell>().get(), env);
    }

    Sexp do_let(Sexp params, Environment& env, bool prebind_scope) {
        Sexp arg_1st;
        Sexp arg_rest;
        list_get_prefix(params, { &arg_1st }, &arg_rest, env);

        auto [scope, _] = env.heap.allocate<CallFrame>();
        scope->prev = HeapPtr(env.curr_scope);

        if (arg_1st.is_symbol()) {
            Sexp binding_forms;
            Sexp body;
            list_get_prefix(arg_rest, { &binding_forms }, &body, env);

            return do_let_named(arg_1st.as_symbol(), binding_forms, body, env);
        } else {
            Sexp binding_forms = arg_1st;
            Sexp body = arg_rest;
            return do_let_unnamed(binding_forms, body, env, prebind_scope);
        }
    }

    Sexp builtin_let_basic(Sexp params, Environment& env) {
        return do_let(params, env, false);
    }
    Sexp builtin_let_star(Sexp params, Environment& env) {
        return do_let(params, env, true);
    }

    Sexp builtin_progn(Sexp params, Environment& env) {
        return eval_many(params.as_ptr<ConsCell>().get(), env);
    }
} // namespace

Sexp call_user_proc(const UserProc& proc, Sexp params, Environment& env) {
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

    return eval_many(proc.body.get(), env);
}

Sexp eval(Sexp sexp, Environment& env) {
    switch (sexp.get_flags()) {
        case SCVAL_FLAG_PTR: {
            auto& cons_cell = *sexp.as_ptr<ConsCell>();
            auto& func = cons_cell.car;
            auto& params = cons_cell.cdr;

            if (func.is_symbol()) {
                auto& proc_name = func.as_symbol();
                auto proc = env.lookup_binding(proc_name);

                if (auto up = proc->as_ptr<UserProc>())
                    return call_user_proc(*up, params, env);
                if (auto bp = proc->as_ptr<BuiltinProc>())
                    return bp->fn(params, env);

                throw EvalException(std::format("proc '{}' not found", std::string_view(proc_name)));
            }

            throw EvalException("(proc-call ...) form must begin with a symbol"s);
        } break;

        case SCVAL_FLAG_SYMBOL: {
            const auto& name = sexp.as_symbol();

            if (auto binding = env.lookup_binding(name))
                return *binding;

            // Non-existent binding evaluates to nil
            return Sexp();
        } break;

        // For every other sexp x, (eval x) => x
        default: return sexp;
    }
}

Sexp eval_maybe_many(Sexp forms, Environment& env) {
    if (!forms.is_ptr<ConsCell>())
        return eval(forms, env);
    else
        return eval_many(forms.as_ptr<ConsCell>().get(), env);
}

Sexp eval_many(ConsCell* forms, Environment& env) {
    const ConsCell* curr = forms;
    while (true) {
        bool has_next = !curr->cdr.is_nil();
        if (!has_next) {
            // Last form in proc body is returned
            return eval(curr->car, env);
        } else {
            eval(curr->car, env);
            curr = curr->cdr.as_ptr<ConsCell>().get();
        }
    }

    std::unreachable();
}

} // namespace yawarakai
