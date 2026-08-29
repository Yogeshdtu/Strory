// pipeline_demo.cpp
// ============================================================
// LESSON 08 ka example: compilation pipeline explore karna
// ============================================================
//
// Ise aise chalao (ek-ek karke, output dekhte hue):
//
//   g++ -E pipeline_demo.cpp -o pipeline_demo.i     # sirf preprocess
//   wc -l pipeline_demo.i                            # kitni lines bani? (hazaaron!)
//
//   g++ -S pipeline_demo.cpp -o pipeline_demo.s     # assembly tak
//   grep -A10 "main:" pipeline_demo.s
//
//   g++ -c pipeline_demo.cpp -o pipeline_demo.o     # object file
//   nm -C pipeline_demo.o                            # symbols dekho
//
//   g++ pipeline_demo.o -o pipeline_demo            # link
//   ldd pipeline_demo                                # shared libraries
//   ./pipeline_demo                                  # chalao
// ============================================================

#include <iostream>

// `#define` ek PREPROCESSOR macro hai.
// Preprocessor har jagah `GREETING` ko `"Namaste"` se text-replace kar dega.
// Compiler ko `GREETING` kabhi dikhta hi nahi.
#define GREETING "Namaste"

// `#define` se ek chhota "function-like macro" bhi ban sakta hai.
// Yeh bhi sirf text replacement hai -- koi type checking nahi hoti.
#define SQUARE(x) ((x) * (x))

int main() {
    // Yeh comment preprocessor hata dega.
    // `pipeline_demo.i` file mein aapko yeh comment nahi milega.

    // `GREETING` yahan `"Namaste"` ban jayega, compile hone se pehle hi.
    std::cout << GREETING << " Duniya!\n";

    // Macro expand hoke: ((5) * (5))
    std::cout << "5 ka square = " << SQUARE(5) << "\n";

    // __LINE__, __FILE__ bhi preprocessor macros hain -- compiler inhe bhar deta hai.
    std::cout << "Yeh line number hai: " << __LINE__ << "\n";
    std::cout << "Yeh file ka naam hai: " << __FILE__ << "\n";

    // __cplusplus batata hai ki kaunsa C++ standard use ho raha hai.
    //   199711 = C++98/03
    //   201103 = C++11
    //   201402 = C++14
    //   201703 = C++17
    //   202002 = C++20
    std::cout << "C++ standard version: " << __cplusplus << "\n";

    // `return 0` ka matlab: OS ko batao ki program successfully khatam hua.
    return 0;
}
