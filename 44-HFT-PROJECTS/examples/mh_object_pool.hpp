// mh_object_pool.hpp
// ============================================================
// PROJECT 6 -- Object pool with reuse + GENERATION-checked handles.
//
// FixedPool raw slots deta hai; yeh typed objects + safe handles deta.
// Handle = {slot index, generation}. release() pe generation bump hota
// -> ek recycled slot ka PURANA handle ab `get()` pe nullptr deta.
//
// Kyun zaroori (HFT): ek order slot recycle ho gaya, phir uske PURANE
// exchange-id pe ek late ack/fill aaya -> bina generation check ke woh
// GALAT (naye) order pe apply ho jaata. Generation = cheap use-after-free
// guard (25-object-model / handles + generation counters).
// ============================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace mhft {

template <class T>
class ObjectPool {
public:
    struct Handle {
        std::uint32_t idx = 0;
        std::uint32_t gen = 0;                 // 0 = null handle
        bool valid() const { return gen != 0; }
    };

    explicit ObjectPool(std::size_t reserve_n = 0) {
        slots_.reserve(reserve_n);
        free_.reserve(reserve_n);
    }

    template <class... Args>
    Handle acquire(Args&&... args) {
        std::uint32_t i;
        if (!free_.empty()) {
            i = free_.back();
            free_.pop_back();
        } else {
            i = static_cast<std::uint32_t>(slots_.size());
            slots_.push_back(Slot{});
        }
        Slot& s = slots_[i];
        s.obj = T(std::forward<Args>(args)...);
        if (s.gen == 0) s.gen = 1;             // never hand out gen 0 (null)
        s.live = true;
        ++live_;
        return Handle{i, s.gen};
    }

    T* get(Handle h) {
        if (!h.valid() || h.idx >= slots_.size()) return nullptr;
        Slot& s = slots_[h.idx];
        return (s.live && s.gen == h.gen) ? &s.obj : nullptr;
    }
    const T* get(Handle h) const {
        if (!h.valid() || h.idx >= slots_.size()) return nullptr;
        const Slot& s = slots_[h.idx];
        return (s.live && s.gen == h.gen) ? &s.obj : nullptr;
    }

    bool release(Handle h) {
        if (!h.valid() || h.idx >= slots_.size()) return false;
        Slot& s = slots_[h.idx];
        if (!s.live || s.gen != h.gen) return false;   // stale / double-release
        s.live = false;
        ++s.gen;                                       // invalidate every old handle
        if (s.gen == 0) s.gen = 1;                     // skip null on wrap
        s.obj = T{};
        free_.push_back(h.idx);
        --live_;
        return true;
    }

    std::size_t live()     const { return live_; }
    std::size_t capacity() const { return slots_.size(); }

private:
    struct Slot {
        T             obj{};
        std::uint32_t gen  = 0;
        bool          live = false;
    };
    std::vector<Slot>          slots_;
    std::vector<std::uint32_t> free_;
    std::size_t                live_ = 0;
};

}  // namespace mhft
