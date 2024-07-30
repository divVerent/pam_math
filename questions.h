#ifndef MATH_QUESTIONS_H
#define MATH_QUESTIONS_H

typedef struct config_s config_t;

config_t *build_config(const char *user, int argc, const char **argv);
void free_config(config_t *config);

int num_questions(config_t *config);
int num_attempts(config_t *config);

typedef struct answer_state_s answer_state_t;

char *make_question(config_t *config, answer_state_t **answer_state);
int check_answer(answer_state_t *answer_state, const char *given);
void free_answer(answer_state_t *answer_state);

#endif
