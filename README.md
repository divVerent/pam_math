# pam_math

A simple PAM module that asks math questions at login time.

[![Video](https://img.youtube.com/vi/d78-0j9jLWs/sddefault.jpg)](https://youtu.be/d78-0j9jLWs)

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

- Login managers:
  - GDM
  - LightDM
  - login
  - XDM (requires `.use_utf8=no`)
- Screen lockers:
  - physlock (won't see numbers as they are entered, but will work)
  - Plasma's screen lock (will however need to backspace away previous
    answers before typing new ones)
  - vlock (won't see numbers as they are entered, but will work)
  - XScreenSaver
  - XSecureLock
- Other:
  - doas
  - OpenSSH (via `KbdInteractiveAuthentication`)
  - su
  - sudo

This module is notably not compatible with programs that do not
implement the full PAM conversation, such as:

- Login managers:
  - Entrance
  - LXDM
  - SDDM
- Screen lockers:
  - i3lock
  - screen (however other screen functionality is working fine, so
    recommending to put `bind x` and `bind ^X` in your `.screenrc` to
    prevent the `lockscreen` command from being invoked by accident)
  - slock
  - Swaylock

In addition the login prompt of macOS is not supported.

## Compiling

You need `make` and a C compiler installed (either `gcc` or `clang` will
do), as well as the PAM headers (they will be in a package named
`libpam-dev`, `pam-dev`, `libpam-devel` or `pam-devel`), and then can
compile it as follows:

    make

## Installing

This module needs to be installed where your distribution expects PAM
modules.

It normally autodetects the location; run

    make -n install

to see where it will install, and then install there using

    sudo make install

or specify an exact directory by

    sudo make install PAM_LIBRARY_PATH=/your/path/to/pam/libraries

## Configuring

1.  Ensure login manager compatibility.

    If your distribution uses one of the incompatible login managers
    listed above, please migrate to a supported one first. For example,
    on Kubuntu, run `sudo apt-get install lightdm` and when prompted set
    LightDM to be the default display manager.

2.  Pick a PAM service to add this to.

    You can see all PAM services by `ls /etc/pam.d`.

    Typical choices will be `lightdm` or another display manager, or
    `common-auth` to apply to all types of login on your system (NOTE:
    this is dangerous as it may make it impossible to log in if a
    mistake happens in the configuration file, and may break some ways
    of logging in entirely if they do not support multi-step PAM
    conversations).

3.  Open its config file in an editor.

    This would be `/etc/pam.d/<servicename>`.

4.  Locate a place that occurs after password authentication.

    You need to locate the last line that starts with `auth` or
    `@include`s a file that has lines that start with `auth`. Typically
    this will be either at the end of the file, or a line like
    `@include common-auth`.

5.  Configure this module.

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

    More complex examples can be found in the [`examples`](examples/)
    directory.

## Reference

Each argument is of the form `.field=value` or `user.field=value`,
whereas the former sets a default and the latter overrides it for that
specific user.

The following fields can be set:

| Field       | Default | Meaning                                                                                                        |
|-------------|---------|----------------------------------------------------------------------------------------------------------------|
| `questions` | `3`     | Number of questions to ask (set to 0 to disable).                                                              |
| `attempts`  | `3`     | Number of attempts per question (exceeding this fails authentication).                                         |
| `amin`      | `0`     | Minimum number to occur in additive math problems posed.                                                       |
| `amax`      | `10`    | Maximum number to occur in additive math problems posed.                                                       |
| `mmin`      | `2`     | Minimum number to occur in multiplicative math problems posed.                                                 |
| `mmax`      | `9`     | Maximum number to occur in multiplicative math problems posed.                                                 |
| `ops`       |         | String of math operators to use in problems posed (use `+-*/dqmr` to include all, and leave unset to disable). |
| `use_utf8`  | `auto`  | `no` to disable UTF-8 support, `yes` to enable, `auto` to detect by locale                                     |

The following `ops` are available:

| Character | Operation               | Formula               | Limits                     | Notes                                         |
|-----------|-------------------------|-----------------------|----------------------------|-----------------------------------------------|
| `+`       | Addition                | `x = a + b`           | `amin ≤ a, b ≤ amax`       |                                               |
| `-`       | Subtraction             | `x = a - b`           | `amin ≤ b, x ≤ amax`       |                                               |
| `*`       | Multiplication          | `x = a * b`           | `mmin ≤ a, b ≤ mmax`       |                                               |
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

## Extension: Arbitrary Questions

This repository also contains a second module `pam_questions_file.so`
that asks random questions from a file.

Its configuration section looks like:

            auth required pam_questions_file.so \
              .attempts=3 .file=/usr/lib/pam_math/questions.csv \
              k1.questions=3 k1.match=capitals

It supports the following fields:

| Field         | Default                           | Meaning                                                                                                    |
|---------------|-----------------------------------|------------------------------------------------------------------------------------------------------------|
| `questions`   | `3`                               | Number of questions to ask (set to 0 to disable).                                                          |
| `attempts`    | `3`                               | Number of attempts per question (exceeding this fails authentication).                                     |
| `file`        | `/usr/lib/pam_math/questions.csv` | Path to a CSV file with questions.                                                                         |
| `ignore_case` | `0`                               | If set to 1, answers are case insensitive.                                                                 |
| `match`       | \`\`                              | If set, a full-match regular expression for the CSV file's `match` column to select a subset of questions. |

The questions file is a CSV that must contain a column with the exact
name `question`, and another column with the exact name `answer`. If a
column with the exact name `match` exists, it can be used to filter
questions from the file using the `match` option. See
`examples/questions.csv` for how it should look. Every question in the
file should normally end with a question mark (`?`) or a colon (`:`) to
ensure a useful prompt is shown to the user.

Do note that the questions file must be accessible by the user running
the login screen, and as such, if this is to be enabled for e.g. a
screen saver lock, then the question file must be readable by every
affected user. There is no way to prevent the user from e.g. printing
out the question file and using that as a help.

## License

This project can be used under the 3-clause BSD license or the GPL; see
[`LICENSE`](LICENSE) for details.
