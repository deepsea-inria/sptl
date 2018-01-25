
#include "spreduce.hpp"
#include "spio.hpp"

namespace sptl {

double ddotprod(size_t n, const double* row, const double* vec) {
  return level1::reducei(vec, vec + n, 0.0, [&] (double x, double y) {
    return x + y;
  }, [&] (size_t i, const double&) {
    return row[i] * vec[i];
  });
}

parray<double> dmdvmult1(const parray<double>& mtx, const parray<double>& vec) {
  size_t n = vec.size();
  parray<double> result(n);
  auto comp_rng = [&] (size_t lo, size_t hi) {
    return (hi - lo) * n;
  };
  parallel_for((size_t)0, n, comp_rng, [&] (size_t i) {
    result[i] = ddotprod(n, mtx.cbegin()+(i * n), vec.begin());
  });
  return result;
}

parray<double> dmdvmult2(const parray<double>& mtx, const parray<double>& vec) {
  size_t n = vec.size();
  parray<double> result(n);
  auto comp_rng = [&] (size_t lo, size_t hi) {
    return (hi - lo) * n;
  };
  parallel_for((size_t)0, n, comp_rng, [&] (size_t i) {
    result[i] = ddotprod(n, mtx.cbegin() + (i * n), vec.begin());
  }, [&] (size_t lo, size_t hi) {
    for (size_t i = lo; i < hi; i++) {
      double dotp = 0.0;
      for (size_t j = 0; j < n; j++) {
        dotp += mtx[i * n + j] * vec[j];
      }
      result[i] = dotp;
    }
  });
  return result;
}

void ex() {

  parray<double> mtx = { 1.1, 2.1, 0.3, 5.8,
                         8.1, 9.3, 3.1, 3.2,
                         5.3, 3.5, 7.9, 2.3,
                         4.5, 5.5, 3.4, 4.5 };

  parray<double> vec = { 4.3, 0.3, 2.1, 3.3 };

  {
    parray<double> result = dmdvmult1(mtx, vec);
    std::cout << "result = " << result << std::endl;
  }

  {
    parray<double> result = dmdvmult2(mtx, vec);
    std::cout << "result = " << result << std::endl;
  }

}
  
} // end namespace

int main(int argc, char** argv) {
  sptl::launch(argc, argv, [&] {
    sptl::ex();
  });
  return 0;
}
