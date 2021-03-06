#include "predef.h"

#ifdef WZ_MSVC
#  pragma warning(push, 3)
#endif

#include "check_fix.h"

#ifdef WZ_MSVC
#  pragma warning(pop)
#endif

#include "test_file.h"

int
main(int argc, char ** argv) {
  Suite * suite;
  SRunner * runner;
  int failed;
  (void) argc;
  (void) argv;
  suite = suite_create("suite");
  suite_add_tcase(suite, create_tcase_file());
  runner = srunner_create(suite);
  srunner_run_all(runner, CK_NORMAL);
  failed = srunner_ntests_failed(runner);
  srunner_free(runner);
  return failed;
}
