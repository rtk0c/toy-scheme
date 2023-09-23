import std;
import yawarakai;

namespace fs = std::filesystem;
using namespace std::literals;

int main(int argc, char** argv) {
    if (argc > 1) {
        fs::path input_file(argv[1]);
        
        std::ifstream ifs(input_file);
        if (!ifs) {
            std::cerr << "Unable to open input file.\n";
            return -1;
        }

        std::stringstream buffer;
        buffer << ifs.rdbuf();

        yawarakai::Heap heap;
        auto sexps = yawarakai::parse_sexp(buffer.view(), heap);
        for (auto& sexp : sexps) {
            std::cout << yawarakai::dump_sexp(sexp, heap) << '\n';
        }

        return 0;
    }

    std::cerr << "Supply an input file to run it.\n";
    return 0;
}
