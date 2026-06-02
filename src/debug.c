#include "debug.h"

void debug_print_vector(gsl_vector * v, char * message) {
  fprintf(stderr, "%s:", message);
	for (size_t i = 0; i < v->size; i++) {
    fprintf(stderr, "\t%f", gsl_vector_get(v, i));
  }
  fprintf(stderr, "\n");
}

void debug_print_matrix(gsl_matrix * m, char * message) {
  fprintf(stderr, "%s:\n", message);
	for (size_t i = 0; i < m->size1; i++) {
    fprintf(stderr, "row %ld: ", i);
    for (size_t j = 0; j < m->size2; j++) {
      fprintf(stderr, "\t%f", gsl_matrix_get(m, i, j));
    }
    fprintf(stderr, "\n");
  }
}
