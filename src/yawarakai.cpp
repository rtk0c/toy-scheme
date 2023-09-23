// TODO this file should be of module `yawarakai:lisp`, but if we do that, nothing from the :lisp interface unit is available here -- xmake bug?
module yawarakai;

import std;

using namespace std::literals;

namespace yawarakai {

Heap::Heap() {
}

MemoryLocation Heap::push(ConsCell cons) {
    storage.push_back(std::move(cons));
    return storage.size() - 1;
}

const ConsCell& Heap::lookup(MemoryLocation addr) const {
    return storage[addr];
}

ConsCell& Heap::lookup(MemoryLocation addr) {
    return storage[addr];
}

Sexp cons(Sexp a, Sexp b, Heap& heap) {
    auto addr = heap.push(ConsCell{ std::move(a), std::move(b) });
    Sexp res;
    res.set(addr);
    return res;
}

void cons_inplace(Sexp a, Sexp& list, Heap& heap) {
    auto addr = heap.push(ConsCell{ std::move(a), std::move(list) });
    list = Sexp();
    list.set(addr);
}

bool is_list(const ConsCell& cons) {
    auto type = cons.cdr.get_type();
    return type == Sexp::TYPE_NIL || type == Sexp::TYPE_REF;
}

std::vector<Sexp> parse_sexp(std::string_view src, Heap& heap) {
    struct ParserStackFrame {
        std::vector<Sexp> children;
        const Sexp* wrapper = nullptr;
    };
    std::vector<ParserStackFrame> cs;

    cs.push_back(ParserStackFrame());

    size_t cursor = 0;
    const Sexp* next_sexp_wrapper = nullptr;

    auto push_sexp_to_parent = [&](Sexp sexp) {
        auto& target = cs.back().children;
        if (next_sexp_wrapper) {
            // Turns (my-sexp la la la) into (<the wrapper> (my-sexp la la la))
            target.push_back(make_list_v(heap, *next_sexp_wrapper, std::move(sexp)));
            next_sexp_wrapper = nullptr;
        } else {
            target.push_back(std::move(sexp));
        }
    };

    while (cursor < src.length()) {
        if (std::isspace(src[cursor])) {
            cursor += 1;
            continue;
        }

        // Eat comments
        if (src[cursor] == ';') {
            while (cursor < src.length() && src[cursor] != '\n')
                cursor += 1;
            continue;
        }

#define CHECK_FOR_WRAP(literal, sym) if (src[cursor] == literal) { next_sexp_wrapper = &sym; cursor++; continue; }
        CHECK_FOR_WRAP('\'', heap.sym.quote);
        CHECK_FOR_WRAP(',', heap.sym.unquote);
        CHECK_FOR_WRAP('`', heap.sym.quasiquote);
#undef CHECK_FOR_WRAP

        if (src[cursor] == '(') {
            ParserStackFrame psf;;
            if (next_sexp_wrapper) {
                psf.wrapper = next_sexp_wrapper;
                next_sexp_wrapper = nullptr;
            }

            cs.push_back(std::move(psf));

            cursor += 1;
            continue;
        }

        if (src[cursor] == ')') {
            if (cs.size() == 1) {
                throw "Error: unbalanced parenthesis";
            }

            ParserStackFrame& curr = cs.back();
            Sexp list;
            for (auto it = curr.children.rbegin(); it != curr.children.rend(); ++it) {
                cons_inplace(std::move(*it), list, heap);
            }
            if (curr.wrapper)
                list = make_list_v(heap, *curr.wrapper, std::move(list));
            cs.pop_back(); // Removes `curr`

            auto& parent = cs.back();
            parent.children.push_back(std::move(list));            

            cursor += 1;
            continue;
        }

        if (src[cursor] == '"') {
            cursor += 1;

            size_t str_size = 0;
            size_t str_begin = cursor;
            while (true) {
                // Break conditions
                if (cursor >= src.length())
                    throw "Error: unexpected end of file while parsing string";
                if (src[cursor] == '"')
                    break;

                if (src[cursor] == '\\') {
                    str_size += 1;
                    cursor += 2;
                    continue;
                }

                // otherwise...
                str_size += 1;
                cursor += 1;
            }
            cursor += 1;

            std::string str;
            str.reserve(str_size);

            size_t i = str_begin;
            while (i < str_begin + str_size) {
                if (src[i] == '\\') {
                    char esc = src[i + 1];
                    switch (esc) {
                        case 'n': str.push_back('\n');
                        case '\\': str.push_back('\\');
                        default: {
                            throw std::format("Invalid escaped char '{}'", esc);
                        } break;
                    }

                    i += 2;
                    continue;
                }

                str.push_back(src[i]);
                i += 1;
            }

            Sexp sexp;
            sexp.set(std::move(str));

            push_sexp_to_parent(std::move(sexp));

            continue;
        }

        {
            double v;
            auto [rest, ec] = std::from_chars(&src[cursor], &*src.end(), v);
            if (ec == std::errc()) {
                Sexp sexp;
                sexp.set(v);

                push_sexp_to_parent(std::move(sexp));

                cursor += rest - &src[cursor];
                continue;
            } else if (ec == std::errc::result_out_of_range) {
                throw "Error: number literal out of range"s;
            } else if (ec == std::errc::invalid_argument) {
                // Not a number
            }
        }

        {
            size_t symbol_size = 0;
            size_t symbol_begin = cursor;

            while (true) {
                if (cursor >= src.length())
                    break;
                char c = src[cursor];
                if (std::isspace(c) || c == '(' || c == ')')
                    break;

                symbol_size += 1;
                cursor += 1;
            }

            char next_c = src[cursor];
            if (std::isspace(next_c))
                cursor += 1;

            Sexp sexp;
            sexp.set(Symbol(std::string(&src[symbol_begin], symbol_size)));

            push_sexp_to_parent(std::move(sexp));
        }
    }

    // NB: this is not against NRVO: our return value is heap allocated anyways (in a std::vector), so we must use std::move
    return std::move(cs[0].children);
}

void dump_sexp_impl(std::string& output, const Sexp& sexp, const Heap& heap) {
    switch (sexp.get_type()) {
        using enum Sexp::Type;

        case TYPE_NIL: {
            output += "()";
        } break;

        case TYPE_NUM: {
            auto v = sexp.as<double>();

            constexpr auto BUF_SIZE = std::numeric_limits<double>::max_digits10;
            char buf[BUF_SIZE];
            auto res = std::to_chars(buf, buf + BUF_SIZE, v);
            
            if (res.ec == std::errc()) {
                output += std::string_view(buf, res.ptr);
            } else {
                throw "Error formatting number.";
            }
        } break;

        case TYPE_STRING: {
            auto& v = sexp.as<std::string>();

            output += '"';
            output += v;
            output += '"';
        } break;

        case TYPE_SYMBOL: {
            auto& v = sexp.as<Symbol>();

            output += v.name;
        } break;

        case TYPE_REF: {
            output += "(";
            traverse_list(sexp, heap, [&](const Sexp& elm) {
                dump_sexp_impl(output, elm, heap);
                output += " ";
            });
            output.pop_back(); // Remove the trailing space
            output += ")";
        } break;
    }
}

std::string dump_sexp(const Sexp& sexp, const Heap& heap) {
    std::string result;
    dump_sexp_impl(result, sexp, heap);
    return result;
}

}
