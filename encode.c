#include "core.h"

int no_encode(dataColumn * data, gsl_vector * response, int nrow)
{
	// Unused
	(void)data;
	(void)response;
	(void)nrow;

	return 0;
}

int dummy_encode(dataColumn * data, gsl_vector * response, int nrow)
{
	// unused
	(void)response;

	char ** categories;
	dataColumn * head;
	dataColumn * remaining;
	char * name = data->name;
	int ncat;
	double value;
	int newCols = 0;

	// Save link to remaining data
	remaining = data->nextColumn;

	// Find all categories
	categories = malloc(nrow * sizeof(char *));
	ncat = unique_categories(data->to_encode, nrow, &categories);

	// Construct new columns
	head = data;
	for (int i = 0; i < ncat - 1; i++) {
		if (i != 0) {
			head->nextColumn = column_alloc(nrow);
			head = head->nextColumn;
			newCols++;
		}

		head->name = strdup(name);
		strcat(head->name, "_");
		strcat(head->name, categories[i]);
		head->type = TYPE_DOUBLE;

		// Set vector
		for (int j = 0; j < nrow; j++) {
			if (strcmp(data->to_encode[j], categories[i]) == 0) {
				value = 1;
			} else {
				value = 0;
			}
			gsl_vector_set(head->vector, j, value);
		}
	}

	// Link back to what remains of original data
	head->nextColumn = remaining;

	// Free memory
	free(categories);

	return newCols;
}

int mean_target_encode(dataColumn * data, gsl_vector * response, int nrow)
{
	char ** categories;
	int ncat;
	int i, j, n;
	double mean;
	double sum;
	double * ptrs[nrow];

	// Find all categories
	categories = malloc(nrow * sizeof(char *));
	ncat = unique_categories(data->to_encode, nrow, &categories);

	// Calculate target means
	for (i = 0; i < ncat; i++) {
		// add value to set
		n = 0;
		for (j = 0; j < nrow; j++) {
			if (strcmp(data->to_encode[j], categories[i]) == 0) {
				ptrs[n++] = gsl_vector_ptr(data->vector, j);
				sum += gsl_vector_get(response, j);
			}
		}

		// compute mean
		mean = sum / n;

		// set values in output column
		for (j = 0; j < n; j++) {
			*ptrs[j] = mean;
			ptrs[j] = NULL;
		}
	}

	// Free memory
	free(categories);

	return 0;
}
