# 06 — Templates & generic programming problems

## Prerequisites
- `21-TEMPLATES/` (poora — function/class templates, specialization, variadics,
  `if constexpr`, concepts, CRTP)
- `22-MODERN-CPP/` (fold expressions, `std::index_sequence`)

## Yeh file kya hai
20 problems — generic functions, traits, SFINAE / concepts, tag dispatch, CRTP,
variadics, aur chhote metaprogramming builds (`tuple`, `variant`, expression
templates).

Har problem: `<details>` mein design + gotcha. Poora code →
[`11-solutions/06-templates-solutions.md`](11-solutions/06-templates-solutions.md).

Compile: `g++ -std=c++20 -Wall -Wextra file.cpp -o t && ./t`
(Concepts / `requires` ke liye `-std=c++20`.)

---

## Part A — Easy (~15 min)

### A1. Generic max / min / clamp
`template<class T> const T& maxv(const T&, const T&)` — aur `clamp(v, lo, hi)`.
`Pattern:` return by `const&`, single comparison.
<details><summary>Approach</summary>

`return a < b ? b : a;` — sirf `<` maango (STL convention). `clamp`: `v < lo ? lo
: (hi < v ? hi : v)`. Trap: `maxv(3, 4)` returns a **reference to a temporary**
agar args prvalue hon aur tum bind karke rakho → dangling. `std::max` mein bhi
yeh trap hai (`auto&& m = std::max(f(), g());`).
</details>

### A2. Generic swap
`template<class T> void swp(T& a, T& b)` — move se, copy se nahi.
<details><summary>Approach</summary>

`T t = std::move(a); a = std::move(b); b = std::move(t);`. `noexcept(
std::is_nothrow_move_constructible_v<T> && …)`. `std::swap` yehi karta; ADL se
type-specific `swap` bhi mil sakta (`using std::swap; swap(a,b);`).
</details>

### A3. print_all for any container
`template<class C> void print_all(const C&)` — vector, list, set, array, C-array
sab.
<details><summary>Approach</summary>

Range-`for` (`for (const auto& x : c)`) — begin/end free functions ke through
C-array bhi. Ya `template<class It> void print(It b, It e)`. C++20: constrain with
`std::ranges::range C`.
</details>

### A4. is_pointer trait by hand
`template<class T> struct is_ptr : std::false_type {};` — partial specialization
se `T*` ke liye true.
<details><summary>Approach</summary>

`template<class T> struct is_ptr<T*> : std::true_type {};`. `is_ptr_v<T>` helper.
Isi pattern se `is_reference`, `remove_pointer` (`::type = T` in the `T*`
partial spec), `is_same<A,B>`.
</details>

### A5. Function vs class template — overload vs specialize
`process(T)` — general + `int` ke liye special behavior. Function template
**specialize** karna kyun bura hai?
<details><summary>Answer</summary>

Function templates full-specialize ho sakte par **overload resolution** unko weird
tarah pick karta (specializations overload set mein participate nahi karte the way
you expect — Dimov/Abrahams). Better: plain `void process(int)` **overload** likho
(non-template exact match jeetta hai). Class templates freely partial+full
specialize.
</details>

### A6. Variadic sum
`template<class... Ts> auto sum(Ts... xs)` — fold expression.
<details><summary>Approach</summary>

`return (xs + ...);` (unary right fold). Empty pack → compile error unless
`(xs + ... + 0)`. Mixed types → `auto` return, `std::common_type_t<Ts...>`.
Pre-C++17: recursion (`head + sum(tail...)`) + base case.
</details>

---

## Part B — Medium (~25–35 min)

### B1. Constrain to integral
`template<class T> T half(T x)` sirf integral types ke liye — `enable_if` **aur**
`requires`, dono.
<details><summary>Approach</summary>

C++20: `template<std::integral T>` ya `T half(T x) requires std::integral<T>`.
Pre-20: `template<class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>`.
Concept version better diagnostics deta.
</details>

### B2. Tag dispatch vs if constexpr
`advance(it, n)` — random-access iterator pe `it += n`, warna loop. Dono style.
<details><summary>Approach</summary>

Tag dispatch: `advance_impl(it, n, std::random_access_iterator_tag)` vs
`…forward_iterator_tag`, dispatch on `typename std::iterator_traits<It>::
iterator_category{}`. `if constexpr`: `if constexpr (std::is_same_v<cat,
random_access…>) it += n; else while (n--) ++it;` — ek function, C++17. `if
constexpr` mein dono branches parse hote par sirf ek instantiate.
</details>

### B3. CRTP for static polymorphism
`template<class D> struct Shape { double area() const { return static_cast<const
D*>(this)->area_impl(); } };` — `Circle : Shape<Circle>`.
<details><summary>Approach</summary>

Base `interface()` derived ka `_impl()` call karta via `static_cast` — no vtable,
fully inlinable. Trap: `Shape<Circle>` aur `Shape<Square>` alag base types →
common `Shape*` container nahi. Use when the concrete type is known at the call
site (e.g. a templated algorithm).
</details>

### B4. TypeList
`template<class...> struct TypeList {};` — `size_v`, `at_t<N>`, `contains_v<T>`.
<details><summary>Approach</summary>

`size` = `sizeof...(Ts)`. `at`: recursive peel (`at<0, T, Rest...> = T`; `at<N,
T, Rest...> = at<N-1, Rest...>`). `contains` = `(std::is_same_v<T, Ts> || ...)`
fold. Basis for `tuple`, `variant`.
</details>

### B5. Compile-time factorial / fib
`constexpr` function (TMP struct nahi).
<details><summary>Approach</summary>

`constexpr uint64_t fact(unsigned n) { return n < 2 ? 1 : n * fact(n-1); }` —
`static_assert(fact(5) == 120);`. C++14+ mein loop bhi `constexpr` mein chalta.
Old-style `template<int N> struct Fact { static const int v = N * Fact<N-1>::v;
};` — slower compile, worse errors.
</details>

### B6. Perfect forwarding wrapper
`template<class F, class... A> auto invoke_log(F&& f, A&&... a)` — `f` ko args
ke saath call kare, forwarding perfectly, return value bhi forward.
<details><summary>Approach</summary>

`return std::forward<F>(f)(std::forward<A>(a)...);`. `decltype(auto)` return
(preserve ref-ness). Universal refs (`T&&` in deduced context) + `std::forward`
= "call it exactly as the caller would have". `emplace_back` isi pe khada.
</details>

### B7. Generic ScopeGuard
File 04 B8 ka template version — koi bhi callable le.
<details><summary>Approach</summary>

`template<class F> class ScopeGuard { F f_; bool live_ = true; public:
explicit ScopeGuard(F f) : f_(std::move(f)) {} ~ScopeGuard() { if (live_) f_(); }
ScopeGuard(ScopeGuard&& o) …; void dismiss(); };` + `template<class F>
ScopeGuard(F) -> ScopeGuard<F>;` (CTAD). Ya `auto g = makeGuard(lambda);`.
</details>

### B8. Detect .reserve()
`template<class C> void maybe_reserve(C& c, size_t n)` — agar `C` mein
`reserve` hai to call karo, warna nop.
<details><summary>Approach</summary>

C++20: `if constexpr (requires { c.reserve(n); }) c.reserve(n);`. Pre-20:
`std::void_t` detection idiom — `template<class, class = void> struct
has_reserve : std::false_type {}; template<class C> struct has_reserve<C,
std::void_t<decltype(std::declval<C&>().reserve(0))>> : std::true_type {};`.
</details>

### B9. conditional / decay usage
`template<class T> struct StorageFor` — agar `T` chhota+trivial → `T` by value,
warna `const T&`. `std::conditional` se.
<details><summary>Approach</summary>

`using type = std::conditional_t<(sizeof(T) <= 16 && std::is_trivially_copyable_v
<T>), T, const T&>;`. `std::decay_t` — array→ptr, function→ptr, cv/ref strip
(jaise pass-by-value karta). Use in generic function param types.
</details>

---

## Part C — Hard (~45+ min)

### C1. Minimal tuple
`Tuple<Ts...>` — `get<N>`, `sizeof...`, CTAD. Recursive inheritance ya
`index_sequence` + typed bases.
<details><summary>Design</summary>

Recursive: `template<class H, class... T> struct Tuple<H, T...> : Tuple<T...> {
H head; };` — `get<0>` returns `head`, `get<N>` recurses into base. EBO for
empty types. Layout order technically implementation-defined (real `std::tuple`
often reverses). `get<N>` via `static_cast` to the right base.
</details>

### C2. variant-lite
`Variant<Ts...>` — `alignas` storage, active type index, `get<T>`, `visit`.
<details><summary>Design</summary>

`alignas(max_align) std::byte buf[max_size]; int idx_;`. `emplace<T>` →
placement new, set `idx_`. Dtor → switch/visit on `idx_` calling correct `~T()`.
`visit(f)` → jump table `f` ke saath (`(idx_ == I ? f(get<I>()) : …)` generated
via `index_sequence`). Valueless-by-exception corner. Real `std::variant` is
~1000 lines.
</details>

### C3. for_each_in_tuple
`template<class Tup, class F> void for_each(Tup&& t, F f)` — har element pe `f`.
<details><summary>Approach</summary>

`std::apply([&](auto&&... xs){ (f(xs), ...); }, t);` — C++17 one-liner. Manual:
`[&]<size_t... I>(std::index_sequence<I...>){ (f(std::get<I>(t)), ...); }
(std::make_index_sequence<std::tuple_size_v<std::decay_t<Tup>>>{});`.
</details>

### C4. constexpr format-spec parser
Ek `constexpr` function jo `"{:>8.2f}"` jaisa mini format spec parse kare into a
struct at compile time (`{width:8, precision:2, align:right, type:'f'}`).
<details><summary>Approach</summary>

`constexpr` char-by-char walk over a `std::string_view`; return a
`struct Spec`. `static_assert(parse("{:>8.2f}").width == 8);`. `std::format`
(C++20) khud compile-time spec validation karta `consteval` ke through. Insight:
C++20+ mein string parsing compile time pe possible.
</details>

### C5. Expression-template Vec
`Vec` jahan `a + b + c` teen temporaries + teen loops nahi, ek fused loop banata.
<details><summary>Design</summary>

`operator+` ek lightweight `Sum<LHS, RHS>` expression node return karta (koi
data copy nahi, bas refs). `Vec` ka ctor / `operator=` jab `Sum` se assign hota
to `for i: result[i] = expr[i]` — `expr[i]` recursively `lhs[i] + rhs[i]`.
Ek loop, zero temporaries. **Danger:** `auto x = a + b;` — `x` ek expression
node hai jo `a`, `b` ke refs rakhta; agar `a`/`b` temp the → dangling. Isliye
Eigen `auto` se manaa karta.
</details>

---

## Next
→ [`07-concurrency-problems.md`](07-concurrency-problems.md)
