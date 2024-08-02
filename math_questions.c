#include "questions.h" // for config_t, answer_state_t, build_config, check...

#include <langinfo.h> // for nl_langinfo, CODESET
#include <limits.h>   // for INT_MAX, INT_MIN
#include <math.h>     // for sqrt
#include <stdio.h>    // for fprintf, stderr, sscanf, NULL, size_t
#include <stdlib.h>   // for abs, free, malloc
#include <string.h>   // for strcmp, strncmp, strlen

#include "helpers.h" // for d0_asprintf

enum { ADD, SUB, MUL, DIV, MOD, REM, DIV_WITH_MOD, QUOT_WITH_REM, NUM_OPS };

struct config_s {
  int questions; // Used by pam_math.c.
  int attempts;  // Used by pam_math.c.
  int amin;
  int amax;
  int mmin;
  int mmax;
  int ops;

  int use_utf8; // Set from the locale.
};

int num_questions(config_t *config) { return config->questions; }
int num_attempts(config_t *config) { return config->attempts; }

// a + b must fit for all a, b in range.
#define AMIN_MIN (-(INT_MAX / 2))
#define AMAX_MAX (INT_MAX / 2)

// a * b (+ or -) c must fit for all a, b, c in range.
#define MMIN_MIN (-((int)sqrt(INT_MAX) - 1))
#define MMAX_MAX ((int)sqrt(INT_MAX) - 1)

// This uses the assumption: INT_MIN <= -INT_MAX.
#if INT_MIN + INT_MAX > 0
#error positive integer range must be smaller or equal negative integer range
#endif

config_t *build_config(const char *user, int argc, const char **argv) {
  config_t *config = malloc(sizeof(config_t));
  if (config == NULL) {
    fprintf(stderr, "ERROR: could not allocate config\n");
    return NULL;
  }
  config->questions = 3;
  config->attempts = 3;
  config->amin = 0;
  config->amax = 10;
  config->mmin = 2;
  config->mmax = 9;
  config->ops = 0;
  config->use_utf8 = -1;
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
    if (sscanf(field, "amin=%d", &config->amin) == 1) {
      continue;
    }
    if (sscanf(field, "amax=%d", &config->amax) == 1) {
      continue;
    }
    if (sscanf(field, "mmin=%d", &config->mmin) == 1) {
      continue;
    }
    if (sscanf(field, "mmax=%d", &config->mmax) == 1) {
      continue;
    }
    if (!strncmp(field, "ops=", 4)) {
      config->ops = 0;
      for (const char *p = field + 4; *p; ++p) {
        if (*p == '+') {
          config->ops |= (1 << ADD);
        } else if (*p == '-') {
          config->ops |= (1 << SUB);
        } else if (*p == '*') {
          config->ops |= (1 << MUL);
        } else if (*p == '/') {
          config->ops |= (1 << DIV);
        } else if (*p == 'm') {
          config->ops |= (1 << MOD);
        } else if (*p == 'r') {
          config->ops |= (1 << REM);
        } else if (*p == 'd') {
          config->ops |= (1 << DIV_WITH_MOD);
        } else if (*p == 'q') {
          config->ops |= (1 << QUOT_WITH_REM);
        } else {
          fprintf(stderr, "Unexpected %.*sops= character in config: %c\n",
                  (int)(field - arg), arg, *p);
        }
      }
      continue;
    }
    if (!strcmp(field, "use_utf8=auto")) {
      config->use_utf8 = -1;
      continue;
    }
    if (!strcmp(field, "use_utf8=yes")) {
      config->use_utf8 = 1;
      continue;
    }
    if (!strcmp(field, "use_utf8=no")) {
      config->use_utf8 = 0;
      continue;
    }
    fprintf(stderr, "Unexpected option in config: %s\n", arg);
  }

  int fixed = 0;

  // Fix reverse ranges.
  if (config->amax < config->amin) {
    fprintf(stderr, "Reverse additive range: %d to %d - swapping.\n",
            config->amin, config->amax);
    int h = config->amax;
    config->amax = config->amin;
    config->amin = h;
    fixed = 1;
  }
  if (config->mmax < config->mmin) {
    fprintf(stderr, "Reverse multiplicative range: %d to %d - swapping.\n",
            config->mmin, config->mmax);
    int h = config->mmax;
    config->mmax = config->mmin;
    config->mmin = h;
    fixed = 1;
  }

  // Avoid integer overflow.
  if (config->amin < AMIN_MIN) {
    fprintf(stderr, "Overly large additive range: %d to %d - shrinking.\n",
            config->amin, config->amax);
    config->amin = AMIN_MIN;
    fixed = 1;
  }
  if (config->amax < AMIN_MIN) {
    fprintf(stderr, "Overly large additive range: %d to %d - shrinking.\n",
            config->amax, config->amax);
    config->amax = AMIN_MIN;
    fixed = 1;
  }
  if (config->amin > AMAX_MAX) {
    fprintf(stderr, "Overly large additive range: %d to %d - shrinking.\n",
            config->amin, config->amin);
    config->amin = AMAX_MAX;
    fixed = 1;
  }
  if (config->amax > AMAX_MAX) {
    fprintf(stderr, "Overly large additive range: %d to %d - shrinking.\n",
            config->amin, config->amax);
    config->amax = AMAX_MAX;
    fixed = 1;
  }
  if (config->mmin < MMIN_MIN) {
    fprintf(stderr,
            "Overly large multiplicative range: %d to %d - shrinking.\n",
            config->mmin, config->mmax);
    config->mmin = MMIN_MIN;
    fixed = 1;
  }
  if (config->mmax < MMIN_MIN) {
    fprintf(stderr,
            "Overly large multiplicative range: %d to %d - shrinking.\n",
            config->mmax, config->mmax);
    config->mmax = MMIN_MIN;
    fixed = 1;
  }
  if (config->mmin > MMAX_MAX) {
    fprintf(stderr,
            "Overly large multiplicative range: %d to %d - shrinking.\n",
            config->mmin, config->mmin);
    config->mmin = MMAX_MAX;
    fixed = 1;
  }
  if (config->mmax > MMAX_MAX) {
    fprintf(stderr,
            "Overly large multiplicative range: %d to %d - shrinking.\n",
            config->mmin, config->mmax);
    config->mmax = MMAX_MAX;
    fixed = 1;
  }

  // There must be at least two options for each range.
  if (config->amax < config->amin + 1) {
    fprintf(stderr, "Invalid additive range: %d to %d - expanding.\n",
            config->amin, config->amax);
    // To avoid integer overflow, we pick one of the two possible ways to fix it
    // - namely whichever grows towards zero.
    if (config->amin < 0) {
      config->amax = config->amin + 1;
    } else {
      config->amin = config->amax - 1;
    }
    fixed = 1;
  }
  if (config->mmax < config->mmin + 1) {
    fprintf(stderr, "Invalid multiplicative range: %d to %d - expanding.\n",
            config->mmin, config->mmax);
    // To avoid integer overflow, we pick one of the two possible ways to fix it
    // - namely whichever grows towards zero.
    if (config->mmin < 0) {
      config->mmax = config->mmin + 1;
    } else {
      config->mmin = config->mmax - 1;
    }
    fixed = 1;
  }

  if (fixed) {
    fprintf(stderr,
            "Fixed additive range: %d to %d\n"
            "Fixed multiplicative range: %d to %d\n",
            config->amin, config->amax, config->mmin, config->mmax);
  }

  if (config->use_utf8 < 0) {
    // NOTE: Not calling setlocale here.
    // If the calling program doesn't call it, we probably shouldn't either.
    config->use_utf8 = !strcmp(nl_langinfo(CODESET), "UTF-8");
  }

  if (config->ops == 0) {
    config->questions = 0;
  }

  return config;
}

void free_config(config_t *config) { free(config); }

struct answer_state_s {
  int answer;
};

char *make_question(config_t *config, answer_state_t **answer_state) {
  int op;

  do {
    op = randint(NUM_OPS);
  } while ((config->ops & (1 << op)) == 0);

  int a, b, c;
  const char *op_prefix = "";
  const char *op_str = NULL;
  const char *op_suffix = "";
  do {
    int q, r, s;
    switch (op) {
    case ADD:
      a = config->amin + randint(config->amax - config->amin + 1);
      b = config->amin + randint(config->amax - config->amin + 1);
      c = a + b;
      op_str = "+";
      break;
    case SUB:
      c = config->amin + randint(config->amax - config->amin + 1);
      b = config->amin + randint(config->amax - config->amin + 1);
      a = c + b;
      op_str = "-";
      break;
    case MUL:
      a = config->mmin + randint(config->mmax - config->mmin + 1);
      b = config->mmin + randint(config->mmax - config->mmin + 1);
      c = a * b;
      if (config->use_utf8) {
        op_str = "×";
      } else {
        op_str = "*";
      }
      break;
    case DIV:
      c = config->mmin + randint(config->mmax - config->mmin + 1);
      b = config->mmin + randint(config->mmax - config->mmin + 1);
      if (b == 0) {
        continue;
      }
      a = c * b;
      if (config->use_utf8) {
        op_str = "÷";
      } else {
        op_str = "/";
      }
      break;
    case MOD:
      q = config->mmin + randint(config->mmax - config->mmin + 1);
      b = config->mmin + randint(config->mmax - config->mmin + 1);
      if (b == 0) {
        continue;
      }
      s = (b < 0 ? -1 : +1);
      // mod result always agrees in sign with divisor.
      c = s * randint(abs(b));
      a = q * b + c;
      op_str = "mod";
      break;
    case REM:
      q = config->mmin + randint(config->mmax - config->mmin + 1);
      b = config->mmin + randint(config->mmax - config->mmin + 1);
      if (b == 0) {
        continue;
      }
      // rem result always agrees in sign with dividend.
      // The dividend is not computed yet though, but only the result is!
      s = (b < 0 ? -1 : +1);
      s *= (q < 0 ? -1 : q > 0 ? +1 : randint(2) * 2 - 1);
      c = s * randint(abs(b));
      a = q * b + c;
      op_str = "rem";
      break;
    case DIV_WITH_MOD:
      c = config->mmin + randint(config->mmax - config->mmin + 1);
      b = config->mmin + randint(config->mmax - config->mmin + 1);
      if (b == 0) {
        continue;
      }
      s = (b < 0 ? -1 : +1);
      // mod result always agrees in sign with divisor.
      r = s * randint(abs(b));
      a = c * b + r;
      if (config->use_utf8) {
        op_prefix = "⌊";
        op_str = "÷";
        op_suffix = "⌋";
      } else {
        op_prefix = "floor(";
        op_str = "/";
        op_suffix = ")";
      }
      break;
    case QUOT_WITH_REM:
      // Incorrect. What is (-23) quot (-7)? 3
      // Incorrect. Login failed.
      // In Haskell, both quot and div are 3 here.
      // Something is wrong here...
      c = config->mmin + randint(config->mmax - config->mmin + 1);
      b = config->mmin + randint(config->mmax - config->mmin + 1);
      if (b == 0) {
        continue;
      }
      // rem result always agrees in sign with dividend.
      // The dividend is not computed yet though, but only the result is!
      s = (b < 0 ? -1 : +1);
      s *= (c < 0 ? -1 : c > 0 ? +1 : randint(2) * 2 - 1);
      r = s * randint(abs(b));
      a = c * b + r;
      op_prefix = "[";
      if (config->use_utf8) {
        op_str = "÷";
      } else {
        op_str = "/";
      }
      op_suffix = "]";
      break;
    default:
      fprintf(stderr, "ERROR: unreachable code: unsupported operation: %d\n",
              op);
      return NULL;
    }
  } while (op_str == NULL);

  *answer_state = malloc(sizeof(answer_state_t));
  if (*answer_state == NULL) {
    fprintf(stderr, "ERROR: could not allocate answer_state\n");
    return NULL;
  }
  (*answer_state)->answer = c;
  return d0_asprintf("What is %s%s%d%s %s %s%d%s%s? ",
                     op_prefix,                             //
                     a < 0 ? "(" : "", a, a < 0 ? ")" : "", //
                     op_str,                                //
                     b < 0 ? "(" : "", b, b < 0 ? ")" : "", //
                     op_suffix);
}

char *get_answer(answer_state_t *answer_state) {
  return d0_asprintf("%d", answer_state->answer);
}

int check_answer(answer_state_t *answer_state, const char *given) {
  int given_int;
  char too_much;
  if (sscanf(given, "%d%c", &given_int, &too_much) != 1) {
    return 0; // Invalid format.
  }
  return given_int == answer_state->answer;
}

void free_answer(answer_state_t *answer_state) { free(answer_state); }
