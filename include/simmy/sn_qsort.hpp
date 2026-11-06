// The game's qsort (SN libc, sn/libc/qsort.obj 80242918): 4.4BSD's Bentley-McIlroy quicksort (insertion
// sort below 7 elements, median of three, or of nine above 40, the equal keys swapped to the ends, back
// to insertion sort when a pass swaps nothing). It is not stable, so a comparator with ties (or one that
// is not a strict order, like _MotiveSort's 0.01 tolerance) gives an order only this algorithm
// reproduces. PC: for simulation code that sorts with ties, so the order matches the GameCube.
#pragma once
#include <algorithm>
#include <cstddef>
#include <utility>

namespace simmy {

namespace sn_qsort_detail {
template <typename T, typename Cmp>
T* Med3(T* a, T* b, T* c, Cmp& cmp) {
  return cmp(*a, *b) < 0 ? (cmp(*b, *c) < 0 ? b : (cmp(*a, *c) < 0 ? c : a))
                         : (cmp(*b, *c) > 0 ? b : (cmp(*a, *c) < 0 ? a : c));
}
template <typename T>
void VecSwap(T* a, T* b, std::ptrdiff_t n) {
  for (; n > 0; --n) std::swap(*a++, *b++);
}
}  // namespace sn_qsort_detail

// `cmp(a, b)` returns <0, 0 or >0 as qsort's comparator does.
template <typename T, typename Cmp>
void SnQsort(T* a, std::size_t n, Cmp cmp) {
  using namespace sn_qsort_detail;
  for (;;) {
    bool swapped = false;
    if (n < 7) {
      for (T* pm = a + 1; pm < a + n; ++pm)
        for (T* pl = pm; pl > a && cmp(*(pl - 1), *pl) > 0; --pl) std::swap(*pl, *(pl - 1));
      return;
    }
    T* pm = a + n / 2;
    if (n > 7) {
      T* pl = a;
      T* pn = a + (n - 1);
      if (n > 40) {
        const std::size_t d = n / 8;
        pl = Med3(pl, pl + d, pl + 2 * d, cmp);
        pm = Med3(pm - d, pm, pm + d, cmp);
        pn = Med3(pn - 2 * d, pn - d, pn, cmp);
      }
      pm = Med3(pl, pm, pn, cmp);
    }
    std::swap(*a, *pm);
    T *pa = a + 1, *pb = a + 1;
    T *pc = a + (n - 1), *pd = a + (n - 1);
    for (;;) {
      int r;
      while (pb <= pc && (r = cmp(*pb, *a)) <= 0) {
        if (r == 0) {
          swapped = true;
          std::swap(*pa, *pb);
          ++pa;
        }
        ++pb;
      }
      while (pb <= pc && (r = cmp(*pc, *a)) >= 0) {
        if (r == 0) {
          swapped = true;
          std::swap(*pc, *pd);
          --pd;
        }
        --pc;
      }
      if (pb > pc) break;
      std::swap(*pb, *pc);
      swapped = true;
      ++pb;
      --pc;
    }
    if (!swapped) {
      for (T* p = a + 1; p < a + n; ++p)
        for (T* pl = p; pl > a && cmp(*(pl - 1), *pl) > 0; --pl) std::swap(*pl, *(pl - 1));
      return;
    }
    T* pn = a + n;
    std::ptrdiff_t r = std::min(pa - a, pb - pa);
    VecSwap(a, pb - r, r);
    r = std::min(pd - pc, pn - pd - 1);
    VecSwap(pb, pn - r, r);
    if ((r = pb - pa) > 1) SnQsort(a, static_cast<std::size_t>(r), cmp);
    if ((r = pd - pc) > 1) {
      a = pn - r;
      n = static_cast<std::size_t>(r);
      continue;
    }
    return;
  }
}

}  // namespace simmy
