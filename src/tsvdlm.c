#include <gsl/gsl_multifit.h>
#include <gsl/gsl_linalg.h>
#include <unistd.h>
#include <time.h>
#include "core.h"
#include "debug.h"
#include "model_utils.h"

#define ADDITIONAL_HELP \
	"\nUNIQUE OPTIONS:\n" \
	"\t-p, --tolerance <number between 0 and 1>\n\n" \
	"\tThis value dictates the tolerance value for SVD linear " \
	"regression. Put\n" \
	"\tsimply, a higher value means more information will be removed " \
	"from\n" \
	"\tinput data, and a lower value more closely resembles standard " \
	"linear\n" \
	"\tregression.\n"

int fit_svd_model(double tol, gsl_matrix * X, gsl_vector * y,
		gsl_vector * beta, gsl_matrix * varBeta, double * chisq)
{
	size_t rank;
	gsl_vector * UTy;
	gsl_vector * yHat;
	gsl_vector * residuals;
	gsl_vector * tmpCol;
	gsl_vector * tmpRow;
	gsl_matrix * U;
	gsl_matrix * V;
	gsl_matrix * SigmaInv;
	gsl_matrix * VSigmaInv;
	gsl_matrix * SigmaInvVT;
	gsl_multifit_linear_workspace * work;

	// Perform BSVD
	work = gsl_multifit_linear_alloc(X->size1, X->size2);
	if (gsl_multifit_linear_svd(X, work)) {
		return 1;
	}
	rank = gsl_multifit_linear_rank(tol, work);

	// Set up matrices
	U = gsl_matrix_alloc(X->size1, rank);		// A
	SigmaInv = gsl_matrix_calloc(rank, rank);	// S^-1
	V = gsl_matrix_alloc(X->size2, rank);		// Q
	VSigmaInv = gsl_matrix_calloc(X->size2, rank);
	SigmaInvVT = gsl_matrix_calloc(rank, X->size2);
	UTy = gsl_vector_alloc(rank);
	yHat = gsl_vector_alloc(y->size);
	residuals = gsl_vector_alloc(y->size);
	tmpCol = gsl_vector_alloc(X->size1);
	tmpRow = gsl_vector_alloc(X->size2);
	for (size_t i = 0; i < rank; i++) {
		gsl_matrix_get_col(tmpCol, work->A, i);
		gsl_matrix_set_col(U, i, tmpCol);
		gsl_matrix_get_col(tmpRow, work->Q, i);
		gsl_matrix_set_col(V, i, tmpRow);
		gsl_matrix_set(SigmaInv, i, i, 1 / gsl_vector_get(work->S, i));
	}

	// Model computations
	gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, V, SigmaInv, 0,
			VSigmaInv);
	gsl_blas_dgemv(CblasTrans, 1.0, U, y, 0, UTy);
	gsl_blas_dgemv(CblasNoTrans, 1.0, VSigmaInv, UTy, 0, beta);

	// Residuals
	gsl_blas_dgemv(CblasNoTrans, 1.0, X, beta, 0, yHat);
	gsl_vector_memcpy(residuals, y);
	gsl_vector_sub(residuals, yHat);

	// Variance-Covariance matrix
	*chisq = gsl_stats_tss(residuals->data, residuals->stride,
			residuals->size);
	gsl_blas_dgemm(CblasNoTrans, CblasTrans, 1.0, SigmaInv, V, 0,
			SigmaInvVT);
	gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, *chisq, VSigmaInv,
			SigmaInvVT, 0, varBeta);

	// Free up memory
	gsl_vector_free(UTy);
	gsl_vector_free(yHat);
	gsl_vector_free(residuals);
	gsl_vector_free(tmpRow);
	gsl_vector_free(tmpCol);
	gsl_matrix_free(U);
	gsl_matrix_free(SigmaInv);
	gsl_matrix_free(VSigmaInv);
	gsl_matrix_free(SigmaInvVT);
	gsl_matrix_free(V);
	gsl_multifit_linear_free(work);

	return 0;
}

int main(int argc, char *argv[])
{
	// Command-line options
	const struct option commandOptions[] = {
		COMMON_OPTIONS,
		{"tolerance", required_argument, NULL, 'p'},
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
	while ((opt = getopt_long_only(argc, argv, COMMON_OPTION_STRING "p:",
					commandOptions, NULL)) != -1) {
		if (parse_args(opt, config, LM_HELP_MESSAGE ADDITIONAL_HELP)) {
			return 1;
		}
		if (opt == 'p') {
			sscanf(optarg, "%lf", &config->svdTolerance);
			if ((0 >= config->svdTolerance) ||
					(1 <= config->svdTolerance)) {
				fprintf(stderr, "Tolerance value must be "
						"between 0 and 1.\n");
				return 1;
			}
		}
	}

	if (!config->svdTolerance) {
		fprintf(stderr, "Must set a tolerance value "
				"(argument `-p`).\n");
		return 1;
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
	if (fit_svd_model(config->svdTolerance, dataMatrix, response,
		coef, covMatrix, &chisq)) {
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
