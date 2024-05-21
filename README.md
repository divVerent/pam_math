# pam_math

A simple PAM module that asks math questions at login time.

[![Video](https://img.youtube.com/vi/d78-0j9jLWs/maxresdefault.jpg)](https://youtu.be/d78-0j9jLWs)

## WARNING

This is not a security utility. Solving basic math problems is not
sufficient authentication for a computer system, and as such, regular
authentication (e.g. via password) has to stay enabled when using this.
This module serves only as a way to have *additional* questions posed to
users.

## Prerequisites

Your system needs to be running Linux, FreeBSD, NetBSD, OpenBSD or
another operating system that uses PAM.

The login you want to modify must support multi-prompt PAM
conversations.

This includes:

-   Login managers:
    -   LightDM
    -   login
-   Screen lockers:
    -   physlock\*
    -   vlock\*
    -   XScreenSaver
    -   XSecureLock
-   Other:
    -   doas
    -   OpenSSH (via `KbdInteractiveAuthentication`)
    -   su
    -   sudo

\*: Applications where the results cannot be seen while typing them in,
but where the module otherwise works correctly.

This module is notably not compatible with:

-   Login managers:
-   Screen lockers:
    -   i3lock
    -   screen
    -   Swaylock

<!--
To be tested:

-   Entrance
-   GDM
-   LXDM
-   SDDM
-   XDM
-->

## Compiling

This module is compiled as follows:

TODO: make it an autoconf based build system?

    gcc -Wall -Wextra -Wpedantic -fPIC -shared -O3 -o pam_math.so pam_math.c

## Installing

This module needs to be installed where your distribution expects PAM
modules. For example, on Debian on x86-64, this would be:

    sudo install -m755 pam_math.so /usr/lib/x86_64-linux-gnu/security/

## Configuring

1.  Pick a PAM service to add this to.

    You can see all PAM services by `ls /etc/pam.d`.

    Typical choices will be `lightdm` or another display manager, or
    `common-auth` to apply to all types of login on your system (NOTE:
    this is dangerous as it may make it impossible to log in if a
    mistake happens in the configuration file, and may break some ways
    of logging in entirely if they do not support multi-step PAM
    conversations).

2.  Open its config file in an editor.

    This would be `/etc/pam.d/<servicename>`.

3.  Locate a place that occurs after password authentication.

    You need to locate the last line that starts with `auth` or
    `@include`s a file that has lines that start with `auth`. Typically
    this will be either at the end of the file, or a line like
    `@include common-auth`.

4.  Configure this module.

    After all existing password authentication, add a section like:

        auth required pam_math.so \
          .attempts=3 .amin=1 .amax=99 .mmin=2 .mmax=9 \
          k1.questions=3 k1.ops=+-*/

    It is important that all but the last line end with a backslash
    (`\`) so that PAM considers this all a single line.

    With this, the user `k1` (and nobody else) will be asked 3 random
    math problems using one of the four basic math operations, with a
    range from 1 to 99 for addition and 2 to 9 for multiplication
    inputs.

    <!--
    TODO:
    More complex examples can be found in the [`examples`](examples/)
    directory.
    -->

## Reference

Each argument is of the form `.field=value` or `user.field=value`,
whereas the former sets a default and the latter overrides it for that
specific user.

The following fields can be set:

| Field     | Default | Meaning                                                                                                         |
|-----------|---------|-----------------------------------------------------------------------------------------------------------------|
| questions | 3       | Number of questions to ask (set to 0 to disable).                                                               |
| attempts  | 3       | Number of attempts per question (exceeding this fails authentication).                                          |
| amin      | 0       | Minimum number to occur in additive math problems posed.                                                        |
| amax      | 10      | Maximum number to occur in additive math problems posed.                                                        |
| mmin      | 2       | Minimum number to occur in multiplicative math problems posed.                                                  |
| mmax      | 9       | Maximum number to occur in multiplicative math problems posed.                                                  |
| ops       |         | String of math operators to use in problems posed (use "+-\*/mrdq" to include all, and leave unset to disable). |

The following `ops` are available:

| Character | Operation               | Formula               | Limits                     | Notes                                         |
|-----------|-------------------------|-----------------------|----------------------------|-----------------------------------------------|
| `+`       | Addition                | `x = a + b`           | `amin ≤ a, b ≤ amax`       |                                               |
| `-`       | Subtraction             | `x = a - b`           | `amin ≤ b, x ≤ amax`       |                                               |
| `*`       | Multiplication          | `x = a \* b`          | `mmin ≤ a, b ≤ mmax`       |                                               |
| `/`       | Remainder-less division | `x = a / b`           | `mmin ≤ b, x ≤ mmax`       |                                               |
| `d`       | Division w/ remainder   | `x = ⌊a / b⌋`         | `mmin ≤ b, x ≤ mmax`       | Remainder has same sign as `b` (Python style) |
| `q`       | Division w/ remainder   | `x = [a / b]`         | `mmin ≤ b, x ≤ mmax`       | Remainder has same sign as `a` (C style)      |
| `m`       | Division remainder      | `x = a - b * ⌊a / b⌋` | `mmin ≤ b, ⌊a / b⌋ ≤ mmax` | Remainder has same sign as `b` (Python style) |
| `r`       | Division remainder      | `x = a - b * [a / b]` | `mmin ≤ b, [a / b] ≤ mmax` | Remainder has same sign as `a` (C style)      |

Note that the `min` and `max` pairs of options apply directly for
addition and multiplication problems, while for subtraction or divison
problems these limits apply to the right-hand side of the operator and
the resulting value. For example, these are all division problems that
would be asked if `.mmin=2 .mmax=4 .ops=/`:

     4 ÷ 2 = 2
     6 ÷ 2 = 3
     8 ÷ 2 = 4
     6 ÷ 3 = 2
     9 ÷ 3 = 3
    12 ÷ 3 = 4
     8 ÷ 4 = 2
    12 ÷ 4 = 3
    16 ÷ 4 = 4

## License

This project can be used under the 3-clause BSD license or the GPL; see
[`LICENSE`](LICENSE) for details.
