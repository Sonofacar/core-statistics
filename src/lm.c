#include <gsl/gsl_multifit.h>
#include <unistd.h>
#include <time.h>
#include "core.h"
#include "model_utils.h"

#define LM_HELP_INTRO \
	"Usage: lm [-h] [-i file] [-n name] [TRANSFORM] [ENCODING] " \
		"[DIAGNOSTIC]\n" \
	"Perform linear regression from the command line.\n\n"

int main(int argc, char *argv[])
{
	// Command-line options
	const struct option commandOptions[] = {
		COMMON_OPTIONS,
	};
	int opt;
	modelConfigType * config;

	// Model variables
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
	while ((opt = getopt_long_only(argc, argv, COMMON_OPTION_STRING,
					commandOptions, NULL)) != -1) {
		if (parse_args(opt, config, LM_HELP_INTRO LM_HELP_MESSAGE)) {
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
	if (gsl_multifit_linear(dataMatrix, response, coef, covMatrix, &chisq,
				work)) {
		return 1;
	}
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

	return 0;
}
