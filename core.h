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
	F_STATISTIC,
	RMSE,
	MAE
} diagnoseType;

typedef struct encodeData {
	char * columnName;
	char ** textValues;
	struct encodeData * nextEncoding;
	double * numValues;
} encodeData;

typedef int (encode_func)(dataColumn * data, gsl_vector * response, int nrow,
			  encodeData ** encoding);

dataColumn * column_alloc(int n, char name[]);

void column_free(dataColumn * data);

valueType detect_type(const char *value);

void translate_row_value(rowValue * row, dataColumn * column, int n);

int process_row(dataColumn * data, size_t n, int row, char * line,
		bool is_header);

int compare_items(const void * x, const void * y);

int unique_categories(char ** column, int n, char *** dest);

int no_encode(dataColumn * data, gsl_vector * response, int nrow,
	      encodeData ** encoding);

int dummy_encode(dataColumn * data, gsl_vector * response, int nrow,
		 encodeData ** encoding);

int mean_target_encode(dataColumn * data, gsl_vector * response, int nrow,
		       encodeData ** encoding);

int median_target_encode(dataColumn * data, gsl_vector * response, int nrow,
			 encodeData ** encoding);

int read_rows(char *** lines, FILE * input);

int read_columns(dataColumn * colHead, char ** lines, encode_func fn, int nrow,
		 encodeData ** encoding);

bool includes_int(int array[], int length, int value);

int test_split(char *** trainLines, char *** testLines, double ratio, int nrow);

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

#define LM_HELP_MESSAGE \
	"Usage: lm [-h] [-i file] [-n name] [TRANSFORM] [ENCODING] " \
		"[DIAGNOSTIC]\n" \
	"Perform linear regression from the command line.\n\n" \
	"OPTIONS:\n" \
	"\t-i, --input\tSpecify input file. If not given, stdin will be " \
		"used.\n" \
	"\t-n, --name\tGive a name for the model. This option is required " \
		"to\n" \
	"\t\t\tuse the DIAGNOSTICS option.\n" \
	"\t-h,--help\tPrint this help message\n\n" \
	"RESPONSE TRANSPOSITION:\n" \
	"\tThe following options transpose the response variable. This is " \
		"unusable\n" \
	"\tif the response has incompatible values (example: values <=0 for " \
		"log\n" \
	"\ttrasposition).\n\n" \
	"\t-l,--log\tLog transformation\n" \
	"\t-L,--log-offset\tOffset log transformation by 0.01, allowing 0\n" \
	"\t\t\tvalues\n\n" \
	"ENCODING:\n" \
	"\tThe following options allow for character values to be " \
		"interpreted\n" \
	"\tby encoding them numerically. If not used, character columns will " \
		"come\n" \
	"\tup as '0' values, being effectively ignored.\n\n" \
	"\t-d,--dummy\t\tDummy/One-hot encoding\n" \
	"\t-t,--target-mean\tMean target encoding\n" \
	"\t-T,--target-median\tMedian target encoding\n\n" \
	"DIAGNOSTICS:\n" \
	"\tFor the following options, the 'name' option must have been given " \
		"for \n" \
	"\tit to apply. Otherwise, these will just be ignored. These " \
		"options\n" \
	"\twill specify a single model diagnostic to print to stdout.\n\n" \
	"\t-a, --aic\n" \
	"\t-b, --bic\n" \
	"\t-r, --r-squared\n" \
	"\t-R, --adjusted-r-squared\n" \
	"\t-f, --f-statistic\n\n" \
	"\tSome diagnostics also require a train-test data split which can be " \
		"set\n" \
	"\twith the following command:\n\n" \
	"\t-s, --test-ratio <number between 0 and 1>\n\n" \
	"\tThe diagnostics train-test splitting enables are the following:" \
		"\n\n" \
	"\t-m, --rmse\n" \
	"\t-M, --mae\n"
