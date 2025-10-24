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
	ENCODE_MEAN_TARGET,
	ENCODE_MEDIAN_TARGET
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

typedef enum {
	ALL,
	AIC,
	BIC,
	R_SQUARED,
	ADJ_R_SQUARED,
	F_STATISTIC
} diagnoseType;

typedef int (encode_func)(dataColumn * data, gsl_vector * response, int nrow);

dataColumn * column_alloc(int n);

void column_free(dataColumn * data);

valueType detect_type(const char *value);

void translate_row_value(rowValue * row, dataColumn * column, int n);

int process_row(dataColumn * data, size_t n, int row, char * line,
		bool is_header);

int compare_items(const void * x, const void * y);

int unique_categories(char ** column, int n, char *** dest);

int no_encode(dataColumn * data, gsl_vector * response, int nrow);

int dummy_encode(dataColumn * data, gsl_vector * response, int nrow);

int mean_target_encode(dataColumn * data, gsl_vector * response, int nrow);

int median_target_encode(dataColumn * data, gsl_vector * response, int nrow);

int read_rows(char *** lines, FILE * input);

int read_columns(dataColumn * colHead, char ** lines, encode_func, int nrow);

int arrange_data(dataColumn * columnHead, gsl_matrix * dataMatrix, int ncol);

double log_offset(double d);

double exp_offset(double d);

void transform(gsl_vector * v, double (*func)(double), int len);

void coefficient_p_values(gsl_vector * pVals, gsl_matrix * varCovar,
		gsl_vector * coef, int n, int df);

void print_coefficients(gsl_vector * coef, gsl_vector * pVals, char ** names,
		int ncol);

void print_diagnostics(double rSquared, double adjRSquared, double fStat,
		double AIC, double BIC);

void save_model(char * baseName, gsl_vector * coef, char ** colNames, int p);
