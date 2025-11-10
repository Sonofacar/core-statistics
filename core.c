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
#include <gsl/gsl_multifit.h>

// Random Variable Distributions
#include <gsl/gsl_cdf.h>

// Other math functions
#include <math.h>
#include <gsl/gsl_statistics_double.h>

#include "core.h"

dataColumn * column_alloc(int n, char name[])
{
	dataColumn * output = malloc(sizeof(dataColumn));
	if (!output) {
		return NULL;
	}
	output->name = strdup(name);
	output->vector = gsl_vector_alloc(n);
	if (!output->vector) {
		free(output->name);
		free(output);
		return NULL;
	}
	output->to_encode = NULL;
	output->nextColumn = NULL;

	return output;
}

void column_free(dataColumn * data)
{
	if (data) {
		if (data->name) {
			free(data->name);
			data->name = NULL;
		}

		if (data->vector) {
			gsl_vector_free(data->vector);
			data->vector = NULL;
		}

		if (data->nextColumn) {
			column_free(data->nextColumn);
			data->nextColumn = NULL;
		}

		free(data);
		data = NULL;
	}
}

valueType detect_type(const char *value)
{
	bool has_dot = false;
	bool has_digit = false;
	const char *p = value;
	const char *P = p + strlen(p) - 1;

	// Skip leading whitespaces
	while (isspace(*p)) p++;

	// Set end to final non-trailing whitespace
	while (isspace(*P)) {
		P--;
	}
	P++; // Adjust back to the final space

	// Optional sign
	if (*p == '+' || *p == '-') p++;

	while (*p) {
		if (isdigit(*p)) {
			has_digit = true;
		} else if (*p == '.') {
			if (has_dot) return TYPE_STRING; // multiple dots = string
			has_dot = true;
		} else if (p == P) {
			break;
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

	// Set type on the first value
	if (n == 1) {
		column->type = detect_type(row->value);
	}

	// Throw error if types don't match
	if (column->type != detect_type(row->value)) {
		perror("Mismatch in column types.");
	}

	if (column->type == TYPE_DOUBLE) {
		// Doubles immediately set values
		sscanf(row->value, "%lf", &value);
		gsl_vector_set(column->vector, n - 1, value);
	} else {
		// Save data to encode later if categorical
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
	rowValue * rowHead;
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

	// Continue to collect values for all other columns
	while ((raw = strsep(&line, ","))) {
		rowHead->nextValue = malloc(sizeof(rowValue));
		rowHead = rowHead->nextValue;
		rowHead->value = raw;
	}
	rowHead->nextValue = NULL;

	// Decide what to do with the data
	rowHead = values;
	if (is_header) {
		colHead->name = strdup(rowHead->value);
		rowHead = rowHead->nextValue;
		while (rowHead) {
			colHead->nextColumn = column_alloc(n, rowHead->value);
			colHead = colHead->nextColumn;
			rowHead = rowHead->nextValue;

			ncol++;
		}
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

int read_rows(char *** lines, FILE * input)
{
	int nrow = 0;
	char * line = NULL;
	char * p;
	size_t len;
	int capacity = 0;

	// Read in the document
	while (getline(&line, &len, input) != -1) {
		// Reallocate if more memory is needed
		if (nrow >= capacity) {
			capacity = capacity == 0 ? 1 : capacity + sizeof(char *);
			*lines = realloc(*lines, capacity * sizeof(char *));
			if (!*lines) {
				perror("Memory allocation failed");
				free(line);
				return 1;
			}
		}

		// Clean the row string
		p = strrchr(line, '\n');
		if (p) *p = '\0';
		(*lines)[nrow] = strdup(line);

		if (!(*lines)[nrow]) {
			perror("Failed to duplicate line.");
			free(line);
			return 1;
		}

		nrow++;
	}

	// No lines read
	if (nrow == 0) {
		free(line);
		return 0;
	}

	// Adjust row value to correct value
	nrow--;

	return nrow;
}

int read_columns(dataColumn * colHead, char ** lines, encode_func fn, int nrow)
{
	int addedCols = 0;
	int ncol = 0;
	dataColumn * p = colHead;

	for (int i = 0; i <= nrow; i++) {
		if (i == 0) {
			ncol = process_row(colHead, nrow, i, lines[i], true);
		} else if ((process_row(colHead, nrow, i, lines[i], false))
				!= ncol) {
			perror("Inconsistent number of columns.");
			return -1;
		}
	}

	// Encode categorical variables
	while (p) {
		if (p->to_encode) {
			addedCols += fn(p, colHead->vector, nrow);
		}
		p = p->nextColumn;
	}

	// Add new columns
	ncol = ncol + addedCols;

	return ncol;
}

int arrange_data(dataColumn * columnHead, gsl_matrix * dataMatrix, int ncol)
{
	int status;
	dataColumn * colHead = columnHead;

	// Put all other rows in the data matrix
	colHead = columnHead->nextColumn;
	for (int i = 0; i < ncol; i++) {
		status = gsl_matrix_set_col(dataMatrix, i, colHead->vector);

		if (status) {
			perror("Error in setting columns of matrix");
			return 1;
		}

		colHead = colHead->nextColumn;
	}

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

void print_coefficients(gsl_vector * coef, gsl_vector * pVals, char ** names,
		int ncol)
{
	printf("Coefficients:\n");
	printf("%17.17s\tValue\t\tP-Value\n", "Name");
	for (int i = 0; i < ncol; i++) {
		printf("%17.17s\t", names[i]);
		printf("%9.9g\t", gsl_vector_get(coef, i));
		printf("%9.9g\n", gsl_vector_get(pVals, i));
	}
}

void print_diagnostics(double rSquared, double adjRSquared, double fStat,
		double AIC, double BIC)
{
	// R-squared and adjusted R-squared
	printf("Model Diagnostics:\n");
	printf("\tR-squared: %g\n", rSquared);
	printf("\tAdjusted R-squared: %g\n", adjRSquared);

	// F statistic
	printf("\tF-statistic: %g\n", fStat);

	// AIC/BIC
	printf("\tAIC: %g\n", AIC);
	printf("\tBIC: %g\n", BIC);
}

void save_model(char * baseName, gsl_vector * coef, char ** colNames, int p)
{
	FILE * file;
	char * name;

	// Write coefficients and column names
	name = strcat(baseName, ".coef");
	file = fopen(name, "w");
	for (int i = 0; i < p; i++) {
		fprintf(file, "%s\t%g\n", colNames[i], gsl_vector_get(coef, i));
	}
	fclose(file);
}
