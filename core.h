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

typedef struct rowValue {
	char * value;
	struct rowValue * nextValue;
} rowValue;

typedef enum {
	TYPE_DOUBLE,
	TYPE_STRING
} valueType;

typedef struct dataColumn {
	char * name;
	valueType type;
	gsl_vector * vector;
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

int process_row(dataColumn * data, size_t n, int row, char * line,
		bool is_header)
{
	dataColumn * colHead = data;
	rowValue * values;
	rowValue * rowHead = values;
	char * raw;
	double value;
	int ncol;

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
		ncol = 0;

		colHead->name = rowHead->value;
		colHead->vector = gsl_vector_calloc(n);
		rowHead = rowHead->nextValue;
		while (rowHead) {
			colHead->nextColumn = malloc(sizeof(dataColumn));
			colHead = colHead->nextColumn;
			colHead->name = rowHead->value;
			colHead->vector = gsl_vector_calloc(n);
			rowHead = rowHead->nextValue;

			ncol++;
		}
		colHead->nextColumn = NULL;
	} else {
		ncol = 0;

		while (rowHead) {
			if (colHead->type) {
				if (colHead->type ==
					detect_type(rowHead->value)) {
					perror("Mismatch in column types.");
				}
			} else {
				colHead->type = detect_type(rowHead->value);
			}

			sscanf(rowHead->value, "%lf", &value);
			gsl_vector_set(colHead->vector, row - 1, value);
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

int read_table(dataColumn ** columnHead, gsl_matrix ** dataMatrix, gsl_vector ** response)
{
	char * line = NULL;
	char ** lines = NULL;
	size_t len;
	char * p;
	int nrow = 0;
	int ncol = 0;
	int capacity = 0;
	int status;
	dataColumn * colHead;
	// gsl_vector * intercept;
	char * intercept = "intercept,";
	bool first = true;

	// Allocate memory for data
	colHead = *columnHead = malloc(sizeof(dataColumn));

	// Read in the document
	while (getline(&line, &len, stdin) != -1) {
		// Reallocate if more memory is needed
		if (nrow >= capacity) {
			capacity = capacity == 0 ? 1 : capacity * 2;
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
	}

	// Set up output matrix
	*response = gsl_vector_calloc(nrow);
	*dataMatrix = gsl_matrix_calloc(nrow, ncol);

	// Set the first column as the response vector
	status = gsl_vector_memcpy(*response, colHead->vector);
	colHead = colHead->nextColumn;

	// Putt all other rows in the data matrix
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
