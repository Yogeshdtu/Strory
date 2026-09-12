// 02_virtual_functions.cpp
// ============================================================
// Virtual functions -- runtime polymorphism (dynamic dispatch)
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 02_virtual_functions.cpp -o vf && ./vf
// ============================================================
//   virtual  -> "call ko RUNTIME pe decide karo, actual object ke type se"
//   Bina virtual: call static type (declared type) se decide -> Base ka version
//   Ke saath virtual: call dynamic type (asli object) se -> Derived ka override
// ============================================================

#include <iostream>
#include <memory>
#include <vector>

struct Shape {
    virtual ~Shape() = default;                     // virtual dtor (base class -- file 07)

    virtual double area() const = 0;                // pure virtual -> Shape abstract (file 06)
    virtual const char* name() const = 0;

    void describe() const {                         // non-virtual -- virtual ko call karta
        std::cout << "  " << name() << " area = " << area() << "\n";
    }
};

struct Circle : Shape {
    double r_;
    explicit Circle(double r) : r_(r) {}
    double area() const override { return 3.14159265 * r_ * r_; }   // override
    const char* name() const override { return "Circle"; }
};

struct Rect : Shape {
    double w_, h_;
    Rect(double w, double h) : w_(w), h_(h) {}
    double area() const override { return w_ * h_; }
    const char* name() const override { return "Rect"; }
};

struct Square : Rect {
    explicit Square(double s) : Rect(s, s) {}
    const char* name() const override { return "Square"; }         // area() Rect ka reuse
};

// non-virtual demo -- static dispatch
struct Animal {
    void speak() const { std::cout << "  <animal noise>\n"; }      // NOT virtual
    virtual void speakV() const { std::cout << "  <animal noise V>\n"; }
    virtual ~Animal() = default;
};
struct Dog : Animal {
    void speak() const { std::cout << "  Woof\n"; }                // HIDES Animal::speak (not override)
    void speakV() const override { std::cout << "  Woof V\n"; }
};

int main() {
    std::cout << "=== polymorphism via base pointer ===\n";
    std::vector<std::unique_ptr<Shape>> shapes;
    shapes.push_back(std::make_unique<Circle>(2.0));
    shapes.push_back(std::make_unique<Rect>(3.0, 4.0));
    shapes.push_back(std::make_unique<Square>(5.0));

    for (const auto& s : shapes)
        s->describe();                             // har call -> actual type ka area()/name()

    std::cout << "\n=== virtual vs non-virtual dispatch ===\n";
    Dog d;
    Animal& a = d;                                  // base reference to a Dog
    std::cout << "  a.speak()  (non-virtual -> Animal::speak): ";  a.speak();
    std::cout << "  a.speakV() (virtual     -> Dog::speakV)  : ";  a.speakV();
    std::cout << "  d.speak()  (static type Dog)             : ";  d.speak();

    std::cout << "\n=== calling virtual from a base method ===\n";
    shapes[0]->describe();   // describe() (non-virtual) -> area() (virtual) -> Circle::area

    std::cout <<
        "\n"
        "  virtual  -> call resolves by the OBJECT's real type at runtime\n"
        "  no virtual -> call resolves by the POINTER/REFERENCE's declared type\n"
        "  override -> compiler-checked 'yeh base ke virtual ko replace karta hai'\n";
    return 0;
}
