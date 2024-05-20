#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// TODO: make this dynamic (i.e. allocate strings on demand).
#define LINE_SIZE 80

enum { ADD, SUB, MUL, DIV, MOD };

typedef struct {
  int questions;
  int attempts;
  int amin;
  int amax;
  int mmin;
  int mmax;
  int ops;
} config_t;

config_t get_config(const char *user, int argc, const char **argv) {
  config_t config = {
      .questions = 3, .attempts = 3, .amin = 0, .amax = 10, .mmin = 2, .mmax = 9, .ops = 0};
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
          config.ops |= (1 << SUB);
        } else if (*p == '%') {
          config.ops |= (1 << MOD);
        } else {
          fprintf(stderr, "Unexpected %.*sops= character in config: %c\n",
                  (int)(field - arg), arg, *p);
        }
      }
      continue;
    }
    fprintf(stderr, "Unexpected option in config: %s\n", arg);
  }
  return config;
}

int ask_questions(pam_handle_t *pamh, config_t *config) {
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

  for (int i = 0; i < config->questions; ++i) {
    int op;

    do {
      op = rand() % 4;
    } while ((config->ops & (1 << op)) == 0);

    const char *op_str;
    int a, b, c, q;
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
      // TODO: use LC_CYTPE to pick appropriate symbol.
      op_str = "×";
      break;
    case DIV:
      c = config->mmin + rand() % (config->mmax - config->mmin + 1);
      b = config->mmin + rand() % (config->mmax - config->mmin + 1);
      if (b == 0) {
        b = 1;
      }
      a = c * b;
      // TODO: use LC_CYTPE to pick appropriate symbol.
      op_str = "÷";
      break;
    case MOD:
      q = config->mmin + rand() % (config->mmax - config->mmin + 1);
      b = config->mmin + rand() % (config->mmax - config->mmin + 1);
      if (b == 0) {
        b = 1;
      }
      c = rand() % b;
      a = q * b + c;
      // TODO: use LC_CYTPE to pick appropriate symbol.
      op_str = "mod";
      break;
    default:
      return PAM_SERVICE_ERR;
    }

    char correct[LINE_SIZE];
    snprintf(correct, sizeof(correct), "%d", c);
    correct[sizeof(correct) - 1] = 0;

    for (int j = 0; j < config->attempts; ++j) {
      const char *prefix = (j == 0) ? "" : "Incorrect. ";
      char question[LINE_SIZE];
      snprintf(question, sizeof(question), "%sWhat is %d %s %d? ", prefix, a,
               op_str, b);
      question[sizeof(question) - 1] = 0;

      struct pam_message msg;
      const struct pam_message *pmsg = &msg;
      struct pam_response *resp = NULL;
      msg.msg_style = PAM_PROMPT_ECHO_ON;
      msg.msg = question;
      retval = conv->conv(1, &pmsg, &resp, conv->appdata_ptr);
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
  }

  return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc,
                              const char **argv) {
  return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc,
                                const char **argv) {
  return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc,
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
