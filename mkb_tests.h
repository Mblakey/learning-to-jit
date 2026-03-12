
#ifndef MKB_TESTS_H
#define MKB_TESTS_H

//static const char *test_case = "x = 2; y = 3; z = x + y + 2; meg(10) { i = 2; x = 3; };";
static const char *test_case = "x = 0; meg(1000) { meg(1000) { x = x + 1; }; }; rachael(x);";


#endif
