#include <langinfo.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char *d0_asprintf(const char *restrict fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char tiny[1];
  int n = vsnprintf(tiny, sizeof(tiny), fmt, ap);
  va_end(ap);
  if (n < 0 || n >= INT_MAX) {
    fprintf(stderr, "FATAL: vsnprintf unexpectedly returned %d\n", n);
    abort();
  }
  char *buf = malloc(n + 1);
  va_start(ap, fmt);
  int m = vsnprintf(buf, n + 1, fmt, ap);
  va_end(ap);
  if (m != n) {
    fprintf(stderr,
            "FATAL: vsnprintf non-deterministic: returned %d, then %d\n", n, m);
    abort();
  }
  buf[n] = 0;
  return buf;
}

enum { ADD, SUB, MUL, DIV, MOD, REM, DIV_WITH_MOD, QUOT_WITH_REM, NUM_OPS };

typedef struct {
  int questions;
  int attempts;
  int amin;
  int amax;
  int mmin;
  int mmax;
  int ops;
} config_t;

// a + b must fit for all a, b in range.
#define AMIN_MIN (-(INT_MAX / 2))
#define AMAX_MAX (INT_MAX / 2)

// a * b (+ or -) c must fit for all a, b, c in range.
#define MMIN_MIN (-((int)sqrt(INT_MAX) - 1))
#define MMAX_MAX ((int)sqrt(INT_MAX) - 1)

// This uses the assumption: INT_MIN < -INT_MAX.
_Static_assert(
    INT_MIN + INT_MAX <= 0,
    "positive integer range must be smaller or equal negative integer range");

static config_t get_config(const char *user, int argc, const char **argv) {
  config_t config = {.questions = 3,
                     .attempts = 3,
                     .amin = 0,
                     .amax = 10,
                     .mmin = 2,
                     .mmax = 9,
                     .ops = 0};
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
    if (sscanf(field, "questions=%d", &config.questions) == 1) {
      continue;
    }
    if (sscanf(field, "attempts=%d", &config.attempts) == 1) {
      continue;
    }
    if (sscanf(field, "amin=%d", &config.amin) == 1) {
      continue;
    }
    if (sscanf(field, "amax=%d", &config.amax) == 1) {
      continue;
    }
    if (sscanf(field, "mmin=%d", &config.mmin) == 1) {
      continue;
    }
    if (sscanf(field, "mmax=%d", &config.mmax) == 1) {
      continue;
    }
    if (!strncmp(field, "ops=", 4)) {
      config.ops = 0;
      for (const char *p = field + 4; *p; ++p) {
        if (*p == '+') {
          config.ops |= (1 << ADD);
        } else if (*p == '-') {
          config.ops |= (1 << SUB);
        } else if (*p == '*') {
          config.ops |= (1 << MUL);
        } else if (*p == '/') {
          config.ops |= (1 << DIV);
        } else if (*p == 'm') {
          config.ops |= (1 << MOD);
        } else if (*p == 'r') {
          config.ops |= (1 << REM);
        } else if (*p == 'd') {
          config.ops |= (1 << DIV_WITH_MOD);
        } else if (*p == 'q') {
          config.ops |= (1 << QUOT_WITH_REM);
        } else {
          fprintf(stderr, "Unexpected %.*sops= character in config: %c\n",
                  (int)(field - arg), arg, *p);
        }
      }
      continue;
    }
    fprintf(stderr, "Unexpected option in config: %s\n", arg);
  }

  int fixed = 0;

  // Fix reverse ranges.
  if (config.amax < config.amin) {
    fprintf(stderr, "Reverse additive range: %d to %d - swapping.\n",
            config.amin, config.amax);
    int h = config.amax;
    config.amax = config.amin;
    config.amin = h;
    fixed = 1;
  }
  if (config.mmax < config.mmin) {
    fprintf(stderr, "Reverse multiplicative range: %d to %d - swapping.\n",
            config.mmin, config.mmax);
    int h = config.mmax;
    config.mmax = config.mmin;
    config.mmin = h;
    fixed = 1;
  }

  // Avoid integer overflow.
  if (config.amin < AMIN_MIN) {
    fprintf(stderr, "Overly large additive range: %d to %d - shrinking.\n",
            config.amin, config.amax);
    config.amin = AMIN_MIN;
    fixed = 1;
  }
  if (config.amax < AMIN_MIN) {
    fprintf(stderr, "Overly large additive range: %d to %d - shrinking.\n",
            config.amax, config.amax);
    config.amax = AMIN_MIN;
    fixed = 1;
  }
  if (config.amin > AMAX_MAX) {
    fprintf(stderr, "Overly large additive range: %d to %d - shrinking.\n",
            config.amin, config.amin);
    config.amin = AMAX_MAX;
    fixed = 1;
  }
  if (config.amax > AMAX_MAX) {
    fprintf(stderr, "Overly large additive range: %d to %d - shrinking.\n",
            config.amin, config.amax);
    config.amax = AMAX_MAX;
    fixed = 1;
  }
  if (config.mmin < MMIN_MIN) {
    fprintf(stderr,
            "Overly large multiplicative range: %d to %d - shrinking.\n",
            config.mmin, config.mmax);
    config.mmin = MMIN_MIN;
    fixed = 1;
  }
  if (config.mmax < MMIN_MIN) {
    fprintf(stderr,
            "Overly large multiplicative range: %d to %d - shrinking.\n",
            config.mmax, config.mmax);
    config.mmax = MMIN_MIN;
    fixed = 1;
  }
  if (config.mmin > MMAX_MAX) {
    fprintf(stderr,
            "Overly large multiplicative range: %d to %d - shrinking.\n",
            config.mmin, config.mmin);
    config.mmin = MMAX_MAX;
    fixed = 1;
  }
  if (config.mmax > MMAX_MAX) {
    fprintf(stderr,
            "Overly large multiplicative range: %d to %d - shrinking.\n",
            config.mmin, config.mmax);
    config.mmax = MMAX_MAX;
    fixed = 1;
  }

  // There must be at least two options for each range.
  if (config.amax < config.amin + 2) {
    fprintf(stderr, "Invalid additive range: %d to %d - expanding.\n",
            config.amin, config.amax);
    // To avoid integer overflow, we pick one of the two possible ways to fix it
    // - namely whichever grows towards zero.
    if (config.amin < 0) {
      config.amax = config.amin + 2;
    } else {
      config.amin = config.amax - 2;
    }
    fixed = 1;
  }
  if (config.mmax < config.mmin + 2) {
    fprintf(stderr, "Invalid multiplicative range: %d to %d - expanding.\n",
            config.mmin, config.mmax);
    // To avoid integer overflow, we pick one of the two possible ways to fix it
    // - namely whichever grows towards zero.
    if (config.mmin < 0) {
      config.mmax = config.mmin + 2;
    } else {
      config.mmin = config.mmax - 2;
    }
    fixed = 1;
  }

  if (fixed) {
    fprintf(stderr,
            "Fixed additive range: %d to %d\n"
            "Fixed multiplicative range: %d to %d\n",
            config.amin, config.amax, config.mmin, config.mmax);
  }

  return config;
}

static int ask_questions(pam_handle_t *pamh, config_t *config) {
  const void *convp;
  int retval = pam_get_item(pamh, PAM_CONV, &convp);
  if (retval != PAM_SUCCESS) {
    return retval;
  }
  const struct pam_conv *conv = convp;
  if (conv == NULL) {
    return PAM_AUTH_ERR;
  }

  if (config->ops == 0) {
    return PAM_SUCCESS;
  }

  srand(time(NULL));

  char *prev_ctype = setlocale(LC_CTYPE, "");
  int have_utf8 = !strcmp(nl_langinfo(CODESET), "UTF-8");
  setlocale(LC_CTYPE, prev_ctype);

  for (int i = 0; i < config->questions; ++i) {
    int op;

    do {
      op = rand() % NUM_OPS;
    } while ((config->ops & (1 << op)) == 0);

    int a, b, c;
    const char *op_prefix = "";
    const char *op_str = NULL;
    const char *op_suffix = "";
    do {
      int q, r, s;
      switch (op) {
      case ADD:
        a = config->amin + rand() % (config->amax - config->amin + 1);
        b = config->amin + rand() % (config->amax - config->amin + 1);
        c = a + b;
        op_str = "+";
        break;
      case SUB:
        c = config->amin + rand() % (config->amax - config->amin + 1);
        b = config->amin + rand() % (config->amax - config->amin + 1);
        a = c + b;
        op_str = "-";
        break;
      case MUL:
        a = config->mmin + rand() % (config->mmax - config->mmin + 1);
        b = config->mmin + rand() % (config->mmax - config->mmin + 1);
        c = a * b;
        if (have_utf8) {
          op_str = "×";
        } else {
          op_str = "*";
        }
        break;
      case DIV:
        c = config->mmin + rand() % (config->mmax - config->mmin + 1);
        b = config->mmin + rand() % (config->mmax - config->mmin + 1);
        if (b == 0) {
          continue;
        }
        a = c * b;
        if (have_utf8) {
          op_str = "÷";
        } else {
          op_str = "/";
        }
        break;
      case MOD:
        q = config->mmin + rand() % (config->mmax - config->mmin + 1);
        b = config->mmin + rand() % (config->mmax - config->mmin + 1);
        if (b == 0) {
          continue;
        }
        s = (b < 0 ? -1 : +1);
        // mod result always agrees in sign with divisor.
        c = s * (rand() % b);
        a = q * b + c;
        op_str = "mod";
        break;
      case REM:
        q = config->mmin + rand() % (config->mmax - config->mmin + 1);
        b = config->mmin + rand() % (config->mmax - config->mmin + 1);
        if (b == 0) {
          continue;
        }
        // rem result always agrees in sign with dividend.
        // The dividend is not computed yet though, but only the result is!
        s = (b < 0 ? -1 : +1);
        s *= (q < 0 ? -1 : q > 0 ? +1 : (rand() % 2) * 2 - 1);
        c = s * (rand() % b);
        a = q * b + c;
        op_str = "rem";
        break;
      case DIV_WITH_MOD:
        c = config->mmin + rand() % (config->mmax - config->mmin + 1);
        b = config->mmin + rand() % (config->mmax - config->mmin + 1);
        if (b == 0) {
          continue;
        }
        s = (b < 0 ? -1 : +1);
        // mod result always agrees in sign with divisor.
        r = s * (rand() % b);
        a = c * b + r;
        if (have_utf8) {
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
        c = config->mmin + rand() % (config->mmax - config->mmin + 1);
        b = config->mmin + rand() % (config->mmax - config->mmin + 1);
        if (b == 0) {
          continue;
        }
        // rem result always agrees in sign with dividend.
        // The dividend is not computed yet though, but only the result is!
        s = (b < 0 ? -1 : +1);
        s *= (c < 0 ? -1 : c > 0 ? +1 : (rand() % 2) * 2 - 1);
        r = s * (rand() % b);
        a = c * b + r;
        op_prefix = "[";
        if (have_utf8) {
          op_str = "÷";
        } else {
          op_str = "/";
        }
        op_suffix = "]";
        break;
      default:
        return PAM_SERVICE_ERR;
      }
    } while (op_str == NULL);

    char *correct = d0_asprintf("%d", c);

    for (int j = 0; j < config->attempts; ++j) {
      const char *prefix = (j == 0) ? "" : "Incorrect. ";
      char *question = d0_asprintf("%sWhat is %s%s%d%s %s %s%d%s%s? ",
                                     prefix,                                //
                                     op_prefix,                             //
                                     a < 0 ? "(" : "", a, a < 0 ? ")" : "", //
                                     op_str,                                //
                                     b < 0 ? "(" : "", b, b < 0 ? ")" : "", //
                                     op_suffix);

      struct pam_message msg;
      const struct pam_message *pmsg = &msg;
      struct pam_response *resp = NULL;
      msg.msg_style = PAM_PROMPT_ECHO_ON;
      msg.msg = question;
      retval = conv->conv(1, &pmsg, &resp, conv->appdata_ptr);
      free(question);
      if (retval != PAM_SUCCESS) {
        return retval;
      }
      if (resp == NULL || resp[0].resp == NULL) {
        return PAM_SERVICE_ERR;
      }

      int ok = !strcmp(resp[0].resp, correct);

      free(resp[0].resp);
      free(resp);
      if (ok) {
        goto correct_answer;
      }
    }

    // Fallthrough when all attempts are exhausted.
    free(correct);
    struct pam_message msg;
    const struct pam_message *pmsg = &msg;
    struct pam_response *resp = NULL;
    msg.msg_style = PAM_ERROR_MSG;
    msg.msg = "Incorrect. Login failed.";
    retval = conv->conv(1, &pmsg, &resp, conv->appdata_ptr);
    if (retval != PAM_SUCCESS) {
      return retval;
    }
    free(resp[0].resp);
    free(resp);
    return PAM_AUTH_ERR;

  correct_answer:
    free(correct);
  }

  return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh __attribute__((unused)),
                              int flags __attribute__((unused)),
                              int argc __attribute__((unused)),
                              const char **argv __attribute__((unused))) {
  return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh __attribute__((unused)),
                                int flags __attribute__((unused)),
                                int argc __attribute__((unused)),
                                const char **argv __attribute__((unused))) {
  return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh,
                                   int flags __attribute__((unused)), int argc,
                                   const char **argv) {
  int retval;

  const char *user;
  retval = pam_get_user(pamh, &user, "Username: ");
  if (retval != PAM_SUCCESS) {
    return retval;
  }

  config_t config = get_config(user, argc, argv);
  return ask_questions(pamh, &config);
}
