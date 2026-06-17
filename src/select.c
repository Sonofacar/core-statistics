#include "core.h"

/*
 * Simple program to select:
 * a) columns to drop
 * b) columns to keep (mutually exclusive to previous)
 * c) which row will be the response
 */

#define HELP_MESSAGE \
	"Choose, drop, and reorder columns of a CSV file.\n\n" \
	"USAGE:\n" \
	"\tselect [-h] [-i <path>] [-d <comma-sepparated columns>]\n" \
	"\t\t[-c <comma-sepparated columns>] [-r <column>]\n\n" \
	"STANDARD OPTIONS:\n" \
	"\t-h, --help\tPrint this help message.\n" \
	"\t-i, --input\tSpecify input file. If not given, stdin will be " \
		"used.\n\n" \
	"COLUMN OPTIONS:\n" \
	"\tOptions in this section each work by using either column names " \
		"or the\n" \
	"\tindex of the column, starting at 0.\n" \
	"\t-d, --drop\tSpecify columns to drop. May be comma-separated " \
		"list.\n" \
	"\t-c, --choose\tSpecify columns to choose. May be comma-separated " \
		"list.\n" \
	"\t-r, --response\tSet first column (response variable). May only " \
		"specify\n" \
	"\t\t\tone column.\n"

int main(int argc, char * argv[])
{
	// Options
	int opt;
	const struct option commandOptions[] = {
		{"help",	no_argument,		NULL,	'h'},
		{"input",	required_argument,	NULL,	'i'},
		{"drop",	required_argument,	NULL,	'd'},
		{"choose",	required_argument,	NULL,	'c'},
		{"response",	required_argument,	NULL,	'r'},
	};

	int nrow;
	int ncol;
	size_t response = 0;
	char * chooseStr = NULL;
	char * dropStr = NULL;
	char * responseStr = NULL;
	FILE * input;
	char ** lines = NULL;
	dataColumn * outputColumns;
	dataColumn * columnHead;
	dataColumn * colPtr;
	encodeData * encodingInfo;

	input = stdin;
	while ((opt = getopt_long_only(argc, argv, "hi:d:c:r:", commandOptions,
					NULL)) != -1) {
		switch (opt) {
			case 'h':
				printf(HELP_MESSAGE);
				return 0;
				break;
			case 'i':
				input = fopen(optarg, "r");
				break;
			case 'd':
				// Allow multiple values with delimiter
				if (chooseStr) {
					fprintf(stderr, "Cannot use drop and "
							"choose options "
							"simultaneously.\n");
					return 1;
				} else {
					dropStr = optarg;
				}
				break;
			case 'c':
				// Mutually exclusive with drop
				if (dropStr) {
					fprintf(stderr, "Cannot use drop and "
							"choose options "
							"simultaneously.\n");
					return 1;
				} else {
					chooseStr = optarg;
				}
				break;
			case 'r':
				if (responseStr) {
					fprintf(stderr, "Cannot specify "
							"multiple response "
							"variables.\n");
					return 1;
				} else {
					responseStr = optarg;
				}
				break;
			case '?':
				fprintf(stderr, "Unknown option: %s", optarg);
				return 1;
				break;
			default:
				break;
		};
	}
	nrow = read_rows(&lines, input);
	fclose(input);
	columnHead = column_alloc(nrow, "");
	ncol = read_columns(columnHead, lines, no_encode, nrow, &encodingInfo);

	size_t i = 0;
	colPtr = columnHead;
	dataColumn * columns[ncol];
	while (colPtr) {
		if (strncmp(colPtr->name, "intercept", 9)) {
			columns[i++] = colPtr;
		}
		colPtr = colPtr->nextColumn;
	}

	if (responseStr) {
		if (is_string(responseStr)) {
			response = find_column_index(columns, ncol,
					responseStr);
		} else {
			sscanf(responseStr, "%ld", &response);
		}
		if (response < (size_t)ncol) {
			outputColumns = columns[response];
		} else {
			fprintf(stderr, "Specified impossible response "
					"variable, ignoring.\n");
			outputColumns = columns[0];
		}
	} else {
		outputColumns = columns[0];
	}

	colPtr = outputColumns;
	if (chooseStr) {
		char * token = strtok(chooseStr, ",");
		while (token) {
			if (is_string(token)) {
				i = find_column_index(columns, ncol, token);
			} else {
				sscanf(token, "%ld", &i);
			}
			if (response != i) {
				colPtr->nextColumn = columns[i];
				colPtr = colPtr->nextColumn;
			} else {
				fprintf(stderr, "Cannot \"choose\" column and "
						"specify as response. It will "
						"only be set as the "
						"response.\n");
			}
			token = strtok(NULL, ",");
		}
	} else if (dropStr) {
		bool keepColumn[ncol];
		memset(keepColumn, 1, sizeof(keepColumn));
		char * token = strtok(dropStr, ",");
		while (token) {
			if (is_string(token)) {
				i = find_column_index(columns, ncol, token);
			} else {
				sscanf(token, "%ld", &i);
			}
			if (response != i) {
				keepColumn[i] = false;
			} else {
				fprintf(stderr, "Cannot \"drop\" column and "
						"specify as response. It will "
						"be kept as the response.\n");
			}
			token = strtok(NULL, ",");
		}
		for (i = 0; i < (size_t)ncol; i++) {
			if ((response != i) && keepColumn[i]) {
				colPtr->nextColumn = columns[i];
				colPtr = colPtr->nextColumn;
			}
		}
	} else {
		for (i = 0; i < (size_t)ncol; i++) {
			if (response != i) {
				colPtr->nextColumn = columns[i];
				colPtr = colPtr->nextColumn;
			}
		}
	}
	colPtr->nextColumn = NULL;

	print_columns(outputColumns, stdout);
	column_free(columnHead);
	return 0;
}
