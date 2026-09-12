// 01_inheritance.cpp
// ============================================================
// Basic inheritance -- base/derived, member reuse, access levels,
// construction order
// ============================================================
//   g++ -std=c++20 -Wall -Wextra -Wshadow -g 01_inheritance.cpp -o inh && ./inh
// ============================================================
//   class Derived : public Base { ... };
//   -> Derived gets Base's members (public + protected), plus its own.
//   -> Base subobject Derived ke andar embedded hota hai (offset 0).
//   -> Construction: Base pehle, phir Derived. Destruction: ulta.
// ============================================================

#include <iostream>
#include <string>

class Vehicle {
    std::string plate_;                 // private -- Derived se bhi nahi
protected:
    int speed_ = 0;                     // protected -- Derived access kar sakta
public:
    explicit Vehicle(std::string plate) : plate_(std::move(plate)) {
        std::cout << "  Vehicle(" << plate_ << ") ctor\n";
    }
    ~Vehicle() { std::cout << "  ~Vehicle(" << plate_ << ")\n"; }

    void accelerate(int by) { speed_ += by; }        // inherited as-is
    int  speed() const { return speed_; }
    const std::string& plate() const { return plate_; }
};

class Car : public Vehicle {            // public inheritance -- "Car IS-A Vehicle"
    int doors_;
public:
    Car(std::string plate, int doors)
        : Vehicle(std::move(plate)),    // base ctor -- init list mein, sabse pehle
          doors_(doors) {
        std::cout << "  Car ctor (doors=" << doors_ << ")\n";
    }
    ~Car() { std::cout << "  ~Car()\n"; }

    void honk() const {
        // speed_ -> protected, accessible. plate_ -> private, NOT (use plate())
        std::cout << "  Car " << plate() << " beep! (speed " << speed_ << ")\n";
    }
    int doors() const { return doors_; }
};

void serviceVehicle(const Vehicle& v) {              // Car ko Vehicle& ki tarah le sakte
    std::cout << "  servicing " << v.plate() << " @ speed " << v.speed() << "\n";
}

int main() {
    std::cout << "=== construction (Base first, then Derived) ===\n";
    Car c{"MH12AB1234", 4};

    std::cout << "\n=== inherited + own members ===\n";
    c.accelerate(30);                   // from Vehicle
    c.accelerate(20);
    c.honk();                           // from Car (uses protected speed_)
    std::cout << "  doors = " << c.doors() << ", speed = " << c.speed() << "\n";

    std::cout << "\n=== Car IS-A Vehicle (upcast) ===\n";
    serviceVehicle(c);                  // Car& -> const Vehicle&  implicit
    Vehicle& vref = c;                  // reference to base subobject
    std::cout << "  vref.speed() = " << vref.speed() << "\n";
    // vref.honk();                     // ❌ Vehicle mein honk() nahi

    std::cout << "\n=== layout ===\n";
    std::cout << "  sizeof(Vehicle) = " << sizeof(Vehicle)
              << ", sizeof(Car) = " << sizeof(Car)
              << "   (Car = Vehicle subobject + doors_ + padding)\n";
    std::cout << "  &c == &(Vehicle&)c : " << std::boolalpha
              << (static_cast<void*>(&c) == static_cast<void*>(static_cast<Vehicle*>(&c)))
              << "   (base subobject offset 0 -- no virtual)\n";

    std::cout << "\n=== destruction (Derived first, then Base -- reverse) ===\n";
    return 0;                           // ~Car() then ~Vehicle()
}
