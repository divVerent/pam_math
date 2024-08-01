#include <langinfo.h> // for nl_langinfo, CODESET
#include <limits.h>   // for INT_MAX, INT_MIN, UINT_MAX
#include <math.h>     // for sqrt
#include <regex.h>
#include <stdio.h>  // for fprintf, sscanf, stderr, NULL, fclose, fopen
#include <stdlib.h> // for abs, malloc
#include <string.h>
#include <string.h> // for strcmp, strncmp, strlen
#include <strings.h>

#include "helpers.h" // for d0_asprintf
#include "questions.h"

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

#define PATH_MAX_STR STRINGIFY(PATH_MAX)
#define MATCHER_MAX_STR STRINGIFY(MATCHER_MAX)

config_t *build_config(const char *user, int argc, const char **argv) {
  config_t *config = malloc(sizeof(config_t));
  if (config == NULL) {
    fprintf(stderr, "ERROR: could not allocate config\n");
    return NULL;
  }
  config->questions = 3;
  config->attempts = 3;
  strncpy(config->filename, "/usr/lib/pam_math/questions.csv",
          sizeof(config->filename));
  config->filename[sizeof(config->filename) - 1] = 0;
  config->ignore_case = 0;
  size_t userlen = strlen(user);
  char matcher[MATCHER_MAX];
  *matcher = 0;
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
    if (sscanf(field, "file=%" PATH_MAX_STR "s", config->filename) == 1) {
      continue;
    }
    if (sscanf(field, "matcher=%" PATH_MAX_STR "s", matcher) == 1) {
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

static char *csv_read(char **buf) {
  if (*buf == NULL) {
    return NULL;
  }
  switch (**buf) {
  case '"': {
    char *ret = malloc(strlen(*buf) + 1);
    char *retpos = ret;
    for (;;) {
      ++buf;
      char *endptr = strchr(*buf, '"');
      if (endptr == NULL) {
        // Technically invalid CSV.
        strcpy(retpos, *buf);
        *buf = NULL;
        return ret;
      }
      strncpy(retpos, *buf, endptr - *buf);
      retpos += endptr - *buf;
      *retpos = 0;
      *buf = endptr + 1;
      switch (**buf) {
      case 0:
        *buf = NULL;
        return ret;
      case '"':
        *retpos++ = '"';
        continue;
      case ',':
        ++*buf;
        return ret;
      default:
        // Technically invalid CSV.
        strcpy(retpos, *buf);
        *buf = NULL;
        return ret;
      }
    }
  } break;
  default: {
    char *endptr = strchr(*buf, ',');
    if (endptr == NULL) {
      char *ret = d0_strndup(*buf, strlen(*buf));
      *buf = NULL;
      return ret;
    } else {
      char *ret = d0_strndup(*buf, endptr - *buf);
      *buf = endptr + 1;
      return ret;
    }
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

  // Pick a question at random.
  int index = 0;
  char *accepted_question = NULL;
  char *accepted_answer = NULL;
  char buf[CSV_MAX];
  int match_col = -1, question_col = -1, answer_col = -1;
  fgets(buf, sizeof(buf), questions); // Skip CSV header.
  char *bufptr = buf;
  for (int col = 0;; ++col) {
    char *col_name = csv_read(&bufptr);
    if (col_name == NULL) {
      break;
    }
    if (!strcasecmp(col_name, "match")) {
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
    return NULL;
  }
  int line = 1;
  while (fgets(buf, sizeof(buf), questions)) {
    ++line;
    char *match = NULL;
    char *question = NULL;
    char *answer = NULL;
    char *bufptr = buf;
    for (int col = 0;; ++col) {
      char *value = csv_read(&bufptr);
      if (col == match_col) {
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
    if (match_col != -1 &&
        regexec(&config->matcher, match ? match : "", 0, NULL, 0) != 0) {
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

  if (accepted_answer == NULL) {
    fprintf(stderr, "ERROR: could not find a single question\n");
    free(accepted_question);
    return NULL;
  }

  fclose(devrandom);

  *answer_state = malloc(sizeof(answer_state_t));
  if (*answer_state == NULL) {
    fprintf(stderr, "ERROR: could not allocate answer_state\n");
    return NULL;
  }
  (*answer_state)->answer = accepted_answer;
  (*answer_state)->ignore_case = config->ignore_case;
  return accepted_question;
}

int check_answer(answer_state_t *answer_state, const char *given) {
  if (answer_state->ignore_case) {
    return !strcasecmp(given, answer_state->answer);
  }
  return !strcmp(given, answer_state->answer);
}

void free_answer(answer_state_t *answer_state) {
  if (answer_state == NULL) {
    return;
  }
  free(answer_state->answer);
  free(answer_state);
}
