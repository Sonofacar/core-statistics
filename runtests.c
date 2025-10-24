#include "core.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>
 
// detect_type
static void test_detect_type_double(void **state)
{
	(void) state;
	assert_int_equal(detect_type("12345"), TYPE_DOUBLE);
	assert_int_equal(detect_type("12.45"), TYPE_DOUBLE);
	assert_int_equal(detect_type("-2345"), TYPE_DOUBLE);
	assert_int_equal(detect_type("  345"), TYPE_DOUBLE);
}
 
static void test_detect_type_string(void **state)
{
	(void) state;
	assert_int_equal(detect_type("abc"), TYPE_STRING);
	assert_int_equal(detect_type("1.3.5"), TYPE_STRING);
	assert_int_equal(detect_type("--345"), TYPE_STRING);
	assert_int_equal(detect_type(""), TYPE_STRING);
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_detect_type_double),
		cmocka_unit_test(test_detect_type_string),
	};
 
	return cmocka_run_group_tests(tests, NULL, NULL);
}
