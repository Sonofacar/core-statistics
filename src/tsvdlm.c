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
	"\tregression.\n\n" \
	"\t-u, --unbalance\n\n" \
	"\tDo not balance magnitude of data matrix prior to SVD " \
	"decomposition. By\n" \
	"\tdefault, columns are scaled to similar magnitudes to improve " \
	"accuracy.\n"

int fit_svd_model(double tol, gsl_matrix * X, gsl_vector * y,
		gsl_vector * beta, gsl_matrix * varBeta, double * chisq,
		bool balance)
{
	size_t rank;
	double s;
	gsl_vector * yHat;
	gsl_vector * residuals;
	gsl_vector * tmpCol;
	gsl_vector * tmpRow;
	gsl_matrix * U;
	gsl_matrix * V;
	gsl_matrix * SigmaInv;
	gsl_matrix * DInv;
	gsl_matrix * DInvV;
	gsl_matrix * SigmaInvUT;
	gsl_matrix * SigmaInvInv;
	gsl_matrix * VTDInv;
	gsl_matrix * intOne;
	gsl_matrix * intTwo;
	gsl_multifit_linear_workspace * work;

	// Perform BSVD or standard SVD
	work = gsl_multifit_linear_alloc(X->size1, X->size2);
	if (balance) {
		if (gsl_multifit_linear_bsvd(X, work)) {
			return 1;
		}
	} else {
		if (gsl_multifit_linear_svd(X, work)) {
			return 1;
		}
	}
	rank = gsl_multifit_linear_rank(tol, work);

	// Set up matrices
	U = gsl_matrix_alloc(X->size1, rank);		// A
	SigmaInv = gsl_matrix_calloc(rank, rank);	// S^{-1}
	V = gsl_matrix_alloc(X->size2, rank);		// Q
	SigmaInvInv = gsl_matrix_calloc(rank, rank);
	tmpCol = gsl_vector_alloc(X->size1);
	tmpRow = gsl_vector_alloc(X->size2);
	for (size_t i = 0; i < rank; i++) {
		gsl_matrix_get_col(tmpCol, work->A, i);
		gsl_matrix_set_col(U, i, tmpCol);
		gsl_matrix_get_col(tmpRow, work->Q, i);
		gsl_matrix_set_col(V, i, tmpRow);
		s = gsl_vector_get(work->S, i);
		gsl_matrix_set(SigmaInv, i, i, 1 / s);
		gsl_matrix_set(SigmaInvInv, i, i, 1 / (s * s));
	}
	gsl_vector_free(tmpRow);
	gsl_vector_free(tmpCol);

	// Model computations: D^{-1} V \Sigma^{-1} U^T y
	DInv = gsl_matrix_calloc(X->size2, X->size2);	// D^{-1}
	DInvV = gsl_matrix_alloc(X->size2, rank);
	SigmaInvUT = gsl_matrix_alloc(rank, X->size1);
	intOne = gsl_matrix_alloc(X->size2, X->size1);
	for (size_t i = 0; i < X->size2; i++) {
		gsl_matrix_set(DInv, i, i, 1 / gsl_vector_get(work->D, i));
	}
	gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, DInv, V, 0, DInvV);
	gsl_blas_dgemm(CblasNoTrans, CblasTrans, 1.0, SigmaInv, U, 0,
			SigmaInvUT);
	gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, DInvV, SigmaInvUT, 0,
			intOne);
	gsl_blas_dgemv(CblasNoTrans, 1.0, intOne, y, 0, beta);
	gsl_matrix_free(U);
	gsl_matrix_free(SigmaInv);
	gsl_matrix_free(SigmaInvUT);
	gsl_matrix_free(intOne);

	// Residuals
	yHat = gsl_vector_alloc(y->size);
	residuals = gsl_vector_alloc(y->size);
	gsl_blas_dgemv(CblasNoTrans, 1.0, X, beta, 0, yHat);
	gsl_vector_memcpy(residuals, y);
	gsl_vector_sub(residuals, yHat);
	gsl_vector_free(yHat);

	// Variance-Covariance matrix
	// \hat\sigma^2 = RSS (Residual Sum of Squares)
	// \hat\sigma^2 D^{-1} V \Sigma^{-2} V^T D^{-1}
	VTDInv = gsl_matrix_alloc(rank, X->size2);
	intTwo = gsl_matrix_alloc(X->size2, rank);
	*chisq = gsl_stats_tss(residuals->data, residuals->stride,
			residuals->size);
	gsl_blas_dgemm(CblasTrans, CblasNoTrans, 1.0, V, DInv, 0,
			VTDInv);
	gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, DInvV, SigmaInvInv, 0,
			intTwo);
	gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, *chisq, intTwo, VTDInv, 0,
			varBeta);

	// Free up remaining allocated memory
	gsl_vector_free(residuals);
	gsl_matrix_free(DInv);
	gsl_matrix_free(V);
	gsl_matrix_free(DInvV);
	gsl_matrix_free(SigmaInvInv);
	gsl_matrix_free(VTDInv);
	gsl_matrix_free(intTwo);
	gsl_multifit_linear_free(work);

	return 0;
}

int main(int argc, char *argv[])
{
	// Command-line options
	const struct option commandOptions[] = {
		COMMON_OPTIONS,
		{"tolerance", required_argument, NULL, 'p'},
		{"unbalance", no_argument, NULL, 'u'},
	};
	int opt;
	modelConfigType * config;

	// Model variables
	bool balance;
	double tolerance;
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
	balance = true;
	while ((opt = getopt_long_only(argc, argv, COMMON_OPTION_STRING "p:u",
					commandOptions, NULL)) != -1) {
		if (parse_args(opt, config, LM_HELP_MESSAGE ADDITIONAL_HELP)) {
			return 1;
		}
		if (opt == 'p') {
			sscanf(optarg, "%lf", &tolerance);
			if ((0 >= tolerance) ||
					(1 <= tolerance)) {
				fprintf(stderr, "Tolerance value must be "
						"between 0 and 1.\n");
				return 1;
			}
		}
		if (opt == 'u') {
			balance = false;
		}
	}

	if (!tolerance) {
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
	if (fit_svd_model(tolerance, dataMatrix, response, coef, covMatrix,
				&chisq, balance)) {
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
