#include "tomos/tomos.hpp"
using namespace tomo;

int main() {
    using T = float;
    constexpr dimension D = 3_D;

    int size = 128;
    auto v = tomo::volume<D, T>(size);

    auto g = tomo::geometry::cone_beam<T>(v, size, {(T)2.0, (T)2.0},
                                          {size, size}, (T)5.5, (T)3.5);
    auto k = tomo::dim::closest<D, T>(v);

    int nzs = 0;
    for (auto [idx, line] : g) {
        (void)idx;
        for (auto elem : k(line)) {
            (void)elem;
            nzs++;
        }
    }

    std::cout << "%%MatrixMarket matrix coordinate real general\n";
    std::cout << g.lines() << " " << v.cells() << " " << nzs << "\n";
    for (auto [idx, line] : g) {
        (void)idx;
        for (auto elem : k(line)) {
            std::cout << idx + 1 << " " << elem.index + 1 << " 1.0\n";
        }
    }
}
