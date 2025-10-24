#include <iostream>
#include <cstdint>

int main() {
    std::cout << "sizeof(bool) = " << sizeof(bool) << std::endl;
    std::cout << "sizeof(std::uint64_t) = " << sizeof(std::uint64_t) << std::endl;
    
    // Test what happens when we write two bools followed by a uint64_t
    bool b1 = false;
    bool b2 = true;
    std::uint64_t val = 50; // Some example value
    
    std::cout << "\nWriting: b1=" << b1 << ", b2=" << b2 << ", val=" << val << std::endl;
    
    // If we write them sequentially
    const char* data = reinterpret_cast<const char*>(&b1);
    std::cout << "b1 bytes: ";
    for (size_t i = 0; i < sizeof(b1); ++i) {
        std::cout << std::hex << (int)(unsigned char)data[i] << " ";
    }
    std::cout << std::dec << std::endl;
    
    return 0;
}
