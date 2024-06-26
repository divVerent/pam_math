#include <security/pam_appl.h>    // for pam_strerror, pam_response, PAM_SU...
#include <security/pam_modules.h> // for pam_handle_t, PAM_EXTERN, pam_get_...
#include <stdio.h>                // for fprintf, NULL, stderr
#include <stdlib.h>               // for free

#include "asprintf.h"  // for d0_asprintf
#include "questions.h" // for build_config, check_answer, make_q...

static int ask_questions(pam_handle_t *pamh, config_t *config) {
  const void *convp;
  int retval = pam_get_item(pamh, PAM_CONV, &convp);
  if (retval != PAM_SUCCESS) {
    fprintf(stderr, "ERROR: could not get PAM conversation: %s\n",
            pam_strerror(pamh, retval));
    return retval;
  }
  const struct pam_conv *conv = convp;
  if (conv == NULL) {
    fprintf(stderr, "ERROR: could not get PAM conversation: got NULL\n");
    return PAM_AUTH_ERR;
  }

  for (int i = 0; i < num_questions(config); ++i) {
    answer_state_t *answer_state = NULL;
    char *question = make_question(config, &answer_state);
    if (question == NULL) {
      free(answer_state);
      fprintf(stderr, "ERROR: could not generate question\n");
      return PAM_SERVICE_ERR;
    }

    for (int j = 0; j < num_attempts(config); ++j) {
      const char *prefix = (j == 0) ? "" : "Incorrect. ";
      char *msg_question = d0_asprintf("%s%s", prefix, question);
      if (msg_question == NULL) {
        free(question);
        free(answer_state);
        fprintf(stderr, "ERROR: could not prefix question\n");
        return PAM_SERVICE_ERR;
      }

      struct pam_message msg;
      const struct pam_message *pmsg = &msg;
      struct pam_response *resp = NULL;
      msg.msg_style = PAM_PROMPT_ECHO_ON;
      msg.msg = msg_question;
      retval = conv->conv(1, &pmsg, &resp, conv->appdata_ptr);

      free(msg_question);

      if (retval != PAM_SUCCESS) {
        free(question);
        free(answer_state);
        fprintf(stderr, "ERROR: could not get PAM conversation: %s\n",
                pam_strerror(pamh, retval));
        return retval;
      }
      if (resp == NULL || resp[0].resp == NULL) {
        free(question);
        free(answer_state);
        fprintf(stderr, "ERROR: could not get a response: got NULL\n");
        return PAM_SERVICE_ERR;
      }

      int ok = check_answer(answer_state, resp[0].resp);

      free(resp[0].resp);
      free(resp);
      if (ok) {
        goto correct_answer;
      }
    }

    // Fallthrough when all attempts are exhausted.
    free(question);
    free(answer_state);

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
    free(question);
    free(answer_state);
  }

  return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh __attribute__((unused)),
                              int flags __attribute__((unused)),
                              int argc __attribute__((unused)),
                              const char **argv __attribute__((unused)))
    __attribute__((__visibility__("default")));
PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh __attribute__((unused)),
                              int flags __attribute__((unused)),
                              int argc __attribute__((unused)),
                              const char **argv __attribute__((unused))) {
  return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh __attribute__((unused)),
                                int flags __attribute__((unused)),
                                int argc __attribute__((unused)),
                                const char **argv __attribute__((unused)))
    __attribute__((__visibility__("default")));
PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh __attribute__((unused)),
                                int flags __attribute__((unused)),
                                int argc __attribute__((unused)),
                                const char **argv __attribute__((unused))) {
  return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh,
                                   int flags __attribute__((unused)), int argc,
                                   const char **argv)
    __attribute__((__visibility__("default")));
PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh,
                                   int flags __attribute__((unused)), int argc,
                                   const char **argv) {
  int retval;

  const char *user;
  retval = pam_get_user(pamh, &user, "Username: ");
  if (retval != PAM_SUCCESS) {
    fprintf(stderr, "ERROR: could not query username: %s\n",
            pam_strerror(pamh, retval));
    return retval;
  }

  config_t *config = build_config(user, argc, argv);
  if (config == NULL) {
    fprintf(stderr, "ERROR: could not get config\n");
    return PAM_SERVICE_ERR;
  }
  int result = ask_questions(pamh, config);
  free(config);
  return result;
}
