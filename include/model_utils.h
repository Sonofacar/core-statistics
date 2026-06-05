typedef struct {
	char * name;
	FILE * input;
	encodeType encoding;
	diagnoseType diagnostic;
	transformType transformation;
	double testRatio;
} modelConfigType;

// Field 2:
// no_argument: 0
// required_argument: 1
// optional_argument: 2
#define COMMON_OPTIONS \
	{"help", 0, NULL, 'h'}, \
	{"input", 1, NULL, 'i'}, \
	{"dummy", 0, NULL, 'd'}, \
	{"target-mean", 0, NULL, 't'}, \
	{"target-median", 0, NULL, 'T'}, \
	{"log", 0, NULL, 'l'}, \
	{"log-offset", 0, NULL, 'L'}, \
	{"name", 1, NULL, 'n'}, \
	{"test-ratio", 1, NULL, 's'}, \
	{"aic", 0, NULL, 'a'}, \
	{"bic", 0, NULL, 'b'}, \
	{"r-squared", 0, NULL, 'r'}, \
	{"adjusted-r-squared", 0, NULL, 'R'}, \
	{"f-statistic", 0, NULL, 'f'}, \
	{"rmse", 0, NULL, 'm'}, \
	{"mae", 0, NULL, 'M'}

#define COMMON_OPTION_STRING ":hi:dtTlLn:s:abrRfmM"

int parse_args(int opt, modelConfigType * config, char * helpMessage);

void coefficient_p_values(gsl_vector * pVals, gsl_matrix * varCovar,
		gsl_vector * coef, int n, int df);

void print_coefficients(gsl_vector * coef, gsl_vector * pVals, char ** names,
		int ncol);

void print_diagnostics(double rSquared, double adjRSquared, double fStat,
		double AIC, double BIC);

void save_model(char * baseName, gsl_vector * coef, char ** colNames, int p);

double diagnostics(diagnoseType type, double chisq, gsl_vector * response,
		gsl_vector * coef, gsl_matrix * covMatrix, char ** colNames,
		int testRows, dataColumn * testData, char * modelName);

#define LM_HELP_MESSAGE \
	"Usage: lm [-h] [-i file] [-n name] [TRANSFORM] [ENCODING] " \
		"[DIAGNOSTIC]\n" \
	"Perform linear regression from the command line.\n\n" \
	"OPTIONS:\n" \
	"\t-i, --input\tSpecify input file. If not given, stdin will be " \
		"used.\n" \
	"\t-n, --name\tGive a name for the model. This option is required " \
		"to\n" \
	"\t\t\tuse the DIAGNOSTICS option.\n" \
	"\t-h,--help\tPrint this help message\n\n" \
	"RESPONSE TRANSPOSITION:\n" \
	"\tThe following options transpose the response variable. This is " \
		"unusable\n" \
	"\tif the response has incompatible values (example: values <=0 for " \
		"log\n" \
	"\ttrasposition).\n\n" \
	"\t-l,--log\tLog transformation\n" \
	"\t-L,--log-offset\tOffset log transformation by 0.01, allowing 0\n" \
	"\t\t\tvalues\n\n" \
	"ENCODING:\n" \
	"\tThe following options allow for character values to be " \
		"interpreted\n" \
	"\tby encoding them numerically. If not used, character columns will " \
		"come\n" \
	"\tup as '0' values, being effectively ignored.\n\n" \
	"\t-d,--dummy\t\tDummy/One-hot encoding\n" \
	"\t-t,--target-mean\tMean target encoding\n" \
	"\t-T,--target-median\tMedian target encoding\n\n" \
	"DIAGNOSTICS:\n" \
	"\tFor the following options, the 'name' option must have been given " \
		"for \n" \
	"\tit to apply. Otherwise, these will just be ignored. These " \
		"options\n" \
	"\twill specify a single model diagnostic to print to stdout.\n\n" \
	"\t-a, --aic\n" \
	"\t-b, --bic\n" \
	"\t-r, --r-squared\n" \
	"\t-R, --adjusted-r-squared\n" \
	"\t-f, --f-statistic\n\n" \
	"\tSome diagnostics also require a train-test data split which can be " \
		"set\n" \
	"\twith the following command:\n\n" \
	"\t-s, --test-ratio <number between 0 and 1>\n\n" \
	"\tThe diagnostics train-test splitting enables are the following:" \
		"\n\n" \
	"\t-m, --rmse\n" \
	"\t-M, --mae\n"
