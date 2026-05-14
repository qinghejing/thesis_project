#include "base64.h"

#include <array>
#include <cctype>

std::string base64_decode(const std::string& input)
{
    static const std::string chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    static std::array<int, 256> table = []() {
        std::array<int, 256> values{};
        values.fill(-1);
        for (int i = 0; i < static_cast<int>(chars.size()); ++i) {
            values[static_cast<unsigned char>(chars[i])] = i;
        }
        return values;
    }();

    std::string output;
    int value = 0;
    int bits = -8;
    for (unsigned char ch : input) {
        if (std::isspace(ch)) {
            continue;
        }
        if (ch == '=') {
            break;
        }
        int decoded = table[ch];
        if (decoded < 0) {
            continue;
        }
        value = (value << 6) + decoded;
        bits += 6;
        if (bits >= 0) {
            output.push_back(static_cast<char>((value >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return output;
}
