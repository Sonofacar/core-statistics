#include <gsl/gsl_multifit.h>
#include <unistd.h>
#include <getopt.h>
#include "core.h"

// Field 2:
// no_argument: 0
// required_argument: 1
// optional_argument: 2
static struct option longOptions[] = {
	{"help", 0, NULL, 'h'},
	{"dummy", 0, NULL, 'd'},
	{"log", 0, NULL, 'l'},
	{"log-offset", 0, NULL, 'L'},
	{"name", 1, NULL, 'n'}
};

int main(int argc, char *argv[])
{
	// Command line options
	int opt;
	encodeType encoding = ENCODE_NONE;
	transformType responseTransform = TRANSFORM_NONE;
	char * name;

	// Model variables
	int status;
	int nrow;
	int ncol;
	double chisq;
	double RSQ;
	double adjRSQ;
	double tss;
	double f;
	double AIC;
	double BIC;
	char ** colNames = NULL;
	dataColumn * columnHead;
	dataColumn * colPtr;
	gsl_vector * response;
	gsl_vector * fit;
	gsl_vector * residuals;
	gsl_vector * meanResponse;
	gsl_vector * coef;
	gsl_vector * st;
	gsl_vector * pVals;
	gsl_matrix * dataMatrix;
	gsl_matrix * covMatrix;
	gsl_multifit_linear_workspace * work;

	while ((opt = getopt_long_only(argc, argv, ":hdlLn:",
		longOptions, NULL)) != -1) {
		switch(opt) {
			case 'd':
				if (encoding == ENCODE_NONE) {
					encoding = ENCODE_DUMMY;
				} else {
					fprintf(stderr, "Multiple types of "
						"encoding specified\n");
					return 1;
				}
				break;

			case 'h':
				printf("Usage: lm [-h] [-d] [-l] [-L] "
					"[-n name]\n");
				printf("Perform linear regression from the "
						"command line.\n");
				printf("\n");
				printf("OPTIONS:\n");
				printf("\t-d,--dummy\tDummy encode categorical"
						"variables\n");
				printf("\t-l,--log\tLog transform response "
						"variable\n");
				printf("\t-L,--log-offset\tLog transform "
					"response variable with offset (to "
					"allow for\n");
				printf("\t\t\t0 values)\n");
				printf("\t-h,--help\tPrint this help message\n");
				return 1;
				break;

			case 'l':
				if (responseTransform  == TRANSFORM_NONE) {
					responseTransform = TRANSFORM_LOG;
				} else {
					fprintf(stderr, "Multiple"
						"transformations specified\n");
					return 1;
				}
				break;

			case 'L':
				if (responseTransform  == TRANSFORM_NONE) {
					responseTransform =
						TRANSFORM_LOG_OFFSET;
				} else {
					fprintf(stderr, "Multiple"
						"transformations specified\n");
					return 1;
				}
				break;

			case 'n':
				name = strdup(optarg);
				break;

			case '?':
				fprintf(stderr, "Unknown option: %c\n",
						optopt);
				return 1;
				break;
		}
	}

	// Parse incoming csv file
	status = read_table(&columnHead, &dataMatrix, &response, encoding);

	// Extract data size
	nrow = dataMatrix->size1;
	ncol = dataMatrix->size2;

	// Pull out column names
	colPtr = columnHead;
	// and skip the response
	colPtr = colPtr->nextColumn;
	colNames = malloc(ncol * sizeof(char *));
	for (int i = 0; i < ncol; i++) {
		colNames[i] = strdup(colPtr->name);
		colPtr = colPtr->nextColumn;
	}

	// We cannot make a model with more columns than rows
	if (nrow < ncol) {
		fprintf(stderr, "More columns than rows; remove columns or"
				"change encoding method.\n");
		exit(EXIT_FAILURE);
	}

	// Allocate remaining data
	work = gsl_multifit_linear_alloc(nrow, ncol);
	coef = gsl_vector_calloc(ncol);
	covMatrix = gsl_matrix_calloc(ncol, ncol);

	// Perform transformation if necessary
	switch(responseTransform) {
		case TRANSFORM_LOG:
			transform(response, log, nrow);
			break;

		case TRANSFORM_LOG_OFFSET:
			transform(response, log_offset, nrow);
			break;
	}

	// Fit the model
	gsl_multifit_linear(dataMatrix, response, coef, covMatrix, &chisq,
			work);

	// Calculate coefficient p-values
	pVals = gsl_vector_alloc(ncol);
	coefficient_p_values(pVals, covMatrix, coef, ncol, nrow - ncol);

	// Calculate sum of squares values
	meanResponse = gsl_vector_alloc(nrow);
	gsl_vector_set_all(meanResponse, gsl_vector_sum(response) / nrow);
	st = gsl_vector_alloc(nrow);
	status = gsl_vector_memcpy(st, response);
	status = gsl_vector_sub(st, meanResponse);
	status = gsl_vector_mul(st, st);
	tss = gsl_vector_sum(st);

	// Calculate R-squared values
	RSQ = 1 - (chisq/tss);
	adjRSQ = 1 - ((1 - RSQ) * (nrow - 1) / (nrow - ncol - 1));

	// Calculate the F-statistic
	// f = ((tss - chisq) / (ncol - 1)) / (chisq / (nrow - ncol));
	f = ((tss - chisq) * (nrow - ncol) / ((ncol - 1) * chisq));

	// Calculate AIC and BIC
	AIC = nrow * log(log(2 * M_PI) + 1 + chisq / nrow) + 2 * ncol;
	BIC = nrow * log(log(2 * M_PI) + 1 + chisq / nrow) + 2 * log(nrow);

	// Print model outputs to stdout
	summarize_model(coef, pVals, colNames, ncol, RSQ, adjRSQ, f, AIC, BIC);

	return status;
}
