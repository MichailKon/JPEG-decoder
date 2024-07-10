#include "include/decoder.h"
#include "include/image.h"
#include <fstream>
#include <iostream>

int main() {
    std::ifstream fin("bad_quality.jpg");
    if (!fin.is_open()) {
        std::cerr << "Can't find bad_quality.jpg\n";
        return 1;
    }
    Image res = Decode(fin);
    fin.close();
    std::cout << "Image size " << res.Width() << "x" << res.Height() << " px\n";
    std::cout << "Comment: " << res.GetComment() << '\n';
}
