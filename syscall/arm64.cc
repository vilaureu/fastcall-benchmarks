/* Measure the latency of steps in the system call execution on arm64 */

#ifdef __aarch64__

#include "compiler.hpp"
#include "os.hpp"
#include "syscall.hpp"

#include <array>
#include <iomanip>
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>

#define SETW std::setw(12)
#define CETW ',' << std::setw(12)

typedef std::array<std::uint64_t, 13> Measurements;

static constexpr std::size_t SVC = 2;
static constexpr std::size_t TRAMP_EXIT = 11;

/* Read pmccntr_el0, optionally serialized. */
static INLINE size_t pmccntr() {
  size_t counter;
  asm volatile(
#ifdef SERIALIZE
      "isb;"
#endif
      "mrs %0, pmccntr_el0;"
#ifdef SERIALIZE
      "isb;"
#endif
      : "=r"(counter)
      :
      : "memory");
  return counter;
}

static Measurements measure() {
  Measurements measurements{};
  if (mlock(&measurements, sizeof(measurements)))
    perror("mlock failed");

  auto ptr = measurements.data();
  ptr[0] = pmccntr();

  // Measure overhead of successive pmccntr() invocations
  ptr[1] = pmccntr();

  if (syscall(SYS_BENCH, &ptr[2]))
    perror("system call failed");

  measurements.back() = pmccntr();

  // If KPTI is disabled, set latency of skipped probes to zero.
  if (measurements[SVC] == 0) {
    measurements[SVC] = measurements[SVC + 1];
  }
  if (measurements[TRAMP_EXIT] == 0) {
    measurements[TRAMP_EXIT] = measurements[TRAMP_EXIT - 1];
  }

  std::uint64_t start = measurements[0];
  for (auto &cycles : measurements) {
    cycles = cycles - start;
  }

  return measurements;
}

int main() {
  os::assert_kernel(os::RELEASE_SYSCALL_BENCH);
  os::fix_cpu();

  std::cout << SETW << "start" << CETW << "overhead" << CETW << "svc" << CETW
            << "tramp_entry" << CETW << "sp_overflow" << CETW << "entry" << CETW
            << "el0_svc" << CETW << "func_entry" << CETW << "func_exit" << CETW
            << "finish" << CETW << "restore" << CETW << "tramp_exit" << CETW
            << "eret" << std::endl;
  for (std::size_t i = 0; i < ITERATIONS; i++) {
    Measurements measurements = measure();

    bool first = true;
    for (auto const &cycles : measurements) {
      if (first)
        first = false;
      else
        std::cout << ',';
      std::cout << SETW << cycles;
    }
    std::cout << std::endl;
  }

  return 0;
}

#endif /* __aarch64__ */
