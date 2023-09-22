import std;
import yawarakai;

using namespace std::literals;

int main(int argc, char** argv) {
    // yawarakai::Memory mem;
    constexpr std::string_view test_src = R"""(
(define (my-function a b)
  (+ a b))
(define my-list (foo bar "a string" 42 3.14159 () "more string"))
)"""sv;
    yawarakai::Heap heap;
    auto sexp = yawarakai::parse_sexp(test_src, heap);
    std::cout << yawarakai::dump_sexp(sexp, heap);
    return 0;
}
