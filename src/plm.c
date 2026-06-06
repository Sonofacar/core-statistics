#include <gsl/gsl_multifit.h>
#include <unistd.h>
#include <time.h>
#include "core.h"
#include "model_utils.h"

#define MULT_LAMBDAS "Multiple lambda-related options set; only set one of " \
	"lambda, gcv-curve, and l-curve."

int main(int argc, char *argv[])
{
	// Command-line options
	const struct option commandOptions[] = {
		COMMON_OPTIONS,
		{"lambda", required_argument, NULL, 'p'}, \
		{"gcv-curve", no_argument, NULL, 'g'}, \
		{"l-curve", no_argument, NULL, 'c'}, \
	};
	int opt;
	modelConfigType * config;

	// Model variables
	int nrow;
	int ncol;
	int testRows;
	double lambda = 0;
	double tmpLambda;
	double chisq;
	double rnorm;
	double snorm;
	char ** lines = NULL;
	char ** testLines = NULL;
	char ** colNames = NULL;
	encodeData * encodingInfo;
	dataColumn * columnHead;
	dataColumn * testData;
	dataColumn * colPtr;
	gsl_vector * response;
	gsl_vector * coef;
	gsl_matrix * dataMatrix;
	gsl_multifit_linear_workspace * work;

	config = malloc(sizeof(modelConfigType));
	config->input = stdin;
	while ((opt = getopt_long_only(argc, argv, COMMON_OPTION_STRING "p:gc",
					commandOptions, NULL)) != -1) {
		if (parse_args(opt, config, LM_HELP_MESSAGE)) {
			return 1;
		}
		if (opt == 'p') {
			sscanf(optarg, "%lf", &tmpLambda);
			if (0 > tmpLambda) {
				fprintf(stderr, "Lambda value must be "
						"greater than or equal to "
						"0.\n");
				return 1;
			}
			if (0 != lambda) {
				fprintf(stderr, MULT_LAMBDAS);
				return 1;
			}
			lambda = tmpLambda;
		}
		if (opt == 'g') {
			if (0 != lambda) {
				fprintf(stderr, MULT_LAMBDAS);
				return 1;
			}
			lambda = -1;
		}
		if (opt == 'c') {
			if (0 != lambda) {
				fprintf(stderr, MULT_LAMBDAS);
				return 1;
			}
			lambda = -2;
		}
	}

	// Set random seed
	srand(time(NULL));

	// Parse incoming csv file
	nrow = read_rows(&lines, config->input);
	testRows = test_split(&lines, &testLines, config->testRatio, nrow);
	nrow -= testRows;
	fclose(config->input);
	columnHead = column_alloc(nrow, "");
	testData = column_alloc(testRows, "");
	switch(config->encoding) {
		case ENCODE_DUMMY:
			encodingInfo = NULL;
			ncol = read_columns(columnHead, lines, dummy_encode,
					nrow, &encodingInfo);
			if (testRows > 0) {
				read_columns(testData, testLines, dummy_encode,
		 			testRows, &encodingInfo);
			}
			break;

		case ENCODE_MEAN_TARGET:
			encodingInfo = NULL;
			ncol = read_columns(columnHead, lines,
					mean_target_encode, nrow,
					&encodingInfo);
			if (testRows > 0) {
				read_columns(testData, testLines,
					mean_target_encode, testRows,
					&encodingInfo);
			}
			break;

		case ENCODE_MEDIAN_TARGET:
			encodingInfo = NULL;
			ncol = read_columns(columnHead, lines,
					median_target_encode, nrow,
					&encodingInfo);
			if (testRows > 0) {
				read_columns(testData, testLines,
					median_target_encode, testRows,
					&encodingInfo);
			}
			break;

		case ENCODE_NONE:
			encodingInfo = NULL;
			ncol = read_columns(columnHead, lines, no_encode,
					nrow, &encodingInfo);
			if (testRows > 0) {
				read_columns(testData, testLines, no_encode,
					testRows, &encodingInfo);
			}
			break;
	}
	dataMatrix = gsl_matrix_alloc(nrow, ncol);
	response = columnHead->vector; // First column is the response
	if (arrange_data(columnHead, dataMatrix, ncol)) return 1;

	// Pull out column names, skipping the response
	colPtr = columnHead;
	colPtr = colPtr->nextColumn;
	colNames = malloc(ncol * sizeof(char *));
	for (int i = 0; i < ncol; i++) {
		colNames[i] = strdup(colPtr->name);
		colPtr = colPtr->nextColumn;
	}

	// We cannot make a model with more columns than rows
	if (nrow < ncol) {
		fprintf(stderr, "More columns than rows; check encoding "
	  			"method or test ratio.\n");
		exit(EXIT_FAILURE);
	}

	// Allocate remaining data
	work = gsl_multifit_linear_alloc(nrow, ncol);
	coef = gsl_vector_calloc(ncol);

	// Perform transformation if necessary
	switch(config->transformation) {
		case TRANSFORM_LOG:
			transform(response, log, nrow);
			break;

		case TRANSFORM_LOG_OFFSET:
			transform(response, log_offset, nrow);
			break;

		case TRANSFORM_NONE:
			break;
	}

	// Fit the model
	if (gsl_multifit_linear_svd(dataMatrix, work)) {
		return 1;
	}
	if (lambda == -1) {		// GCV-Curve
		gsl_vector * G = gsl_vector_alloc(nrow);
		double G_gcv;
		gsl_vector * regParam = gsl_vector_alloc(nrow);
		gsl_multifit_linear_gcv(response, regParam, G, &lambda, &G_gcv,
				work);
	} else if (lambda == -1) {	// L-Curve
		size_t idx;
		gsl_vector * rho = gsl_vector_alloc(nrow);
		gsl_vector * eta = gsl_vector_alloc(nrow);
		gsl_vector * regParam = gsl_vector_alloc(nrow);
		gsl_multifit_linear_lcurve(response, regParam, rho, eta, work);
		gsl_multifit_linear_lcorner(rho, eta, &idx);
		lambda = gsl_vector_get(regParam, idx);
	}
	if (gsl_multifit_linear_solve(lambda, dataMatrix, response, coef,
				&rnorm, &snorm, work)) {
		return 1;
	}
	chisq = pow(rnorm, 2.0) + pow(lambda * snorm, 2.0);
	gsl_matrix_free(dataMatrix);
	gsl_multifit_linear_free(work);

	// Print diagnostics
	diagnostics(config->diagnostic, chisq, response, coef, NULL, colNames,
			testRows, testData, config->name);

	// Free memory
	column_free(testData);
	free(colNames);
	free(config);
	gsl_vector_free(coef);

	return 0;
}
