#include "core.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

// dataColumn functions
static void test_column_alloc_not_null(void **state)
{
	(void) state;
	will_return_always(__wrap_malloc, false);
	will_return_maybe(__wrap_gsl_vector_alloc, false);
	dataColumn * data = column_alloc(0, "");
	assert_non_null(data);
	column_free(data);
}

static void test_column_alloc_vector_size(void **state)
{
	(void) state;
	int n = 10;
	will_return_always(__wrap_malloc, false);
	will_return_maybe(__wrap_gsl_vector_alloc, false);
	dataColumn * data = column_alloc(n, "");
	assert_int_equal(n, data->vector->size);
}

static void test_column_alloc_null_values(void **state)
{
	(void) state;
	will_return_always(__wrap_malloc, false);
	will_return_maybe(__wrap_gsl_vector_alloc, false);
	dataColumn * data = column_alloc(0, "");
	assert_null(data->to_encode);
	assert_null(data->nextColumn);
	column_free(data);
}

void * __real_malloc(size_t size);
void * __wrap_malloc(size_t size)
{
	// Cause allocation failure
	if (mock()) {
		return NULL;
	}

	return __real_malloc(size);
}

void * __real_gsl_vector_alloc(size_t n);
void * __wrap_gsl_vector_alloc(size_t n)
{
	// Cause allocation failure
	if (mock()) {
		return NULL;
	}

	return __real_gsl_vector_alloc(n);
}

static void test_column_alloc_oom(void **state)
{
	(void) state;
	will_return_always(__wrap_malloc, true);
	will_return_maybe(__wrap_gsl_vector_alloc, true);
	dataColumn * data = column_alloc(0, "");
	assert_null(data);
}
 
// detect_type
static void test_detect_type_double(void **state)
{
	(void) state;
	assert_int_equal(detect_type("12345"), TYPE_DOUBLE);
	assert_int_equal(detect_type("12.45"), TYPE_DOUBLE);
	assert_int_equal(detect_type("-2345"), TYPE_DOUBLE);
	assert_int_equal(detect_type("  345"), TYPE_DOUBLE);
	assert_int_equal(detect_type(" +345"), TYPE_DOUBLE);
}
 
static void test_detect_type_string(void **state)
{
	(void) state;
	assert_int_equal(detect_type("abc"), TYPE_STRING);
	assert_int_equal(detect_type("1.3.5"), TYPE_STRING);
	assert_int_equal(detect_type("--345"), TYPE_STRING);
	assert_int_equal(detect_type(""), TYPE_STRING);
	assert_int_equal(detect_type(" - "), TYPE_STRING);
	assert_int_equal(detect_type(" + "), TYPE_STRING);
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_column_alloc_not_null),
		cmocka_unit_test(test_column_alloc_vector_size),
		cmocka_unit_test(test_column_alloc_null_values),
		cmocka_unit_test(test_column_alloc_oom),
		cmocka_unit_test(test_detect_type_double),
		cmocka_unit_test(test_detect_type_string),
	};
 
	return cmocka_run_group_tests(tests, NULL, NULL);
}
