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
	ignore_function_calls(__wrap_free);
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
	ignore_function_calls(__wrap_free);
	column_free(data);
}

static void test_column_alloc_null_values(void **state)
{
	(void) state;
	will_return_always(__wrap_malloc, false);
	will_return_maybe(__wrap_gsl_vector_alloc, false);
	dataColumn * data = column_alloc(0, "");
	assert_null(data->to_encode);
	assert_null(data->nextColumn);
	ignore_function_calls(__wrap_free);
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

void __real_free(void * p);
void __wrap_free(void * p)
{
	function_called();
	__real_free(p);
}

static void test_column_free(void **state)
{
	(void) state;
	will_return_always(__wrap_malloc, false);
	will_return_maybe(__wrap_gsl_vector_alloc, false);
	dataColumn * data = column_alloc(0, "");
	expect_function_calls(__wrap_free, 2);
	column_free(data);
}

static void test_column_free_recursive(void **state)
{
	(void) state;
	int n = 5;
	will_return_always(__wrap_malloc, false);
	will_return_maybe(__wrap_gsl_vector_alloc, false);
	dataColumn * data = column_alloc(0, "");
	dataColumn * head = data;
	for (int i = 1; i < n; i++) {
		head->nextColumn = column_alloc(0, "");
		head = head->nextColumn;
	}
	expect_function_calls(__wrap_free, 2 * n);
	column_free(data);
}

static void test_column_free_large_recursive(void **state)
{
	(void) state;
	int p = 50000;
	int n = 100000;
	will_return_always(__wrap_malloc, false);
	will_return_maybe(__wrap_gsl_vector_alloc, false);
	dataColumn * data = column_alloc(p, "");
	dataColumn * head = data;
	for (int i = 1; i < n; i++) {
		head->nextColumn = column_alloc(p, "");
		head = head->nextColumn;
	}
	ignore_function_calls(__wrap_free);
	column_free(data);
}
 
static void test_column_free_null(void **state)
{
	(void) state;
	ignore_function_calls(__wrap_free);
	column_free(NULL);
}

static void test_column_free_partial_null(void **state)
{
	(void) state;
	// name
	will_return_always(__wrap_malloc, false);
	will_return_maybe(__wrap_gsl_vector_alloc, false);
	dataColumn * data = column_alloc(0, "");
	__real_free(data->name);
	data->name = NULL;
	expect_function_calls(__wrap_free, 1);
	column_free(data);

	// vector
	data = column_alloc(0, "");
	__real_free(data->vector);
	data->vector = NULL;
	expect_function_calls(__wrap_free, 2);
	column_free(data);
}

// detect_type
static void test_detect_type_double(void **state)
{
	(void) state;
	assert_int_equal(detect_type("12345"), TYPE_DOUBLE);
	assert_int_equal(detect_type("12.45"), TYPE_DOUBLE);
	assert_int_equal(detect_type("-2345"), TYPE_DOUBLE);
	assert_int_equal(detect_type(" -34 "), TYPE_DOUBLE);
	assert_int_equal(detect_type("  34 "), TYPE_DOUBLE);
	assert_int_equal(detect_type("+2345"), TYPE_DOUBLE);
	assert_int_equal(detect_type(" +34 "), TYPE_DOUBLE);
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
		cmocka_unit_test(test_column_free),
		cmocka_unit_test(test_column_free_recursive),
		cmocka_unit_test(test_column_free_large_recursive),
		cmocka_unit_test(test_column_free_null),
		cmocka_unit_test(test_column_free_partial_null),
		cmocka_unit_test(test_detect_type_double),
		cmocka_unit_test(test_detect_type_string),
	};
 
	return cmocka_run_group_tests(tests, NULL, NULL);
}
