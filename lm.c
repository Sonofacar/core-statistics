#include <gsl/gsl_multifit.h>
#include <unistd.h>
#include "core.h"

// dataColumn * columnHead;
// gsl_vector * response;
// gsl_vector * fit;
// gsl_vector * residuals;
// gsl_vector * coef;
// gsl_matrix * dataMatrix;
// gsl_matrix * covMatrix;
// double * chisq;
// gsl_multifit_linear_workspace * work;

int main(int argc, char *argv[])
{
	int opt;
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

	while ((opt = getopt(argc, argv, ":h")) != -1) {
		switch(opt) {
			case 'h':
				printf("Usage: lm OPTIONS\n");
				printf("Collection of tools making it possible"
						"to wrangle and model data"
						"from the command line.\n");
				printf("\n");
				printf("OPTIONS:\n");
				printf("\t-h\tPrint this help message\n");
				return 1;
				break;

			case '?':
				fprintf(stderr, "Unknown option: %c\n",
						optopt);
				return 1;
				break;
		}
	}

	// Parse incoming csv file
	status = read_table(&columnHead, &dataMatrix, &response);

	// Extract data size
	nrow = dataMatrix->size1;
	ncol = dataMatrix->size2;

	// Allocate remaining data
	work = gsl_multifit_linear_alloc(nrow, ncol);
	coef = gsl_vector_calloc(ncol);
	covMatrix = gsl_matrix_calloc(ncol, ncol);

	gsl_multifit_linear(dataMatrix, response, coef, covMatrix, &chisq,
		     work);

	gsl_vector_fprintf(stdout, coef, "%g");

	return status;
}
