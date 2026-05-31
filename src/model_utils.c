#include <gsl/gsl_statistics_double.h>
#include "core.h"
#include "model_utils.h"

int parse_args(int opt, diagnoseType * output, FILE ** input,
		encodeType * encoding, char ** name,
		transformType * responseTransform, double * testRatio)
{
	switch(opt) {
		case 'h':
			printf("%s", LM_HELP_MESSAGE);
			return 1;
			break;

		case 'i':
			*input = fopen(optarg, "r");
			return 0;
			break;

		case 'd':
			if (*encoding == ENCODE_NONE) {
				*encoding = ENCODE_DUMMY;
			} else {
				fprintf(stderr, "Multiple types of "
					"encoding specified\n");
				return 1;
			}
			return 0;
			break;

		case 't':
			if (*encoding == ENCODE_NONE) {
				*encoding = ENCODE_MEAN_TARGET;
			} else {
				fprintf(stderr, "Multiple types of "
					"encoding specified\n");
				return 1;
			}
			return 0;
			break;

		case 'T':
			if (*encoding == ENCODE_NONE) {
				*encoding = ENCODE_MEDIAN_TARGET;
			} else {
				fprintf(stderr, "Multiple types of "
					"encoding specified\n");
				return 1;
			}
			return 0;
			break;

		case 'l':
			if (*responseTransform  == TRANSFORM_NONE) {
				*responseTransform = TRANSFORM_LOG;
			} else {
				fprintf(stderr, "Multiple "
					"transformations specified\n");
				return 1;
			}
			return 0;
			break;

		case 'L':
			if (*responseTransform  == TRANSFORM_NONE) {
				*responseTransform = TRANSFORM_LOG_OFFSET;
			} else {
				fprintf(stderr, "Multiple "
					"transformations specified\n");
				return 1;
			}
			return 0;
			break;

		case 'n':
			*name = strdup(optarg);
			return 0;
			break;

		case 's':
			sscanf(optarg, "%lf", testRatio);
			if ((0 > *testRatio) || (1 < *testRatio)) {
				*testRatio = 0;
			}
			return 0;
			break;

		case 'a':
			if (*output == ALL) {
				*output = AIC;
			} else {
				fprintf(stderr, "Multiple diagnostics "
					"specified. Using the "
					"first.\n");
			}
			return 0;
			break;

		case 'b':
			if (*output == ALL) {
				*output = BIC;
			} else {
				fprintf(stderr, "Multiple diagnostics "
					"specified. Using the "
					"first.\n");
			}
			return 0;
			break;

		case 'r':
			if (*output == ALL) {
				*output = R_SQUARED;
			} else {
				fprintf(stderr, "Multiple diagnostics "
					"specified. Using the "
					"first.\n");
			}
			return 0;
			break;

		case 'R':
			if (*output == ALL) {
				*output = ADJ_R_SQUARED;
			} else {
				fprintf(stderr, "Multiple diagnostics "
					"specified. Using the "
					"first.\n");
			}
			return 0;
			break;

		case 'f':
			if (*output == ALL) {
				*output = F_STATISTIC;
			} else {
				fprintf(stderr, "Multiple diagnostics "
					"specified. Using the "
					"first.\n");
			}
			return 0;
			break;

		case 'm':
			if (*output == ALL) {
				*output = RMSE;
			} else {
				fprintf(stderr, "Multiple diagnostics "
					"specified. Using the "
					"first.\n");
			}
			return 0;
			break;

		case 'M':
			if (*output == ALL) {
				*output = MAE;
			} else {
				fprintf(stderr, "Multiple diagnostics "
					"specified. Using the "
					"first.\n");
			}
			return 0;
			break;

		case '?':
			fprintf(stderr, "Unknown option: %c\n",
					optopt);
			return 1;
			break;

		default:
			fprintf(stderr, "Unknown option: %c\n",
					optopt);
			return 1;
			break;
	};
}

void coefficient_p_values(gsl_vector * pVals, gsl_matrix * varCovar,
		gsl_vector * coef, int n, int df)
{
	double se;
	double c;
	double t;
	double tail;

	for (int i = 0; i < n; i++) {
		se = sqrt(gsl_matrix_get(varCovar, i, i));
		c = gsl_vector_get(coef, i);
		t = fabs(c / se);
		tail = gsl_cdf_tdist_Q(t, df);
		gsl_vector_set(pVals, i, 2 * tail);
	}
}

void print_coefficients(gsl_vector * coef, gsl_vector * pVals, char ** names,
		int ncol)
{
	printf("Coefficients:\n");
	printf("%17.17s\tValue\t\tP-Value\n", "Name");
	for (int i = 0; i < ncol; i++) {
		printf("%17.17s\t", names[i]);
		printf("%9.9g\t", gsl_vector_get(coef, i));
		printf("%9.9g\n", gsl_vector_get(pVals, i));
	}
}

void print_diagnostics(double rSquared, double adjRSquared, double fStat,
		double AIC, double BIC)
{
	// R-squared and adjusted R-squared
	printf("Model Diagnostics:\n");
	printf("\tR-squared: %g\n", rSquared);
	printf("\tAdjusted R-squared: %g\n", adjRSquared);

	// F statistic
	printf("\tF-statistic: %g\n", fStat);

	// AIC/BIC
	printf("\tAIC: %g\n", AIC);
	printf("\tBIC: %g\n", BIC);
}

void save_model(char * baseName, gsl_vector * coef, char ** colNames, int p)
{
	FILE * file;
	char * name;

	// Write coefficients and column names
	name = strcat(baseName, ".coef");
	file = fopen(name, "w");
	for (int i = 0; i < p; i++) {
		fprintf(file, "%s\t%g\n", colNames[i], gsl_vector_get(coef, i));
	}
	fclose(file);
}

double diagnostics(diagnoseType type, double chisq, gsl_vector * response,
		gsl_vector * coef, gsl_matrix * covMatrix, char ** colNames,
		int testRows, dataColumn * testData, char * modelName)
{
	int ncol = coef->size - 1;
	int nrow = response->size;
	double value = 0;
	double tss = 0;
	gsl_vector * pVals;
	gsl_vector * testResponse;
	gsl_vector * testResid;
	gsl_matrix * testMatrix;

	if (testRows > 0) {
		// Test-split diagnostics
		testMatrix = gsl_matrix_alloc(testRows, ncol);
		if (arrange_data(testData, testMatrix, ncol)) {
			return -1;
		}
		testResid = gsl_vector_alloc(testRows);
		testResponse = testData->vector; // First column is the response
		if (gsl_multifit_linear_residuals(testMatrix, testResponse,
					coef, testResid)) {
			return -1;
		}
	} else {
		// Non-test-split diagnostics
		tss = gsl_stats_tss(response->data, response->stride, nrow);
	}

	switch(type) {
		case ALL:
			double aic, bic, rsq, adjRSQ, f;
			// We just print things out here
			pVals = gsl_vector_alloc(ncol);
			coefficient_p_values(pVals, covMatrix, coef, ncol,
					nrow - ncol - 1);
			aic = nrow * log(log(2 * M_PI) + 1 + chisq / nrow) +
				2 * ncol;
			bic = nrow * log(log(2 * M_PI) + 1 + chisq / nrow) +
				2 * log(nrow);
			rsq = 1 - (chisq/tss);
			adjRSQ = 1 - ((1 - rsq) * (nrow - 1) /
					(nrow - ncol - 1));
			f = ((tss - chisq) * (nrow - ncol) /
					((ncol - 1) * chisq));
			value = 0;
			print_coefficients(coef, pVals, colNames, ncol);
			printf("\n");
			print_diagnostics(rsq, adjRSQ, f, aic, bic);
			return 0; // Early return to avoid printing
			break;

		case AIC:
			value = nrow * log(log(2 * M_PI) + 1 + chisq / nrow) +
				2 * ncol;
			break;

		case BIC:
			value = nrow * log(log(2 * M_PI) + 1 + chisq / nrow) +
				2 * log(nrow);
			break;

		case R_SQUARED:
			value = 1 - (chisq/tss);
			break;

		case ADJ_R_SQUARED:
			value = 1 - ((chisq/tss) * (nrow - 1) /
					(nrow - ncol - 1));
			break;

		case F_STATISTIC:
			value = ((tss - chisq) * (nrow - ncol) /
					((ncol - 1) * chisq));
			break;

		case RMSE:
			if (gsl_vector_mul(testResid, testResid)) {
				return -1;
			}
			value = sqrt(gsl_vector_sum(testResid) / testRows);
			gsl_vector_free(testResid);
			gsl_matrix_free(testMatrix);
			break;

		case MAE:
			if (gsl_vector_mul(testResid, testResid) &&
					gsl_vector_div(testResid, testResid)) {
				return -1;
			}
			value = gsl_vector_sum(testResid) / testRows;
			gsl_vector_free(testResid);
			gsl_matrix_free(testMatrix);
			break;
	}

	printf("%f\n", value);
	save_model(modelName, coef, colNames, ncol);
	return value;
}
