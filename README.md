\#pam_math

A simple PAM module that asks math questions at login time.

## Compiling

This module is compiled as follows:

TODO: make it an autoconf based build system?

    gcc -Wall -Wextra -Wpedantic -fPIC -shared -O3 -o pam_math.so pam_math.c

## Installing

This module needs to be installed where your distribution expects PAM
modules. For example, on Debian on x86-64, this would be:

    sudo install -m755 pam_math.so /usr/lib/x86_64-linux-gnu/security/

## Configuring

This module is configured entirely in `/etc/pam.d/common-auth` as
follows (a good bet would be putting this at the end of the file):

    auth required pam_math.so \
      .attempts=3 .amin=2 .amax=9 .mmin=2 .mmax=9 \
      k1.questions=3 k1.ops=+-*/ \
      k1g.questions=5 k1g.mmax=15 k1g.ops=*/ \
      k2s.questions=3 k2s.ops=+-* \
      k2.questions=5 k2.ops=* \
      k3s.questions=1 k3s.ops=+- \
      k3.questions=2 k3.mmax=5 k3.ops=* \
      rpolzer.questions=1 rpolzer.max=19 rpolzer.ops=+-*/

Each command-line argument is of the form `.field=value` or
`user.field=value`, whereas the former sets a default and the latter
overrides it for that specific user.

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

| Character | Operation               | Formula               | Limits               | Notes                                         |
|-----------|-------------------------|-----------------------|----------------------|-----------------------------------------------|
| `+`       | Addition                | `x = a + b`           | `amin ≤ a, b ≤ amax` |                                               |
| `-`       | Subtraction             | `x = a - b`           | `amin ≤ b, x ≤ amax` |                                               |
| `*`       | Multiplication          | `x = a \* b`          | `mmin ≤ a, b ≤ mmax` |                                               |
| `/`       | Remainder-less division | `x = a / b`           | `mmin ≤ b, x ≤ mmax` |                                               |
| `d`       | Division w/ remainder   | `x = ⌊a / b⌋`         | `mmin ≤ b, x ≤ mmax` | Remainder has same sign as `b` (Python style) |
| `q`       | Division w/ remainder   | `x = [a / b]`         | `mmin ≤ b, x ≤ mmax` | Remainder has same sign as `a` (C style)      |
| `m`       | Division remainder      | `x = a - b * ⌊a / b⌋` | `mmin ≤ b, x ≤ mmax` | Remainder has same sign as `b` (Python style) |
| `r`       | Division remainder      | `x = a - b * [a / b]` | `mmin ≤ b, x ≤ mmax` | Remainder has same sign as `a` (C style)      |

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
