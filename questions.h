#ifndef MATH_QUESTIONS_H
#define MATH_QUESTIONS_H

#include <langinfo.h> // for nl_langinfo, CODESET
#include <limits.h>   // for INT_MAX, INT_MIN
#include <locale.h>   // for setlocale, NULL, LC_CTYPE
#include <math.h>     // for sqrt
#include <stdio.h>    // for fprintf, sscanf, stderr, NULL, size_t
#include <stdlib.h>   // for rand, srand
#include <string.h>   // for strncmp, strcmp, strlen
#include <time.h>     // for time

#include "asprintf.h" // for d0_asprintf

typedef struct config_s config_t;

config_t *build_config(const char *user, int argc, const char **argv);

int num_questions(config_t *config);
int num_attempts(config_t *config);

typedef struct answer_state_s answer_state_t;

char *make_question(config_t *config, answer_state_t **answer_state);
int check_answer(answer_state_t *answer_state, const char *given);

#endif
