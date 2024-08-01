#define _POSIX_C_SOURCE 1

#include "questions.h" // for config_t, answer_state_t, build_config, check...

#include <limits.h>  // for PATH_MAX, UINT_MAX
#include <regex.h>   // for regcomp, regerror, regexec, regfree, REG_EXTE...
#include <stdio.h>   // for NULL, fprintf, sscanf, fclose, stderr, snprintf
#include <stdlib.h>  // for free, malloc
#include <string.h>  // for strcmp, strlen, strncmp
#include <strings.h> // for strcasecmp

#include "csv.h"     // for csv_read, csv_start, csv_buf
#include "helpers.h" // for d0_strlcpy

#define REGERROR_MAX 1024
#define MATCHER_MAX 1024
#define CSV_MAX 4096

#ifndef PATH_MAX
#define PATH_MAX _POSIX_PATH_MAX
#endif

struct config_s {
  int questions; // Used by pam_math.c.
  int attempts;  // Used by pam_math.c.
  char filename[PATH_MAX];
  regex_t matcher;
  int ignore_case;
};

int num_questions(config_t *config) { return config->questions; }
int num_attempts(config_t *config) { return config->attempts; }

#define STRINGIFY2(s) #s
#define STRINGIFY(s) STRINGIFY2(s)

config_t *build_config(const char *user, int argc, const char **argv) {
  config_t *config = malloc(sizeof(config_t));
  if (config == NULL) {
    fprintf(stderr, "ERROR: could not allocate config\n");
    return NULL;
  }
  config->questions = 3;
  config->attempts = 3;
  d0_strlcpy(config->filename, "/usr/lib/pam_math/questions.csv",
             sizeof(config->filename));
  config->ignore_case = 0;
  size_t userlen = strlen(user);
  char matcher[MATCHER_MAX] = ".*";

  char file_scan_fmt[32];
  snprintf(file_scan_fmt, sizeof(file_scan_fmt), "file=%%%ds", PATH_MAX - 1);
  file_scan_fmt[sizeof(file_scan_fmt) - 1] = 0;
  char matcher_scan_fmt[32];
  snprintf(matcher_scan_fmt, sizeof(matcher_scan_fmt), "match=%%%ds",
           MATCHER_MAX - 1);
  matcher_scan_fmt[sizeof(matcher_scan_fmt) - 1] = 0;

  for (int i = 0; i < argc; ++i) {
    const char *arg = argv[i];
    const char *field;
    if (arg[0] == '.') {
      field = arg + 1;
    } else if (!strncmp(arg, user, userlen) && arg[userlen] == '.') {
      field = arg + userlen + 1;
    } else {
      continue;
    }
    if (sscanf(field, "questions=%d", &config->questions) == 1) {
      continue;
    }
    if (sscanf(field, "attempts=%d", &config->attempts) == 1) {
      continue;
    }
    if (sscanf(field, file_scan_fmt, config->filename) == 1) {
      continue;
    }
    if (sscanf(field, matcher_scan_fmt, matcher) == 1) {
      continue;
    }
    if (sscanf(field, "ignore_case=%d", &config->ignore_case) == 1) {
      continue;
    }
    // TODO edit distance?
    fprintf(stderr, "Unexpected option in config: %s\n", arg);
  }

  char fullmatcher[MATCHER_MAX + 4];
  snprintf(fullmatcher, sizeof(fullmatcher), "^(%s)$", matcher);
  fullmatcher[sizeof(fullmatcher) - 1] = 0;
  int reg_error =
      regcomp(&config->matcher, fullmatcher, REG_EXTENDED | REG_NOSUB);
  if (reg_error != 0) {
    char errbuf[REGERROR_MAX];
    *errbuf = 0;
    regerror(reg_error, &config->matcher, errbuf, sizeof(errbuf));
    fprintf(stderr, "Failed to compile regex %s: %s\n", matcher, errbuf);
    free(config);
    return NULL;
  }

  if (*config->filename == 0) {
    config->questions = 0;
  }

  return config;
}

void free_config(config_t *config) {
  if (config == NULL) {
    return;
  }
  regfree(&config->matcher);
  free(config);
}

struct answer_state_s {
  char *answer;
  int ignore_case;
};

static int randint(FILE *devrandom, int min, int max) {
  unsigned int d = (unsigned int)max - (unsigned int)min;
  unsigned int rmax = (UINT_MAX / d) * d;
  for (;;) {
    unsigned int r;
    if (fread(&r, sizeof(r), 1, devrandom) != 1) {
      fprintf(
          stderr,
          "ERROR: /dev/random did not read exactly %d bytes; trying again...",
          (int)sizeof(r));
    }
    if (r < rmax) {
      return min + r % d;
    }
  }
}

char *make_question(config_t *config, answer_state_t **answer_state) {
  FILE *devrandom = fopen("/dev/random", "rb");
  if (devrandom == NULL) {
    perror("ERROR: could not open /dev/random");
    return NULL;
  }

  // Read questions file.
  FILE *questions = fopen(config->filename, "r");
  if (questions == NULL) {
    perror("ERROR: could not open questions file");
    return NULL;
  }

  // Identify CSV columns.
  int match_col = -1, question_col = -1, answer_col = -1;
  char buf[CSV_MAX];
  fgets(buf, sizeof(buf), questions); // Skip CSV header.
  csv_buf csvbuf;
  csv_start(buf, &csvbuf);
  for (int col = 0;; ++col) {
    char *col_name = csv_read(&csvbuf);
    if (col_name == NULL) {
      break;
    } else if (!strcasecmp(col_name, "match")) {
      match_col = col;
    } else if (!strcasecmp(col_name, "question")) {
      question_col = col;
    } else if (!strcasecmp(col_name, "answer")) {
      answer_col = col;
    }
    free(col_name);
  }
  if (question_col == -1 || answer_col == -1) {
    fprintf(stderr, "ERROR: no column named question or answer found\n");
    fclose(questions);
    fclose(devrandom);
    return NULL;
  }

  // Pick a question at random.
  int index = 0;
  char *accepted_question = NULL;
  char *accepted_answer = NULL;
  int line = 1;
  while (fgets(buf, sizeof(buf), questions)) {
    ++line;
    char *match = NULL;
    char *question = NULL;
    char *answer = NULL;
    csv_start(buf, &csvbuf);
    for (int col = 0;; ++col) {
      char *value = csv_read(&csvbuf);
      if (value == NULL) {
        break;
      } else if (col == match_col) {
        match = value;
      } else if (col == question_col) {
        question = value;
      } else if (col == answer_col) {
        answer = value;
      } else {
        free(value);
      }
    }
    if (question == NULL || answer == NULL) {
      fprintf(stderr,
              "WARNING: no question or answer in line found in line %d\n",
              line);
      free(answer);
      free(question);
      free(match);
      continue;
    }
    if (regexec(&config->matcher, match ? match : "", 0, NULL, 0) != 0) {
      free(answer);
      free(question);
      free(match);
      continue;
    }
    free(match);
    ++index;
    if (randint(devrandom, 0, index) == 0) {
      free(accepted_answer);
      free(accepted_question);
      accepted_question = question;
      accepted_answer = answer;
    } else {
      free(answer);
      free(question);
    }
  }

  fclose(questions);
  fclose(devrandom);

  if (accepted_answer == NULL) {
    fprintf(stderr, "ERROR: could not find a single question\n");
    free(accepted_question);
    return NULL;
  }

  *answer_state = malloc(sizeof(answer_state_t));
  if (*answer_state == NULL) {
    fprintf(stderr, "ERROR: could not allocate answer_state\n");
    return NULL;
  }
  (*answer_state)->answer = accepted_answer;
  (*answer_state)->ignore_case = config->ignore_case;

  char *formatted_question = d0_asprintf("%s ", accepted_question);
  free(accepted_question);
  return formatted_question;
}

int check_answer(answer_state_t *answer_state, const char *given) {
  if (answer_state->ignore_case) {
    return !strcasecmp(given, answer_state->answer);
  }
  return !strcmp(given, answer_state->answer);
}

char *get_answer(answer_state_t *answer_state) {
  return d0_strndup(answer_state->answer, strlen(answer_state->answer));
}

void free_answer(answer_state_t *answer_state) {
  if (answer_state == NULL) {
    return;
  }
  free(answer_state->answer);
  free(answer_state);
}
