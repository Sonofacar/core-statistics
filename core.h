// Reading lines from /dev/stdin
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

// Creating a linear model
#include <gsl/gsl_math.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>

// Random Variable Distributions
// #include <gsl/gsl_randist.h>
#include <gsl/gsl_cdf.h>

// Other math functions
#include <math.h>
#include <gsl/gsl_statistics_double.h>

// Offset for transformations
#define OFFSET 0.01

typedef struct rowValue {
	char * value;
	struct rowValue * nextValue;
} rowValue;

typedef enum {
	TYPE_DOUBLE,
	TYPE_STRING
} valueType;

typedef enum {
	ENCODE_NONE,
	ENCODE_DUMMY,
	ENCODE_TARGET
} encodeType;

typedef enum {
	TRANSFORM_NONE,
	TRANSFORM_LOG,
	TRANSFORM_LOG_OFFSET
} transformType;

typedef struct dataColumn {
	char * name;
	valueType type;
	gsl_vector * vector;
	char ** to_encode;
	struct dataColumn * nextColumn;
} dataColumn;

typedef struct {
	dataColumn * columnHead;
	gsl_vector * response;		// vector of response
	gsl_vector * fit;		// vector of response
	gsl_vector * residuals;		// vector of response
	gsl_vector * coef;		// model coefficients
	gsl_matrix * dataMatrix;	// matrix of predictors
	gsl_matrix * covMatrix;		// covariance matrix
	double * chisq;			// model chi-squared value
	gsl_multifit_linear_workspace * work;
} multiLinearModel;

valueType detect_type(const char *value) {
	bool has_dot = false;
	bool has_digit = false;
	const char *p = value;

	// Skip leading whitespaces
	while (isspace(*p)) p++;

	// Optional sign
	if (*p == '+' || *p == '-') p++;

	while (*p) {
		if (isdigit(*p)) {
			has_digit = true;
		} else if (*p == '.') {
			if (has_dot) return TYPE_STRING; // multiple dots = string
			has_dot = true;
		} else {
			return TYPE_STRING;
		}
		p++;
	}

	if (!has_digit) return TYPE_STRING;
	return TYPE_DOUBLE;
}

void translate_row_value(rowValue * row, dataColumn * column, int n)
{
	double value;

	if (n == 1) {
		column->type = detect_type(row->value);
	}

	if (column->type != detect_type(row->value)) {
		perror("Mismatch in column types.");
	}

	if (column->type == TYPE_DOUBLE) {
		sscanf(row->value, "%lf", &value);
		gsl_vector_set(column->vector, n - 1, value);
	} else {
		column->to_encode = realloc(column->to_encode,
				sizeof(char *) * n);
		column->to_encode[n - 1] = strdup(row->value);
	}
}

int process_row(dataColumn * data, size_t n, int row, char * line,
		bool is_header)
{
	dataColumn * colHead = data;
	rowValue * values;
	rowValue * rowHead = values;
	char * raw;
	int ncol = 0;

	// Get first value in row
	values = malloc(sizeof(rowValue));
	rowHead = values;
	raw = strsep(&line, ",");
	rowHead->value = raw;

	// Insert intercept row
	rowHead->nextValue = malloc(sizeof(rowValue));
	rowHead = rowHead->nextValue;
	if (is_header) {
		rowHead->value = "intercept";
	} else {
		rowHead->value = "1";
	}

	// Continue to collect values for all other rows
	while (raw = strsep(&line, ",")) {
		rowHead->nextValue = malloc(sizeof(rowValue));
		rowHead = rowHead->nextValue;
		rowHead->value = raw;
	}
	rowHead->nextValue = NULL;

	// Decide what to do with the data
	rowHead = values;
	if (is_header) {
		colHead->name = strdup(rowHead->value);
		colHead->vector = gsl_vector_calloc(n);
		rowHead = rowHead->nextValue;
		while (rowHead) {
			colHead->nextColumn = malloc(sizeof(dataColumn));
			colHead = colHead->nextColumn;
			colHead->name = strdup(rowHead->value);
			colHead->vector = gsl_vector_calloc(n);
			rowHead = rowHead->nextValue;

			ncol++;
		}
		colHead->nextColumn = NULL;
	} else {
		ncol = 0;

		while (rowHead) {
			translate_row_value(rowHead, colHead, row);

			colHead = colHead->nextColumn;
			rowHead = rowHead->nextValue;
			
			ncol ++;
		}
		
		ncol--;
	}

	// Free memory
	rowHead = values->nextValue;
	while (rowHead) {
		free(values);
		values = rowHead;
		rowHead = values->nextValue;
	}

	return ncol;
}

int compare_items(const void * x, const void * y)
{
	const char * str1 = *(const char **) x;
	const char * str2 = *(const char **) y;
	return strcmp(str1, str2);
}

size_t unique_categories(char ** column, int n, char *** dest)
{
	size_t output = 0;
	char ** sorted;

	// Copy strings
	sorted = malloc(n * sizeof(char *));
	for (int i = 0; i < n; i++) {
		sorted[i] = strdup(column[i]);
	}

	// Sort values
	qsort(sorted, n, sizeof(char *), compare_items);

	// Pull out distinct values
	*dest = malloc(n * sizeof(char *));
	for (int i = 0; i < n; i++) {
		if (i == 0 || strcmp(sorted[i], sorted[i - 1])) {
			(*dest)[output++] = strdup(sorted[i]);
		}
	}

	free(sorted);

	return output;
}

int dummy_encode(dataColumn * data, int nrow)
{
	char ** categories;
	dataColumn * head;
	dataColumn * remaining;
	char * name = data->name;
	size_t ncat;
	double value;
	int newCols = 0;

	// Save link to remaining data
	remaining = data->nextColumn;

	// Find all categories
	ncat = unique_categories(data->to_encode, nrow, &categories);

	// Construct new columns
	head = data;
	for (int i = 0; i < ncat - 1; i++) {
		if (i != 0) {
			head->nextColumn = malloc(sizeof(dataColumn));
			head = head->nextColumn;
			newCols++;
		}

		head->name = strdup(name);
		strcat(head->name, "_");
		strcat(head->name, categories[i]);
		head->type = TYPE_DOUBLE;

		// Set vector
		head->vector = gsl_vector_calloc(nrow);
		for (int j = 0; j < nrow; j++) {
			if (strcmp(data->to_encode[j], categories[i]) == 0) {
				value = 1;
			} else {
				value = 0;
			}
			gsl_vector_set(head->vector, j, value);
		}
	}

	// Link back to what remains of original data
	head->nextColumn = remaining;

	return newCols;
}

void target_encode(dataColumn * data, gsl_vector * response, int nrow)
{
	char ** categories;
	size_t ncat;
	int i, j, n;
	double mean;
	double sum;
	double * ptrs[nrow];

	// Find all categories
	ncat = unique_categories(data->to_encode, nrow, &categories);

	// Calculate target means
	for (i = 0; i < ncat; i++) {
		// add value to set
		n = 0;
		for (j = 0; j < nrow; j++) {
			if (strcmp(data->to_encode[j], categories[i]) == 0) {
				ptrs[n++] = gsl_vector_ptr(data->vector, j);
				sum += gsl_vector_get(response, j);
			}
		}

		// compute mean
		mean = sum / n;

		// set values in output column
		for (j = 0; j < n; j++) {
			*ptrs[j] = mean;
			ptrs[j] = NULL;
		}
	}
}

int encode(dataColumn * data, gsl_vector * response, int nrow,
		encodeType encoding)
{
	int newCols = 0;

	dataColumn * newData;
	switch (encoding) {
		case ENCODE_DUMMY:
			newCols = dummy_encode(data, nrow);
			break;
		
		case ENCODE_TARGET:
			target_encode(data, response, nrow);
			break;

		default:
			perror("Unknown encoding type, ignoring categories");
	}

	return newCols;
}

int read_table(dataColumn ** columnHead, gsl_matrix ** dataMatrix,
		gsl_vector ** response, encodeType encoding)
{
	char * line = NULL;
	char ** lines = NULL;
	size_t len;
	char * p;
	int nrow = 0;
	int ncol = 0;
	int addedCols = 0;
	int capacity = 0;
	int status;
	dataColumn * colHead;
	bool first = true;

	// Allocate memory for data
	colHead = *columnHead = malloc(sizeof(dataColumn));

	// Read in the document
	while (getline(&line, &len, stdin) != -1) {
		// Reallocate if more memory is needed
		if (nrow >= capacity) {
			capacity = capacity == 0 ? 1 : capacity + sizeof(char *);
			lines = realloc(lines, capacity * sizeof(char *));
			if (!lines) {
				perror("Memory allocation failed");
				free(line);
				return 1;
			}
		}

		// Clean the row string
		p = strrchr(line, '\n');
		if (p) *p = '\0';
		lines[nrow] = strdup(line);

		if (!lines[nrow]) {
			perror("Failed to duplicate line.");
			free(line);
			return 1;
		}

		nrow++;
	}

	// No lines read
	if (nrow == 0) {
		free(line);
		free(lines);
		return 0;
	}

	// Adjust row value to correct value
	nrow--;

	for (size_t i = 0; i <= nrow; i++) {
		if (i == 0) {
			ncol = process_row(colHead, nrow, i, lines[i], true);
		} else if ((process_row(colHead, nrow, i, lines[i], false))
				!= ncol) {
			perror("Inconsistent number of columns.");
			return 1;
		}

		free(lines[i]);
	}

	// Set the first column as the response vector
	colHead = *columnHead;
	*response = colHead->vector;

	// Encode categorical variables
	while (colHead) {
		if (colHead->to_encode) {
			addedCols = encode(colHead, *response, nrow, encoding);
		}
		colHead = colHead->nextColumn;
	}

	// Add new columns and allocate data matrix
	ncol = ncol + addedCols;
	*dataMatrix = gsl_matrix_calloc(nrow, ncol);

	// Put all other rows in the data matrix
	colHead = (*columnHead)->nextColumn;
	for (size_t i = 0; i < ncol; i++) {
		status = gsl_matrix_set_col(*dataMatrix, i, colHead->vector);

		if (status) {
			perror("Error in setting columns of matrix");
			return 1;
		}

		colHead = colHead->nextColumn;
	}

	// Free document memory
	free(line);
	free(lines);

	return 0;
}

double log_offset(double d)
{
	return log(d + OFFSET);
}

double exp_offset(double d)
{
	return exp(d) - OFFSET;
}

void transform(gsl_vector * v, double (*func)(double), int len)
{
	double * p;

	for (int i = 0; i < len; i++) {
		p = gsl_vector_ptr(v, i);
		*p = (*func)(*p);
	}
}

void coefficient_p_values(gsl_vector * pVals, gsl_matrix * varCovar,
		gsl_vector * coef, int n, int df)
{
	double se;
	double c;
	double t;
	double upper;
	double lower;

	for (int i = 0; i < n; i++) {
		se = gsl_matrix_get(varCovar, i, i);
		c = gsl_vector_get(coef, i);
		t = fabs(c / se);
		lower = gsl_cdf_tdist_P(-t, df);
		upper = gsl_cdf_tdist_Q(t, df);
		gsl_vector_set(pVals, i, lower + upper);
	}
}

void summarize_model(gsl_vector * coef, gsl_vector * pVals, char ** names,
		int ncol, double RSQ, double adjRSQ, double fStat, double AIC,
		double BIC)
{
	// print coefficients
	// coefficient VIF
	printf("Coefficients:\n");
	printf("%17.17s\tValue\t\tP-Value\n", "Name");
	for (int i = 0; i < ncol; i++) {
		printf("%17.17s\t", names[i]);
		printf("%9.9g\t", gsl_vector_get(coef, i));
		printf("%9.9g\n", gsl_vector_get(pVals, i));
	}
	printf("\n");

	// R-squared and adjusted R-squared
	printf("Model Diagnostics:\n");
	printf("\tR-squared: %g\n", RSQ);
	printf("\tAdjusted R-squared: %g\n", adjRSQ);

	// F statistic
	printf("\tF-statistic: %g\n", fStat);

	// AIC/BIC
	printf("\tAIC: %g\n", AIC);
	printf("\tBIC: %g\n", BIC);
}

void output_model()
{
}
