#include "test.hpp"
#include <cstring>

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int ran = 0, bad = 0;
    for (auto& c : sft::registry()) {
        if (filter && !std::strstr(c.name, filter)) continue;
        int before = sft::failures();
        try { c.fn(); }
        catch (const std::exception& e) { ++sft::failures(); std::cerr << "  EXCEPTION in " << c.name << ": " << e.what() << "\n"; }
        ++ran;
        if (sft::failures() != before) { ++bad; std::cerr << "FAILED " << c.name << "\n"; }
    }
    std::cout << ran << " tests, " << bad << " failed\n";
    return bad ? 1 : 0;
}
