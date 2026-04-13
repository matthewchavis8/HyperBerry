#include "tests/integration/suite.h"
#include "core/mm/pmm/pmm.h"

static bool test_alloc_order0_succeeds() {
  uint64_t addr = pmm::allocPages(0);
  if (addr == 0)
    return false;

  pmm::freePages(addr, 0);
  return true;
}

static bool test_alloc_page_aligned() {
  uint64_t addr = pmm::allocPages(0);
  if (addr == 0)
    return false;

  bool aligned = (addr & (PAGE_SIZE - 1)) == 0;
  pmm::freePages(addr, 0);
  return aligned;
}

static bool test_free_and_realloc() {
  uint64_t a = pmm::allocPages(0);
  if (a == 0)
    return false;

  pmm::freePages(a, 0);
  uint64_t b = pmm::allocPages(0);
  if (b == 0)
    return false;

  pmm::freePages(b, 0);
  return true;
}

static bool test_alloc_too_large_fails() {
  uint64_t addr = pmm::allocPages(MAX_ORDER + 1);
  return addr == 0;
}

static bool test_alloc_two_different() {
  uint64_t a = pmm::allocPages(0);
  uint64_t b = pmm::allocPages(0);
  if (a == 0 || b == 0)
    return false;

  bool different = (a != b);
  pmm::freePages(a, 0);
  pmm::freePages(b, 0);
  return different;
}

static const TestCase buddy_cases[] = {
    {"alloc_order0_succeeds\n",   test_alloc_order0_succeeds},
    {"alloc_page_aligned\n",      test_alloc_page_aligned},
    {"free_and_realloc\n",        test_free_and_realloc},
    {"alloc_too_large_fails\n",   test_alloc_too_large_fails},
    {"alloc_two_different\n",     test_alloc_two_different},
};

static const TestSuite buddySuite = {
    "pmm",
    buddy_cases,
    5,
};

REGISTER_SUITE(buddySuite);
