# 02 — Arrays & strings problems

## Prerequisites
- `09-ARRAYS/`, `10-STRINGS/`
- Two-pointer / sliding window: `20-ALGORITHMS-DSA/02-arrays-and-two-pointers.md`
- Hash maps: `20-ALGORITHMS-DSA/08-hash-tables.md`
- Prefix sums, Kadane: `20-ALGORITHMS-DSA/14-dynamic-programming.md`

## Yeh file kya hai
40 problems. Yeh **interview ka sabse bada surface area** hai — two-pointer,
sliding window, prefix sum, hash map, in-place tricks. Har problem: statement +
`Pattern:` hint + `<details>` approach + complexity.

Poora code chahiye → [`11-solutions/02-arrays-strings-solutions.md`](11-solutions/02-arrays-strings-solutions.md).

Patterns cheat-sheet: two-pointer · sliding window (fixed / variable) · prefix
sum + hash map · sort-then-scan · in-place index-as-hash · monotonic
stack/deque · expand-around-center.

---

## Part A — Easy (~10–15 min)

### A1. Two Sum
`std::vector<int> a`, `int target`. Do indices lauta do jinka sum `target` hai.
Exactly ek solution hai.
`Pattern:` hash map value→index, one pass.
<details><summary>Approach</summary>

Har `a[i]` pe check `target - a[i]` map mein hai kya; nahi to `a[i]→i` daalo.
`O(n)` time, `O(n)` space. Sorted hota to two-pointer `O(n)`/`O(1)` par indices
kho jaate.
</details>

### A2. Maximum subarray (Kadane)
Contiguous subarray ka max sum. Array mein negatives hain.
`Pattern:` running sum, reset when negative.
<details><summary>Approach</summary>

`cur = max(x, cur + x); best = max(best, cur);`. `O(n)`/`O(1)`. All-negative
case: `best` ko `a[0]` se init karo, `0` se nahi. Indices bhi chahiye to `cur`
reset pe start-index note karo.
</details>

### A3. Move zeroes
Saare `0` end pe, baaki ka order preserve. In-place.
`Pattern:` slow/fast write pointer.
<details><summary>Approach</summary>

`w = 0`; `for r: if (a[r] != 0) std::swap(a[w++], a[r]);`. `O(n)`/`O(1)`.
Non-zero count = final `w`; baaki already `0` (swap ne push kiye).
</details>

### A4. Remove duplicates from sorted array
Sorted `a`, duplicates in-place hatao, naya length lauta do.
`Pattern:` slow/fast pointer.
<details><summary>Approach</summary>

`w = 1`; `for r in 1..n: if (a[r] != a[w-1]) a[w++] = a[r];`. `O(n)`/`O(1)`.
Empty → `0`.
</details>

### A5. Rotate array by k
`a` ko right se `k` steps rotate karo, in-place, `O(1)` extra.
`Pattern:` reverse whole, reverse two parts.
<details><summary>Approach</summary>

`k %= n;` `reverse(a, a+n); reverse(a, a+k); reverse(a+k, a+n);`. `O(n)`/`O(1)`.
Juggling / cyclic-replacement bhi `O(1)` space but reversal likhna sabse aasan.
</details>

### A6. Majority element (> n/2)
Guarantee: aisa element hai.
`Pattern:` Boyer–Moore voting.
<details><summary>Approach</summary>

`cand, cnt=0`; har `x`: `cnt==0 → cand=x`; `cnt += (x==cand) ? 1 : -1`. Majority
bach jaata hai. `O(n)`/`O(1)`. Guarantee na ho to second pass se verify.
</details>

### A7. Best time to buy/sell stock I
`price[]`, ek buy phir ek later sell. Max profit (`0` agar koi profit nahi).
`Pattern:` running min + best diff.
<details><summary>Approach</summary>

`minSoFar = price[0]`; `best = max(best, price[i] - minSoFar); minSoFar =
min(minSoFar, price[i]);`. `O(n)`/`O(1)`.
</details>

### A8. Contains duplicate
Koi value do baar aati hai kya?
`Pattern:` hash set, ya sort.
<details><summary>Approach</summary>

`unordered_set`: insert, agar already present → true. `O(n)`/`O(n)`. Space O(1)
chahiye aur array mutate kar sakte ho → sort + adjacent compare, `O(n log n)`.
</details>

### A9. Single number
Har element do baar aata hai, ek ke alawa. Woh dhundo. `O(1)` space.
`Pattern:` XOR all.
<details><summary>Approach</summary>

`x ^ x == 0`, XOR commutative → saare XOR karo, jodidaar cancel, akela bachta hai.
`O(n)`/`O(1)`. Variant "har element 3 baar, ek 1 baar" → bit-count `% 3`.
</details>

### A10. Plus one
`digits[]` (ek bada number, MSB first) mein `1` add karo, in-place / new vector.
`Pattern:` carry propagate from the back.
<details><summary>Approach</summary>

Peeche se: `d[i] < 9 → ++d[i]; return`; warna `d[i] = 0`, carry aage. Loop khatam
aur carry bacha → front pe `1` insert (`999 → 1000`). `O(n)`.
</details>

### A11. Merge sorted array in place
`a` ka size `m+n`, pehle `m` filled + `n` zeros at end; `b` ka size `n`. `a` mein
merge karo.
`Pattern:` fill from the back.
<details><summary>Approach</summary>

`i=m-1, j=n-1, w=m+n-1`; bada wala `a[w--]` pe likho. Peeche se likhne se
unread `a` values overwrite nahi hoti. `O(m+n)`/`O(1)`.
</details>

### A12. Valid palindrome
Sirf alphanumeric consider karo, case-insensitive.
`Pattern:` two pointers, skip non-alnum.
<details><summary>Approach</summary>

`l=0, r=n-1`; `l<r`: non-alnum skip karo dono taraf; `tolower(s[l]) !=
tolower(s[r]) → false`; else `++l, --r`. `O(n)`/`O(1)`. `std::isalnum` ko
`unsigned char` cast ke saath call karo.
</details>

### A13. Reverse words in a string
`"  the sky  is blue "` → `"blue is sky the"`. Extra spaces collapse.
`Pattern:` split on spaces, reverse order.
<details><summary>Approach</summary>

`std::istringstream` + `>>` (whitespace skip free) → words vector → reverse-join.
`O(n)`. In-place `O(1)`: whole reverse, phir har word individually reverse, phir
spaces normalize.
</details>

### A14. First unique character
String mein pehla non-repeating char ka index (`-1` agar koi nahi).
`Pattern:` frequency array, second pass.
<details><summary>Approach</summary>

`int cnt[128]` fill; phir left→right pehla `cnt[c]==1`. `O(n)`, `O(1)` space
(fixed alphabet).
</details>

---

## Part B — Medium (~25–35 min)

### B1. Product of array except self
`out[i] = ∏ a[j], j != i`. **Division mat use karo.** `O(1)` extra space (output
array count nahi hota).
`Pattern:` prefix products + suffix pass.
<details><summary>Approach</summary>

Pass 1: `out[i] = ∏ a[0..i-1]` (prefix). Pass 2: running `suffix`, `out[i] *=
suffix; suffix *= a[i];`. `O(n)`/`O(1)`. Zero handling free milta hai.
</details>

### B2. 3-Sum
Saare unique triples `a[i]+a[j]+a[k] == 0`.
`Pattern:` sort + fix one + two-pointer.
<details><summary>Approach</summary>

Sort. `i` fix karo, `l=i+1, r=n-1` two-pointer. Duplicates skip (`i`, `l`, `r`
teeno pe). `O(n²)` time, `O(1)` extra (sort ke alawa).
</details>

### B3. Container with most water
`height[]`, do lines + x-axis. Max area.
`Pattern:` two pointers from ends, move the shorter.
<details><summary>Approach</summary>

`area = min(h[l], h[r]) * (r - l)`. Chhoti line waali taraf move karo (badi rakhne
se hi area badh sakta hai). `O(n)`/`O(1)`.
</details>

### B4. Longest substring without repeating chars
`Pattern:` variable sliding window + last-seen map.
<details><summary>Approach</summary>

`lastSeen[c]`; `r` badhao; agar `s[r]` window ke andar (`lastSeen[s[r]] >= l`) →
`l = lastSeen[s[r]] + 1`. `best = max(best, r - l + 1)`. `O(n)`.
</details>

### B5. Group anagrams
Strings ko permutation-classes mein group karo.
`Pattern:` canonical key (sorted string / 26-count signature).
<details><summary>Approach</summary>

Key = sorted chars ya `"a1b0c2…"` count string. `unordered_map<string,
vector<string>>`. Count-key: `O(Σ Lᵢ)`; sorted-key: `O(Σ Lᵢ log Lᵢ)`.
</details>

### B6. Longest common prefix
String array ka LCP.
`Pattern:` vertical scan, ya divide-and-conquer.
<details><summary>Approach</summary>

Column `j` pe har string ka `s[j]` compare; mismatch / string end → `s[0..j)`.
`O(Σ Lᵢ)` worst. Ya pehle string ko prefix maan ke har agli string se shrink karo.
</details>

### B7. String to integer (atoi)
Leading spaces skip, optional sign, digits, non-digit pe ruk jao, `int` range
mein clamp.
`Pattern:` check overflow **before** `res = res*10 + d`.
<details><summary>Approach</summary>

`if (res > INT_MAX/10 || (res == INT_MAX/10 && d > 7)) return sign>0 ? INT_MAX :
INT_MIN;`. Simpler safe bound: `res > (INT_MAX - d) / 10`. Arithmetic pehle
karke check karna signed-overflow UB. `O(n)`.
</details>

### B8. Longest palindromic substring
`Pattern:` expand around center (2n-1 centers).
<details><summary>Approach</summary>

Har index ko odd-center aur har gap ko even-center maan ke bahar phailao jab tak
`s[l]==s[r]`. Track best `(l, len)`. `O(n²)` time, `O(1)` space. Manacher =
`O(n)` (solutions file).
</details>

### B9. Set matrix zeroes
Agar `m[i][j]==0`, poori row `i` aur column `j` zero. `O(1)` extra space.
`Pattern:` first row/col ko markers ki tarah use karo.
<details><summary>Approach</summary>

Row 0 aur col 0 mein flag karo ki us row/col ko zero karna hai; do separate bools
row0/col0 ke liye. Phir andar se bahar zero karo, aakhir mein row0/col0.
`O(mn)`/`O(1)`.
</details>

### B10. Spiral matrix
`m×n` matrix ko spiral order mein read karo.
`Pattern:` four shrinking bounds (top, bottom, left, right).
<details><summary>Approach</summary>

`top,bottom,left,right`; ek layer: top row L→R, right col T→B, (agar bacha) bottom
row R→L, left col B→T; bounds andar karo. `O(mn)`. Row/col count check har phase
pe (single row/col edge case).
</details>

### B11. Rotate image 90°
`n×n` matrix in-place clockwise 90°.
`Pattern:` transpose + reverse each row.
<details><summary>Approach</summary>

Transpose (`swap(m[i][j], m[j][i])` for `j>i`), phir har row reverse. `O(n²)`/
`O(1)`. Layer-by-layer 4-way swap bhi.
</details>

### B12. Subarray sum equals k
Kitne contiguous subarrays ka sum `== k` (negatives allowed).
`Pattern:` prefix sum + hash map of counts.
<details><summary>Approach</summary>

`count[prefix]`; running `pre`; `ans += count[pre - k]`; `++count[pre]`.
`count[0] = 1` initial. `O(n)`/`O(n)`. Sliding window yahan kaam nahi karta
(negatives).
</details>

### B13. Minimum window substring
`s` ka chhota substring jo `t` ke saare chars (multiplicity ke saath) contain
kare.
`Pattern:` variable window + need-count + `have` matched count.
<details><summary>Approach</summary>

`need[c]` from `t`; window expand karke `have` (fully-satisfied distinct chars)
badhao; `have == required` pe left se shrink karke min record karo. `O(|s| +
|t|)`.
</details>

### B14. Sliding window maximum
Har length-`k` window ka max, `O(n)`.
`Pattern:` monotonic decreasing deque of indices.
<details><summary>Approach</summary>

Naye `i` ke aane pe back se chhote values pop; front agar `i-k` se bahar → pop;
push `i`; front = window max. Har index ek baar in/out → `O(n)`.
</details>

### B15. Find all anagrams in a string
`s` mein `p` ke saare anagram start indices.
`Pattern:` fixed-size sliding window + count match.
<details><summary>Approach</summary>

`k = |p|` size ka window; `cnt[26]` diff array (`p` minus window); jab saare zero
→ match. Window slide pe do updates. `O(|s|)`.
</details>

### B16. Next permutation
Array ko lexicographically next permutation mein badlo, in-place (largest ho to
sorted ascending).
`Pattern:` find pivot from right, swap with next-larger, reverse suffix.
<details><summary>Approach</summary>

Right se pehla `a[i] < a[i+1]` (pivot). Right se pehla `a[j] > a[i]`, swap. `i+1..`
reverse. `O(n)`/`O(1)`. Pivot na mile → poora reverse.
</details>

---

## Part C — Hard (~40+ min)

### C1. Trapping rain water
`height[]` ke upar kitna paani ruke. `O(n)` time, `O(1)` space.
`Pattern:` two pointers + leftMax/rightMax.
<details><summary>Approach</summary>

`l, r` ends; `leftMax, rightMax`. Jis taraf ka max chhota hai wahi pointer move
karo, `water += max - height[i]`. Chhoti max side pe answer decided hai. `O(n)`/
`O(1)`. Monotonic stack bhi.
</details>

### C2. Median of two sorted arrays
`O(log(m+n))` — actually `O(log min(m,n))`.
`Pattern:` binary search the partition of the smaller array.
<details><summary>Approach</summary>

Smaller array ko `i` pe cut, bade ko `j = (m+n+1)/2 - i` pe. Valid jab `Aleft <=
Bright && Bleft <= Aright`. Median = boundary 4 values se (parity pe depend).
`O(log min(m,n))`.
</details>

### C3. First missing positive
Unsorted `a`; smallest missing positive integer. `O(n)` time, `O(1)` space.
`Pattern:` array itself as a hash — put `v` at index `v-1`.
<details><summary>Approach</summary>

`1..n` range ke `a[i]` ko `a[a[i]-1]` se swap karke jagah pe rakho (cyclic).
Phir pehla index `i` jahan `a[i] != i+1` → answer `i+1`; sab sahi → `n+1`.
`O(n)`/`O(1)`.
</details>

### C4. Largest rectangle in histogram
Bar heights; max axis-aligned rectangle.
`Pattern:` monotonic increasing stack of indices.
<details><summary>Approach</summary>

Stack heights badhte rakho; chhota bar aaya → pop karte jao, har pop pe `height =
h[popped]`, `width = i - stack.top() - 1`. Sentinel `0` end pe. `O(n)`.
</details>

### C5. Jump game II
`a[i]` = us index se max jump. Min jumps to reach last index (guarantee: reachable).
`Pattern:` greedy BFS "levels".
<details><summary>Approach</summary>

`curEnd` = current jump ki farthest reach; iterate, `farthest = max(farthest, i +
a[i])`; `i == curEnd → ++jumps, curEnd = farthest`. `O(n)`/`O(1)`.
</details>

### C6. Longest consecutive sequence
Unsorted `a`; longest run of consecutive integers (value-wise). `O(n)`.
`Pattern:` hash set, only start from sequence-heads.
<details><summary>Approach</summary>

Sab `unordered_set` mein. Har `x` ke liye agar `x-1` set mein nahi (yani `x` ek
run ka start) tabhi `x, x+1, x+2…` count karo. Har element ek baar visit → `O(n)`.
</details>

### C7. Text justification
Words ko `maxWidth` ki lines mein pack karo, spaces evenly distribute (left slots
zyada), last line left-justified.
`Pattern:` greedy line packing + space arithmetic.
<details><summary>Approach</summary>

Greedy: line mein words tab tak jab tak `sumLen + gaps <= maxWidth`. Spaces:
`total = maxWidth - sumLen`; `base = total / gaps`, `extra = total % gaps` (pehle
`extra` gaps ko `+1`). Single-word line aur last line → left-justify + pad right.
`O(Σ Lᵢ)`.
</details>

### C8. Sort colors (Dutch national flag)
`0/1/2` array ko ek pass, `O(1)` space mein sort karo.
`Pattern:` three pointers low/mid/high.
<details><summary>Approach</summary>

`lo, mid, hi`; `a[mid]==0 → swap(a[lo++], a[mid++])`; `==1 → ++mid`; `==2 →
swap(a[mid], a[hi--])` (`mid` mat badhao). `O(n)`/`O(1)`.
</details>

### C9. Maximum sum rectangle in 2D matrix
`m×n` (negatives allowed); max sum submatrix.
`Pattern:` fix top/bottom rows → Kadane on the column-sum array.
<details><summary>Approach</summary>

Har (top, bottom) row pair ke liye column sums ka 1D array banao (incrementally),
us pe Kadane. `O(m² · n)` time, `O(n)` space. `m <= n` ho aise arrange karo.
</details>

### C10. KMP substring search
`strstr` bina naive `O(nm)` ke — `O(n + m)`. Prefix-function khud likho.
`Pattern:` failure function (longest proper prefix = suffix).
<details><summary>Approach</summary>

`lps[]` for pattern: `O(m)`. Text scan: mismatch pe `j = lps[j-1]` (backtrack
sirf pattern mein). `O(n)`. `lps` construction ki recurrence hi asli insight hai.
Z-algorithm alternative.
</details>

---

## Next
→ [`03-pointers-memory-problems.md`](03-pointers-memory-problems.md)
