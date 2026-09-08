#include <bitset>
#include <iostream>

#define for_bs(pos, bs) for(int pos = (int)(bs)._Find_first(); pos < (int)bs.size(); pos = (int)(bs)._Find_next(pos))

int main() {
    constexpr size_t N = 100;
    std::bitset<N> bs;
    // 例としていくつかのビットをセット
    bs.set(3);
    bs.set(5);
    bs.set(70);

    // _Find_first() で最初のオンビットの位置を取得し、
    // _Find_next() で次のオンビットの位置を取得します。
    // for (size_t pos = bs._Find_first(); pos < bs.size(); pos = bs._Find_next(pos)) {
    for_bs(pos, bs) {
        std::cout << "Bit " << pos << " is on.\n";
    }
    return 0;
}
