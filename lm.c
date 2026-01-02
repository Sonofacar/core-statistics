#include <gsl/gsl_multifit.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include "core.h"

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
	{"f-statistic", 0, NULL, 'f'}
};

int main(int argc, char *argv[])
{
	// Command line options
	int opt;
	encodeType encoding = ENCODE_NONE;
	transformType responseTransform = TRANSFORM_NONE;
	char * name = "";
	FILE * input = stdin;
	diagnoseType output = ALL;
	double testRatio = 0;

	// Model variables
	int status;
	int nrow;
	int ncol;
	int testRows;
	double chisq;
	double rsq;
	double adjRSQ;
	double tss;
	double f;
	double aic;
	double bic;
	char ** lines = NULL;
	char ** testLines = NULL;
	char ** colNames = NULL;
	encodeData * encodingInfo;
	dataColumn * columnHead;
	dataColumn * testData;
	dataColumn * colPtr;
	gsl_vector * response;
	gsl_vector * meanResponse;
	gsl_vector * coef;
	gsl_vector * st;
	gsl_vector * pVals;
	gsl_matrix * dataMatrix;
	gsl_matrix * covMatrix;
	gsl_multifit_linear_workspace * work;

	while ((opt = getopt_long_only(argc, argv, ":hi:dtTlLn:s:abrRf",
		longOptions, NULL)) != -1) {
		switch(opt) {
			case 'h':
				printf("Usage: lm [-h] [-i file] [-n name] [TRANSFORM] [ENCODING] [DIAGNOSTIC]\n");
				printf("Perform linear regression from the "
					"command line.\n");
				printf("\n");

				printf("OPTIONS:\n");
				printf("\t-i, --input\tSpecify input file. If "
					"not given, stdin will be used.\n");
				printf("\t-n, --name\tGive a name for the "
	   				"model. This option is required to\n");
				printf("\t\t\tuse the DIAGNOSTICS option.\n");
				printf("\t-h,--help\tPrint this help "
					"message\n");
				printf("\n");

				printf("RESPONSE TRANSPOSITION:\n");
				printf("\tThe following options transpose the "
	   				"response variable. This is "
	   				"unusable\n");
				printf("\tif the response has incompatible "
	   				"values (example: values <=0 for "
	   				"log\n");
	   			printf("\ttrasposition).\n");
				printf("\n");
				printf("\t-l,--log\tLog transformation\n");
				printf("\t-L,--log-offset\tOffset log "
	   				"transformation by %g, allowing 0\n",
	   				OFFSET);
				printf("\t\t\tvalues\n");
				printf("\n");

				printf("ENCODING:\n");
				printf("\tThe following options allow for "
	   				"character values to be "
	   				"interpreted\n");
				printf("\tby encoding them numerically. If "
	   				"not used, character columns will "
	   				"come\n");
				printf("\tup as '0' values, being effectively "
	   				"ignored.\n");
				printf("\n");
				printf("\t-d,--dummy\t\tDummy/One-hot "
	   				"encoding\n");
				printf("\t-t,--target-mean\tMean target "
	   				"encoding\n");
				printf("\t-T,--target-median\tMedian target "
	   				"encoding\n");
				printf("\n");

				printf("DIAGNOSTICS:\n");
				printf("\tFor the following options, the "
	   				"'name' option must have been given "
	   				"for \n");
	   			printf("\tit to apply. Otherwise, these will "
					"just be ignored. These options \n");
	      			printf("\twill specify a single model "
					"diagnostic to print to stdout.\n");
				printf("\n");
				printf("\t-a, --aic\n");
				printf("\t-b, --bic\n");
				printf("\t-r, --r-squared\n");
				printf("\t-R, --adjusted-r-squared\n");
				printf("\t-f, --f-statistic\n");
				return 1;
				break;

			case 'i':
				input = fopen(optarg, "r");
				break;

			case 'd':
				if (encoding == ENCODE_NONE) {
					encoding = ENCODE_DUMMY;
				} else {
					fprintf(stderr, "Multiple types of "
						"encoding specified\n");
					return 1;
				}
				break;

			case 't':
				if (encoding == ENCODE_NONE) {
					encoding = ENCODE_MEAN_TARGET;
				} else {
					fprintf(stderr, "Multiple types of "
						"encoding specified\n");
					return 1;
				}
				break;

			case 'T':
				if (encoding == ENCODE_NONE) {
					encoding = ENCODE_MEDIAN_TARGET;
				} else {
					fprintf(stderr, "Multiple types of "
						"encoding specified\n");
					return 1;
				}
				break;

			case 'l':
				if (responseTransform  == TRANSFORM_NONE) {
					responseTransform = TRANSFORM_LOG;
				} else {
					fprintf(stderr, "Multiple "
						"transformations specified\n");
					return 1;
				}
				break;

			case 'L':
				if (responseTransform  == TRANSFORM_NONE) {
					responseTransform =
						TRANSFORM_LOG_OFFSET;
				} else {
					fprintf(stderr, "Multiple "
						"transformations specified\n");
					return 1;
				}
				break;

			case 'n':
				name = strdup(optarg);
				break;

			case 's':
				sscanf(optarg, "%lf", &testRatio);
				if ((0 > testRatio) || (1 < testRatio)) {
					testRatio = 0;
				}
				break;

			case 'a':
				if (output == ALL) {
					output = AIC;
				} else {
					fprintf(stderr, "Multiple diagnostics "
	     					"specified. Using the "
	     					"first.\n");
				}
				break;

			case 'b':
				if (output == ALL) {
					output = BIC;
				} else {
					fprintf(stderr, "Multiple diagnostics "
	     					"specified. Using the "
	     					"first.\n");
				}
				break;

			case 'r':
				if (output == ALL) {
					output = R_SQUARED;
				} else {
					fprintf(stderr, "Multiple diagnostics "
	     					"specified. Using the "
	     					"first.\n");
				}
				break;

			case 'R':
				if (output == ALL) {
					output = ADJ_R_SQUARED;
				} else {
					fprintf(stderr, "Multiple diagnostics "
	     					"specified. Using the "
	     					"first.\n");
				}
				break;

			case 'f':
				if (output == ALL) {
					output = F_STATISTIC;
				} else {
					fprintf(stderr, "Multiple diagnostics "
	     					"specified. Using the "
	     					"first.\n");
				}
				break;

			case '?':
				fprintf(stderr, "Unknown option: %c\n",
						optopt);
				return 1;
				break;
		}
	}

	// Set random seed
	srand(time(NULL));

	// Parse incoming csv file
	nrow = read_rows(&lines, input);
	testRows = test_split(&lines, &testLines, testRatio, nrow);
	nrow -= testRows;
	fclose(input);
	columnHead = column_alloc(nrow, "");
	testData = column_alloc(testRows, "");
	switch(encoding) {
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
		       			median_target_encode, nrow, &encodingInfo);
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
	switch(responseTransform) {
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

	// Calculate coefficient p-values
	pVals = gsl_vector_alloc(ncol);
	coefficient_p_values(pVals, covMatrix, coef, ncol, nrow - ncol);
	gsl_matrix_free(covMatrix);

	// Calculate sum of squares values
	meanResponse = gsl_vector_alloc(nrow);
	gsl_vector_set_all(meanResponse, gsl_vector_sum(response) / nrow);
	st = gsl_vector_alloc(nrow);
	status = gsl_vector_memcpy(st, response);
	status = gsl_vector_sub(st, meanResponse);
	status = gsl_vector_mul(st, st);
	tss = gsl_vector_sum(st);
	column_free(columnHead);
	gsl_vector_free(st);
	gsl_vector_free(meanResponse);

	// Calculate R-squared values
	rsq = 1 - (chisq/tss);
	adjRSQ = 1 - ((1 - rsq) * (nrow - 1) / (nrow - ncol - 1));

	// Calculate the F-statistic
	f = ((tss - chisq) * (nrow - ncol) / ((ncol - 1) * chisq));

	// Calculate AIC and BIC
	aic = nrow * log(log(2 * M_PI) + 1 + chisq / nrow) + 2 * ncol;
	bic = nrow * log(log(2 * M_PI) + 1 + chisq / nrow) + 2 * log(nrow);

	// Print model outputs to stdout
	// strlen
	if (strlen(name)) {
		save_model(name, coef, colNames, ncol);
		switch(output) {
			case ALL:
				print_diagnostics(rsq, adjRSQ, f, aic, bic);
				break;

			case AIC:
				printf("%g\n", aic);
				break;

			case BIC:
				printf("%g\n", bic);
				break;

			case R_SQUARED:
				printf("%g\n", rsq);
				break;

			case ADJ_R_SQUARED:
				printf("%g\n", adjRSQ);
				break;

			case F_STATISTIC:
				printf("%g\n", f);
				break;
		}
	} else {
		if (output != ALL) {
			fprintf(stderr, "Error: Diagnostic option given with "
				"no name. It will be ignored.\n");
		}
		print_coefficients(coef, pVals, colNames, ncol);
		printf("\n");
		print_diagnostics(rsq, adjRSQ, f, aic, bic);
	}

	// Free memory
	column_free(testData);
	free(colNames);
	gsl_vector_free(coef);
	gsl_vector_free(pVals);

	return status;
}
