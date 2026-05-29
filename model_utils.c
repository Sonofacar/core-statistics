#include "core.h"
#include "model_utils.h"

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
