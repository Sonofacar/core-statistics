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

// encode
int __wrap_dummy_encode(dataColumn * data, int nrow)
{
	function_called();
}

void __wrap_target_encode(dataColumn * data, gsl_vector * response, int nrow)
{
	function_called();
}

static void test_encode_call_dummy(void **state)
{
	dataColumn * data;
	data->to_encode;
	gsl_vector * response;
	int nrow;

	expect_function_call(target_encode);
	encode(data, response, nrow, ENCODE_DUMMY);
}

static void test_encode_call_target(void **state)
{
	dataColumn * data;
	gsl_vector * response;
	int nrow;

	expect_function_call(target_encode);
	encode(data, response, nrow, ENCODE_TARGET);
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_detect_type_double),
		cmocka_unit_test(test_detect_type_string),
		cmocka_unit_test(test_encode_call_dummy),
		cmocka_unit_test(test_encode_call_target),
	};
 
	return cmocka_run_group_tests(tests, NULL, NULL);
}
