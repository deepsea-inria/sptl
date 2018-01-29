
#include "spreduce.hpp"
#include "spio.hpp"

namespace sptl {

int max(const parray<int>& xs) {
  int id = std::numeric_limits<int>::lowest();
  return reduce(xs.cbegin(), xs.cend(), id, [&] (int x, int y) {
    return std::max(x, y);
  });
}

int max0(const parray<parray<int>>& xss) {
  parray<int> id = { std::numeric_limits<int>::lowest() };
  parray<size_t> ps =
    sums(xss.size(), [&] (size_t i) {
      return std::max((size_t)1, xss[i].size());
    });
  auto combine_comp_rng = [&] (const parray<int>* lo, const parray<int>* hi) {
    return ps[lo - xss.cbegin()] + ps[hi - xss.cbegin()];
  };
  auto combine = [&] (const parray<int>& xs1,
                      const parray<int>& xs2) {
    parray<int> r = { std::max(max(xs1), max(xs2)) };
    return r;
  };
  parray<int> a =
    reduce(xss.cbegin(), xss.cend(), id, combine_comp_rng, combine);
  return a[0];
}
  
int max1(const parray<parray<int>>& xss) {
  auto lo = xss.cbegin();
  auto hi = xss.cend();
  int id = std::numeric_limits<int>::lowest();
  auto combine = [&] (int x, int y) {
    return std::max(x, y);
  };
  parray<size_t> ps =
    sums(xss.size(), [&] (size_t i) {
      return std::max((size_t)1, xss[i].size());
    });
  auto lift_comp_rng = [&] (const parray<int>* lo, const parray<int>* hi) {
    return ps[lo - xss.cbegin()] + ps[hi - xss.cbegin()];
  };
  auto lift = [&] (const parray<int>& xs) {
    return max(xs);
  };
  return level1::reduce(lo, hi, id, combine, lift_comp_rng, lift);
}

int max2(const parray<parray<int>>& xss) {
  using const_iterator = typename parray<parray<int>>::const_iterator;
  const_iterator lo = xss.cbegin();
  const_iterator hi = xss.cend();
  int id = std::numeric_limits<int>::lowest();
  auto combine = [&] (int x, int y) {
    return std::max(x, y);
  };
  parray<size_t> ps =
    sums(xss.size(), [&] (size_t i) {
      return std::max((size_t)1, xss[i].size());
    });
  auto lift_comp_rng = [&] (const parray<int>* lo, const parray<int>* hi) {
    return ps[lo - xss.cbegin()] + ps[hi - xss.cbegin()];
  };
  auto lift_idx = [&] (int, const parray<int>& xs) {
    return max(xs);
  };
  auto seq_reduce_rng = [&] (const_iterator lo, const_iterator hi) {
    int m = id;
    for (const_iterator i = lo; i != hi; i++) {
      for (auto j = i->cbegin(); j != i->cend(); j++) {
        m = std::max(m, *j);
      }
    }
    return m;
  };
  return level2::reduce(lo, hi, id, combine, lift_comp_rng,
                                    lift_idx, seq_reduce_rng);
}

int max3(const parray<parray<int>>& xss) {
  using const_iterator = typename parray<parray<int>>::const_iterator;
  const_iterator lo = xss.cbegin();
  const_iterator hi = xss.cend();
  int id = std::numeric_limits<int>::lowest();
  auto combine = [&] (int x, int y) {
    return std::max(x, y);
  };
  using output_type = level3::cell_output<int, decltype(combine)>;
  output_type out(id, combine);
  parray<size_t> ps =
    sums(xss.size(), [&] (size_t i) {
      return std::max((size_t)1, xss[i].size());
    });
  auto lift_comp_rng = [&] (const parray<int>* lo, const parray<int>* hi) {
    return ps[lo - xss.cbegin()] + ps[hi - xss.cbegin()];
  };
  auto lift_idx_dst = [&] (int, const parray<int>& xs, int& result) {
    result = max(xs);
  };
  auto seq_reduce_rng = [&] (const_iterator lo, const_iterator hi, int& result) {
    int m = id;
    for (const_iterator i = lo; i != hi; i++) {
      for (auto j = i->cbegin(); j != i->cend(); j++) {
        m = std::max(m, *j);
      }
    }
    result = m;
  };
  int result;
  level3::reduce(lo, hi, out, id, result, lift_comp_rng,
                 lift_idx_dst, seq_reduce_rng);
  return result;
}

void ex() {
  
  {
    parray<int> xs = { 1, 3, 9, 0, 33, 1, 1 };
    std::cout << "xs\t\t= " << xs << std::endl;
    std::cout << "max(xs)\t= " << max(xs) << std::endl;
  }

  parray<parray<int>> xss = { {23, 1, 3}, { 19, 3, 3 }, { 100 } };
  
  {
    std::cout << "xss\t\t= " << xss << std::endl;
    std::cout << "max0(xss)\t= " << max0(xss) << std::endl;
  }

  {
    std::cout << "xss\t\t= " << xss << std::endl;
    std::cout << "max1(xss)\t= " << max1(xss) << std::endl;
  }

  {
    std::cout << "xss\t\t= " << xss << std::endl;
    std::cout << "max2(xss)\t= " << max2(xss) << std::endl;
  }

  {
    std::cout << "xss\t\t= " << xss << std::endl;
    std::cout << "max3(xss)\t= " << max3(xss) << std::endl;
  }

}
  
} // end namespace

int main(int argc, char** argv) {
  sptl::launch(argc, argv, [&] {
    sptl::ex();
  });
  return 0;
}
