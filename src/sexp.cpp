module yawarakai;

import std;
import std.compat;

using namespace std::literals;

namespace yawarakai {

Heap::Heap() {
}

MemoryLocation Heap::push(ConsCell cons) {
    storage.push_back(std::move(cons));
    return storage.size() - 1;
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

static bool is_char_in_symbol(char c) {
    return !(std::isspace(c) || c == '(' || c == ')');
}

Sexp parse_sexp(std::string_view src, Heap& heap) {
    struct ParserStackFrame {
        Sexp the_list;
    };

    // Parser Call Stack
    std::vector<ParserStackFrame> pcs;

    pcs.push_back(ParserStackFrame());

    size_t cursor = 0;

    while (cursor < src.length()) {
        if (auto c = src[cursor];
            c == ' ' || c == '\t' || c == '\n')
        {
            cursor += 1;
            continue;
        }

        // TODO
        // if (src[cursor] == '\'') {
        //     pcs.push_back(ParserStackFrame());
        //     const auto& quoter = pcs.rbegin()[0];
        //     const auto& parent = pcs.rbegin()[1]
        //     parent.;
        //     cursor += 1;
        //     continue;
        // }

        if (src[cursor] == '(') {
            pcs.push_back(ParserStackFrame());

            cursor += 1;
            continue;
        }

        if (src[cursor] == ')') {
            if (pcs.size() == 1) {
                throw "Error: unbalanced parenthesis";
            }

            auto& curr = pcs.rbegin()[0];
            auto& parent = pcs.rbegin()[1];
            cons_inplace(curr.the_list, parent.the_list, heap);
            pcs.pop_back();

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

            auto& parent = pcs.back();
            cons_inplace(std::move(sexp), parent.the_list, heap);

            continue;
        }

        {
            double v;
            auto [rest, ec] = std::from_chars(&src[cursor], &*src.end(), v);
            if (ec == std::errc()) {
                Sexp sexp;
                sexp.set(v);

                auto& parent = pcs.back();
                cons_inplace(std::move(sexp), parent.the_list, heap);

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
                if (!is_char_in_symbol(src[cursor]))
                    break;

                symbol_size += 1;
                cursor += 1;
            }
            cursor += 1;

            Sexp sexp;
            sexp.set(Symbol(std::string(&src[symbol_begin], symbol_size)));

            auto& parent = pcs.back();
            cons_inplace(std::move(sexp), parent.the_list, heap);
        }
    }

    return pcs[0].the_list;
}

std::string dump_sexp(Sexp sexp, const Heap& heap) {
    std::string result;

    // TODO

    return result;
}

}
