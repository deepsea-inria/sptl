
#include <math.h>

#include "spreduce.hpp"
#include "spio.hpp"

namespace sptl {

long seq_fib(long n){
  if (n < 2) {
    return n;
  } else { 
    return seq_fib (n - 1) + seq_fib (n - 2);
  }
}

double phi = (1 + sqrt(5)) / 2;

long par_fib(long n) {
  long result;
  spguard([&] { return pow(phi, n); }, [&] {
    if (n < 2) {
      result = seq_fib(n);
      return;
    }
    long a, b;
    fork2([n, &a] { a = par_fib(n-1); },
          [n, &b] { b = par_fib(n-2); });
    result = a + b;
  }, [&] {
    result = seq_fib(n);
  });
  return result;
}

void ex() {
  long n = deepsea::cmdline::parse_or_default_int("n", 10);
  long r = par_fib(n);
  std::cout << "result\t" << r << std::endl;
}

} // end namespace

int main(int argc, char** argv) {
  sptl::launch(argc, argv, [&] {
    sptl::ex();
  });
  return 0;
}
