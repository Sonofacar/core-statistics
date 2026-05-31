#include <gsl/gsl_multifit.h>
#include <unistd.h>
#include <time.h>
#include "core.h"
#include "model_utils.h"

// Field 2:
// no_argument: 0
// required_argument: 1
// optional_argument: 2
static struct option longOptions[] = {
	{"help", 0, NULL, 'h'},
	{"input", 1, NULL, 'i'},
	{"dummy", 0, NULL, 'd'},
	{"target-mean", 0, NULL, 't'},
	{"target-median", 0, NULL, 'T'},
	{"log", 0, NULL, 'l'},
	{"log-offset", 0, NULL, 'L'},
	{"name", 1, NULL, 'n'},
	{"test-ratio", 1, NULL, 's'},
	{"aic", 0, NULL, 'a'},
	{"bic", 0, NULL, 'b'},
	{"r-squared", 0, NULL, 'r'},
	{"adjusted-r-squared", 0, NULL, 'R'},
	{"f-statistic", 0, NULL, 'f'},
	{"rmse", 0, NULL, 'm'},
	{"mae", 0, NULL, 'M'}
};

int main(int argc, char *argv[])
{
	// Model variables
	int opt;
	modelConfigType * config;
	int status;
	int nrow;
	int ncol;
	int testRows;
	double chisq;
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
	gsl_matrix * covMatrix;
	gsl_multifit_linear_workspace * work;

	config = malloc(sizeof(modelConfigType));
	config->input = stdin;
	while ((opt = getopt_long_only(argc, argv, ":hi:dtTlLn:s:abrRfmM",
		longOptions, NULL)) != -1) {
		status = parse_args(opt, config, LM_HELP_MESSAGE);
		if (status == 1) {
			return 1;
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
	status = arrange_data(columnHead, dataMatrix, ncol);

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
	covMatrix = gsl_matrix_calloc(ncol, ncol);

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
	gsl_multifit_linear(dataMatrix, response, coef, covMatrix, &chisq,
			work);
	gsl_matrix_free(dataMatrix);
	gsl_multifit_linear_free(work);

	// Print diagnostics
	diagnostics(config->diagnostic, chisq, response, coef, covMatrix,
			colNames, testRows, testData, config->name);

	// Free memory
	gsl_matrix_free(covMatrix);
	column_free(testData);
	free(colNames);
	free(config);
	gsl_vector_free(coef);

	return status;
}
