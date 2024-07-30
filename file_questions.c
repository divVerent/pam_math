#include "questions.h"

#include <langinfo.h> // for nl_langinfo, CODESET
#include <limits.h>   // for INT_MAX, INT_MIN, UINT_MAX
#include <math.h>     // for sqrt
#include <stdio.h>    // for fprintf, sscanf, stderr, NULL, fclose, fopen
#include <stdlib.h>   // for abs, malloc
#include <string.h>   // for strcmp, strncmp, strlen

#include "asprintf.h" // for d0_asprintf

#define TAGS_MAX 1024
#define CSV_MAX 4096

struct config_s {
  int questions; // Used by pam_math.c.
  int attempts;  // Used by pam_math.c.
  char filename[PATH_MAX];
  char tags[TAGS_MAX];
  int ignore_case;
  int edit_distance;
};

int num_questions(config_t *config) { return config->questions; }
int num_attempts(config_t *config) { return config->attempts; }

#define STRINGIFY2(s) #s
#define STRINGIFY(s) STRINGIFY2(s)

#define PATH_MAX_STR STRINGIFY(PATH_MAX)
#define TAGS_MAX_STR STRINGIFY(TAGS_MAX)

config_t *build_config(const char *user, int argc, const char **argv) {
  config_t *config = malloc(sizeof(config_t));
  if (config == NULL) {
    fprintf(stderr, "ERROR: could not allocate config\n");
    return NULL;
  }
  config->questions = 3;
  config->attempts = 3;
  strncpy(config->filename, sizeof(config->filename), "/usr/lib/pam_math/questions.csv");
  config->filename[sizeof(config->filename)-1] = 0;
  *tags_re = 0;
  config->ignore_case = 0;
  config->edit_distance = 0;
  size_t userlen = strlen(user);
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
    if (sscanf(field, "file=%" PATH_MAX_STR "s", &config->filename) == 1) {
      continue;
    }
    if (sscanf(field, "tags=%" PATH_MAX_STR "s", &config->tags) == 1) {
      continue;
    }
    if (sscanf(field, "ignore_case=%d", &config->ignore_case) == 1) {
      continue;
    }
    if (sscanf(field, "edit_distance=%d", &config->edit_distance) == 1) {
      continue;
    }
    fprintf(stderr, "Unexpected option in config: %s\n", arg);
  }

  return config;
}

struct answer_state_s {
  const char *answer;
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
        char *ret = malloc(strlen(buf) + 1);
        char *retpos = ret;
        for (;;) {
          ++buf;
          char *endptr = strchr(*buf, '"');
          if (endptr == NULL) {
            // Technically invalid CSV.
            strcpy(retpos, buf);
            *buf = NULL;
            return ret;
          }
          strncpy(retpos, buf, endptr - *buf);
          retpos += endptr - *buf;
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
              strcpy(retpos, buf);
              *buf = NULL;
              return ret;
          }
        }
      }
      break;
    default: {
      char *endptr = strchr(*buf, ',');
      if (endptr == NULL) {
        char *ret = strdup(*buf);
        *buf = NULL;
        return ret;
      } else {
        char *ret = strndup(buf, endptr - buf);
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
  fgets(buf, sizeof(buf), questions); // Skip CSV header.
  int index = 0;
  char *accepted_question = NULL;
  char *accepted_answer = NULL;
  char buf[CSV_MAX];
  while (fgets(buf, sizeof(buf), questions)) {
    char *bufptr = buf;
    char *tags = csv_read(&bufptr);
    char *question = csv_read(&bufptr);
    char *answer = csv_read(&bufptr);
    if (answer == NULL) {
      free(question);
      free(tags);
      continue;
    }
    // TODO check tags.
    free(tags);
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
  return accepted_question;
}

int check_answer(answer_state_t *answer_state, const char *given) {
  return !strcmp(given, answer_state->answer);
}

void free_answer(answer_state_t *answer_state) {
  if (answer_state == NULL) {
    return;
  }
  free(answer_state->answer);
  free(answer_state);
}
