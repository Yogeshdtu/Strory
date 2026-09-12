#define GREET_BUILD          // is TU se symbols EXPORT ho rahe hain
#include "greet.hpp"

namespace greet {

const char* hello() { return "hello from shared lib"; }

int bump() {
    static int n = 0;        // library ke andar rehne wala state
    return ++n;
}

}  // namespace greet
