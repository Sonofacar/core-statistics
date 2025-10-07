#include <gsl/gsl_multifit.h>
#include <unistd.h>
#include "core.h"

int main(int argc, char *argv[])
{
	int opt;
	encodeType encoding = ENCODE_NONE;
	transformType responseTransform = TRANSFORM_NONE;
	int status;
	int nrow;
	int ncol;
	dataColumn * columnHead;
	gsl_vector * response;
	gsl_vector * fit;
	gsl_vector * residuals;
	gsl_vector * coef;
	gsl_matrix * dataMatrix;
	gsl_matrix * covMatrix;
	double chisq;
	gsl_multifit_linear_workspace * work;

	while ((opt = getopt(argc, argv, ":hdlL")) != -1) {
		switch(opt) {
			case 'd':
				if (encoding == ENCODE_NONE) {
					encoding = ENCODE_DUMMY;
				} else {
					fprintf(stderr, "Multiple types of"
						"encoding specified\n");
					return 1;
				}
				break;

			case 'h':
				printf("Usage: lm OPTIONS\n");
				printf("Perform linear regression from the"
						"command line.\n");
				printf("\n");
				printf("OPTIONS:\n");
				printf("\t-l\tLog transform response"
						"variable\n");
				printf("\t-L\tLog transform response variable"
					"with offset (to allow for 0 values)\n");
				printf("\t-h\tPrint this help message\n");
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

			case '?':
				fprintf(stderr, "Unknown option: %c\n",
						optopt);
				return 1;
				break;
		}
	}

	// Parse incoming csv file
	status = read_table(&columnHead, &dataMatrix, &response, encoding);

	// gsl_matrix_fprintf(stdout, dataMatrix, "%g");

	// Extract data size
	nrow = dataMatrix->size1;
	ncol = dataMatrix->size2;

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

	switch(responseTransform) {
		case TRANSFORM_LOG:
			transform(response, log, nrow);
			break;

		case TRANSFORM_LOG_OFFSET:
			transform(response, log_offset, nrow);
			break;
	}

	gsl_multifit_linear(dataMatrix, response, coef, covMatrix, &chisq,
		     work);

	gsl_vector_fprintf(stdout, coef, "%g");

	return status;
}
