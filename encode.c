#include "core.h"

int compare_items(const void * x, const void * y)
{
	const char * str1 = *(const char **) x;
	const char * str2 = *(const char **) y;
	return strcmp(str1, str2);
}

int unique_categories(char ** column, int n, char *** dest)
{
	int output = 0;
	char ** sorted;

	// Copy strings
	sorted = malloc(n * sizeof(char *));
	for (int i = 0; i < n; i++) {
		sorted[i] = strdup(column[i]);
	}

	// Sort values
	qsort(sorted, n, sizeof(char *), compare_items);

	// Pull out distinct values
	for (int i = 0; i < n; i++) {
		if (i == 0 || strcmp(sorted[i], sorted[i - 1])) {
			(*dest)[output++] = strdup(sorted[i]);
		}
	}

	free(sorted);

	return output;
}

int no_encode(dataColumn * data, gsl_vector * response, int nrow,
	      encodeData ** encoding)
{
	// Unused
	(void)data;
	(void)response;
	(void)nrow;
	(void)encoding;

	return 0;
}

int dummy_encode(dataColumn * data, gsl_vector * response, int nrow,
		 encodeData ** encoding)
{
	// unused
	(void)response;

	dataColumn * head;
	dataColumn * remaining;
	char * name = strdup(data->name);
	int ncat;
	double value;
	int newCols = 0;

	// Save link to remaining data
	remaining = data->nextColumn;

	// Find all categories
	if (*encoding) {
		// Found encoding
		ncat = unique_categories(data->to_encode, nrow,
			   &((*encoding)->textValues));
	} else {
		// Initialize encoding
		*encoding = malloc(sizeof(encodeData));
		(*encoding)->columnName = data->name;
		(*encoding)->textValues = malloc(nrow * sizeof(char *));
		ncat = unique_categories(data->to_encode, nrow,
			   &((*encoding)->textValues));
	}

	// Construct new columns
	head = data;
	for (int i = 0; i < ncat - 1; i++) {
		if (i != 0) {
			head->nextColumn = column_alloc(nrow, name);
			head = head->nextColumn;
			newCols++;
		}

		head->name = realloc(head->name, sizeof(*head->name) +
		       sizeof("_") + sizeof((*encoding)->textValues[i]));
		strcat(head->name, "_");
		strcat(head->name, (*encoding)->textValues[i]);
		head->type = TYPE_DOUBLE;

		// Set vector
		for (int j = 0; j < nrow; j++) {
			if (strcmp(data->to_encode[j],
	      			(*encoding)->textValues[i]) == 0) {
				value = 1;
			} else {
				value = 0;
			}
			gsl_vector_set(head->vector, j, value);
		}
	}

	// Link back to what remains of original data
	head->nextColumn = remaining;

	free(name);

	return newCols;
}

int mean_target_encode(dataColumn * data, gsl_vector * response, int nrow,
		       encodeData ** encoding)
{
	*encoding = NULL;
	int ncat;
	int i, j, n;
	double sum;
	double * ptrs[nrow];
	encodeData * tmpEncoding;

	// Find all categories
	if (*encoding) {
		// Found encoding
		ncat = unique_categories(data->to_encode, nrow,
			   &((*encoding)->textValues));
	} else {
		// Initialize encoding
		tmpEncoding = malloc(sizeof(encodeData));
		tmpEncoding->columnName = data->name;
		tmpEncoding->textValues = malloc(nrow * sizeof(char *));
		ncat = unique_categories(data->to_encode, nrow,
			   &(tmpEncoding->textValues));
		tmpEncoding->numValues = malloc(ncat * sizeof(double));
	}

	// Calculate target means
	for (i = 0; i < ncat; i++) {
		// add value to set
		n = 0;
		for (j = 0; j < nrow; j++) {
			if (strcmp(data->to_encode[j],
 					tmpEncoding->textValues[i]) == 0) {
				ptrs[n++] = gsl_vector_ptr(data->vector, j);
				if (!*encoding) sum += gsl_vector_get(response,
					  j);
			}
		}

		// compute mean
		if (!*encoding) tmpEncoding->numValues[i] = sum / n;

		// set values in output column
		for (j = 0; j < n; j++) {
			*ptrs[j] = tmpEncoding->numValues[i];
			ptrs[j] = NULL;
		}
	}

	*encoding = tmpEncoding;

	return 0;
}

int compare_doubles(const void * a, const void * b)
{
	double da = *(double *)a;
	double db = *(double *)b;
	return (da > db) - (da < db);
}

double median(double arr[], size_t n)
{
	double med;
	if (n == 0) return 0;

	qsort(arr, n, sizeof(double), compare_doubles);

	if (n % 2 == 1) {
		med = arr[n / 2];
	} else {
		med = (arr[n / 2 - 1] + arr[n / 2]) / 2;
	}

	return med;
}

int median_target_encode(dataColumn * data, gsl_vector * response, int nrow,
			 encodeData ** encoding)
{
	*encoding = NULL;
	int ncat;
	int i, j, n;
	double * ptrs[nrow];
	double vals[nrow];
	encodeData * tmpEncoding;

	// Find all categories
	if (*encoding) {
		// Found encoding
		ncat = unique_categories(data->to_encode, nrow,
			   &((*encoding)->textValues));
	} else {
		// Initialize encoding
		tmpEncoding = malloc(sizeof(encodeData));
		tmpEncoding->columnName = data->name;
		tmpEncoding->textValues = malloc(nrow * sizeof(char *));
		ncat = unique_categories(data->to_encode, nrow,
			   &(tmpEncoding->textValues));
		tmpEncoding->numValues = malloc(ncat * sizeof(double));
	}

	// Calculate target medians
	for (i = 0; i < ncat; i++) {
		// add value to set
		n = 0;
		for (j = 0; j < nrow; j++) {
			if (strcmp(data->to_encode[j],
 					tmpEncoding->textValues[i]) == 0) {
				vals[n] = gsl_vector_get(response, j);
				ptrs[n++] = gsl_vector_ptr(data->vector, j);
			}
		}

		// Compute median
		if (!*encoding) tmpEncoding->numValues[i] = median(vals,
						     (size_t)n);

		// set values in output column
		for (j = 0; j < n; j++) {
			*ptrs[j] = tmpEncoding->numValues[i];
			ptrs[j] = NULL;
		}
	}

	*encoding = tmpEncoding;

	return 0;
}
