import std;
import yawarakai;

using namespace std::literals;

int main(int argc, char** argv) {
    // yawarakai::Memory mem;
    constexpr std::string_view test_src = R"""(
1
2
3
"test"
word
(1 2 3)
(1 (2 3))
(symbol)
(symbol (and nesting symbols))
(symbol (and ("mixed" nesting 5 symbols)))
(define (my-function a b)
  (+ a b))
(define my-list (foo bar "a string" 42 3.14159 () "more string"))
)"""sv;
    yawarakai::Heap heap;
    auto sexps = yawarakai::parse_sexp(test_src, heap);
    for (auto& sexp : sexps) {
        std::cout << yawarakai::dump_sexp(sexp, heap) << '\n';
    }
    return 0;
}
