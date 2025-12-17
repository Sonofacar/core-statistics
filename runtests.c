#include "core.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

// Test memory allocation
#define UNIT_TESTING 1
#define ALLOCATION_TESTING 1

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

// translate_row_value
static void test_translate_row_value_type_setting(void ** state)
{
	(void) state;
	// TYPE_DOUBLE
	int n = 1;
	will_return_always(__wrap_malloc, false);
	will_return_maybe(__wrap_gsl_vector_alloc, false);
	ignore_function_calls(__wrap_free);
	rowValue * row = __real_malloc(sizeof(rowValue));
	row->value = "1";
	row->nextValue = NULL;
	dataColumn * data = column_alloc(n, "");
	translate_row_value(row, data, n);
	assert_int_equal(data->type, TYPE_DOUBLE);

	// TYPE_STRING
	row->value = "a";
	data->nextColumn = column_alloc(n, "");
	translate_row_value(row, data->nextColumn, n);
	assert_int_equal(data->nextColumn->type, TYPE_STRING);

	column_free(data);
	__real_free(row);
}

static void test_translate_row_value_vector_update(void ** state)
{
	(void) state;
	int n = 1;
	will_return_always(__wrap_malloc, false);
	will_return_maybe(__wrap_gsl_vector_alloc, false);
	ignore_function_calls(__wrap_free);
	rowValue * row = __real_malloc(sizeof(rowValue));
	row->value = "123";
	row->nextValue = NULL;
	dataColumn * data = column_alloc(n, "");
	translate_row_value(row, data, n);

	assert_int_equal((int) gsl_vector_get(data->vector, 0), 123);

	column_free(data);
	__real_free(row);
}

static void test_translate_row_value_to_encode_update(void ** state)
{
	(void) state;
	int n = 1;
	will_return_always(__wrap_malloc, false);
	will_return_maybe(__wrap_gsl_vector_alloc, false);
	ignore_function_calls(__wrap_free);
	rowValue * row = __real_malloc(sizeof(rowValue));
	row->value = "a";
	row->nextValue = NULL;
	dataColumn * data = column_alloc(n, "");
	translate_row_value(row, data, n);

	assert_string_equal(data->to_encode[0], "a");

	column_free(data);
	__real_free(row);
}

// process_row
static void test_process_row_initialize_columns(void ** state)
{
	(void) state;
	int n = 1;
	char * line = strdup("a,b,c");
	will_return_always(__wrap_malloc, false);
	will_return_maybe(__wrap_gsl_vector_alloc, false);
	ignore_function_calls(__wrap_free);
	dataColumn * data = column_alloc(n, "");
	dataColumn * p = data;

	// The line results in 4 columns including the intercept
	process_row(data, n, 0, line, true);
	for (int i = 0; i < 4; i++) {
		assert_non_null(p);
		p = p->nextColumn;
	}
	assert_null(p);

	column_free(data);
}

static void test_process_row_return_n_columns(void ** state)
{
	(void) state;
	int n = 1;
	char * line = strdup("a,b,c");
	will_return_always(__wrap_malloc, false);
	will_return_maybe(__wrap_gsl_vector_alloc, false);
	ignore_function_calls(__wrap_free);
	dataColumn * data = column_alloc(n, "");

	assert_int_equal(process_row(data, n, 0, line, true), 3);

	column_free(data);
}

static void test_process_row_null_input(void ** state)
{
	(void) state;
	int n = 1;
	char * line = strdup("");
	will_return_always(__wrap_malloc, false);
	will_return_maybe(__wrap_gsl_vector_alloc, false);
	ignore_function_calls(__wrap_free);
	dataColumn * data = column_alloc(n, "");

	assert_int_equal(process_row(data, n, 0, line, true), 1);

	column_free(data);
}

static void test_process_row_insert_intercept(void ** state)
{
	(void) state;
	int n = 1;
	int i;
	char * line = strdup("a,b,c");
	will_return_always(__wrap_malloc, false);
	will_return_maybe(__wrap_gsl_vector_alloc, false);
	ignore_function_calls(__wrap_free);
	dataColumn * data = column_alloc(n, "");

	// Check intercept name
	process_row(data, n, 0, line, true);
	assert_string_equal(data->nextColumn->name, "intercept");

	// Check intercept value
	line = strdup("3,3,3");
	process_row(data, n, 1, line, false);
	i = gsl_vector_get(data->nextColumn->vector, 0);
	assert_int_equal(i, 1);

	column_free(data);
}

// read_rows
static void test_read_rows_number(void ** state)
{
	(void) state;
	int nrow;
	char ** lines = NULL;
        char * input_str = "a,b,c\n1,2,3\n4,5,6\n7,8,9\n";
        FILE * input = fmemopen(input_str, strlen(input_str), "r");

	ignore_function_calls(__wrap_free);
	nrow = read_rows(&lines, input);
	assert_int_equal(nrow, 3); // outputs number of data rows
	free(lines);
	fclose(input);
}

static void test_read_rows_null_input(void ** state)
{
	(void) state;
	int nrow;
	char ** lines = NULL;
        char * input_str = "";
        FILE * input = fmemopen(input_str, strlen(input_str), "r");

	ignore_function_calls(__wrap_free);
	nrow = read_rows(&lines, input);
	assert_int_equal(nrow, 0);
	free(lines);
	fclose(input);
}

static void test_read_rows_carriage_return(void ** state)
{
	(void) state;
	int nrow;
	char ** lines = NULL;
        char * input_str = "a,b,c\r\n1,2,3\r\n4,5,6\r\n7,8,9\r\n";
        FILE * input = fmemopen(input_str, strlen(input_str), "r");

	ignore_function_calls(__wrap_free);
	nrow = read_rows(&lines, input);
	assert_int_equal(nrow, 3);
	assert_string_not_equal(*lines + strlen(*lines) - 1, "\r");
	free(lines);
	fclose(input);
}

// read_columns
static void test_read_columns_column_number(void ** state)
{
	(void) state;
	int nrow, ncol;
	char ** lines = NULL;
        char * input_str = "a,b,c\n1,2,3\n4,5,6\n7,8,9\n";
        FILE * input = fmemopen(input_str, strlen(input_str), "r");

	ignore_function_calls(__wrap_free);
	will_return_always(__wrap_malloc, false);
	will_return_maybe(__wrap_gsl_vector_alloc, false);
	nrow = read_rows(&lines, input);
	dataColumn * columnHead = column_alloc(nrow, "");
	ncol = read_columns(columnHead, lines, NULL, nrow);
	assert_int_equal(ncol, 3);

	column_free(columnHead);
	free(lines);
	fclose(input);
}

static void test_read_columns_error_return(void ** state)
{
	(void) state;
	int nrow, ncol;
	char ** lines = NULL;
        char * input_str = "a,b,c\n1,2\n";
        FILE * input = fmemopen(input_str, strlen(input_str), "r");

	// Test with too few columns
	ignore_function_calls(__wrap_free);
	will_return_always(__wrap_malloc, false);
	will_return_maybe(__wrap_gsl_vector_alloc, false);
	nrow = read_rows(&lines, input);
	dataColumn * columnHead = column_alloc(nrow, "");
	ncol = read_columns(columnHead, lines, NULL, nrow);
	assert_int_equal(ncol, -1);

	column_free(columnHead);
	free(lines);
	fclose(input);

	// Test with too many columns
        input_str = "a,b,c\n1,2,3,4\n";
        input = fmemopen(input_str, strlen(input_str), "r");

	nrow = read_rows(&lines, input);
	columnHead = column_alloc(nrow, "");
	ncol = read_columns(columnHead, lines, NULL, nrow);
	assert_int_equal(ncol, -1);

	column_free(columnHead);
	free(lines);
	fclose(input);
}

int fake_encode(dataColumn * data, gsl_vector * response, int nrow)
{
	(void) data;
	(void) response;
	(void) nrow;
	function_called();
	return 0;
}

static void test_read_columns_call_encode(void ** state)
{
	(void) state;
	int nrow, ncol;
	char ** lines = NULL;
        char * input_str = "a,b,c\nd,e,f\n";
        FILE * input = fmemopen(input_str, strlen(input_str), "r");

	// Test with too few columns
	ignore_function_calls(__wrap_free);
	will_return_always(__wrap_malloc, false);
	will_return_maybe(__wrap_gsl_vector_alloc, false);
	nrow = read_rows(&lines, input);
	dataColumn * columnHead = column_alloc(nrow, "");

	expect_function_calls(fake_encode, 3);
	ncol = read_columns(columnHead, lines, fake_encode, nrow);
	(void) ncol;

	column_free(columnHead);
	free(lines);
	fclose(input);
}

// log_offset and exp_offset
static void test_log_offset_transform(void ** state)
{
	(void) state;
	double a, b = 0;
	b = exp_offset(log_offset(b));
	assert_int_equal(a, b);
}

int main(void) {
	const struct CMUnitTest column_alloc_test[] = {
		cmocka_unit_test(test_column_alloc_not_null),
		cmocka_unit_test(test_column_alloc_vector_size),
		cmocka_unit_test(test_column_alloc_null_values),
		cmocka_unit_test(test_column_alloc_oom),
	};
	const struct CMUnitTest column_free_test[] = {
		cmocka_unit_test(test_column_free),
		cmocka_unit_test(test_column_free_recursive),
		cmocka_unit_test(test_column_free_large_recursive),
		cmocka_unit_test(test_column_free_null),
		cmocka_unit_test(test_column_free_partial_null),
	};
	const struct CMUnitTest detect_type_test[] = {
		cmocka_unit_test(test_detect_type_double),
		cmocka_unit_test(test_detect_type_string),
	};
	const struct CMUnitTest translate_row_value_test[] = {
		cmocka_unit_test(test_translate_row_value_type_setting),
		cmocka_unit_test(test_translate_row_value_vector_update),
		cmocka_unit_test(test_translate_row_value_to_encode_update),
	};
	const struct CMUnitTest process_row_test[] = {
		cmocka_unit_test(test_process_row_initialize_columns),
		cmocka_unit_test(test_process_row_return_n_columns),
		cmocka_unit_test(test_process_row_null_input),
		cmocka_unit_test(test_process_row_insert_intercept),
	};
	const struct CMUnitTest read_rows_test[] = {
		cmocka_unit_test(test_read_rows_number),
		cmocka_unit_test(test_read_rows_carriage_return),
		cmocka_unit_test(test_read_rows_null_input),
	};
	const struct CMUnitTest read_columns_test[] = {
		cmocka_unit_test(test_read_columns_column_number),
		cmocka_unit_test(test_read_columns_error_return),
		cmocka_unit_test(test_read_columns_call_encode),
	};
	const struct CMUnitTest offset_transform_test[] = {
		cmocka_unit_test(test_log_offset_transform),
	};
 
	return cmocka_run_group_tests(column_alloc_test, NULL, NULL) &
		cmocka_run_group_tests(column_free_test, NULL, NULL) &
		cmocka_run_group_tests(detect_type_test, NULL, NULL) &
		cmocka_run_group_tests(translate_row_value_test, NULL, NULL) &
		cmocka_run_group_tests(process_row_test, NULL, NULL) &
		cmocka_run_group_tests(read_rows_test, NULL, NULL) &
		cmocka_run_group_tests(read_columns_test, NULL, NULL) &
		cmocka_run_group_tests(offset_transform_test, NULL, NULL);
}
