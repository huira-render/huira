// Built into every test in a coverage build (see add_unit_test() in CMakeLists.txt): it limits TBB
// to one thread, the test's own, from before main() until the test exits.
//
// gcov's counters are shared by every thread that runs the code they count, so threads that render
// side by side contend for them, and an increment two threads make at once can be lost, which
// lcov 2 reports as a negative count. On one thread the counts are exact and the tests are faster;
// the coverage target runs as many tests at once as there are cores instead.

#include "tbb/global_control.h"

namespace {

const tbb::global_control one_thread(tbb::global_control::max_allowed_parallelism, 1);

} // namespace
