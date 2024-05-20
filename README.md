# pam_math

A simple PAM module that asks math questions at login time.

## Compiling

This module is compiled as follows:

    gcc -Wall -fPIC -shared -o pam_math.so pam_math.c

## Installing

    sudo install -m755 pam_math.so /usr/lib/x86_64-linux-gnu/security/

## Configuring

This module is configured entirely in `/etc/pam.d/common-auth` as
follows:

    auth required pam_math.so \
      .attempts=3 .min=2 .max=9 \
      k1.questions=3 k1.ops=+-*/ \
      k1g.questions=5 k1g.max=15 k1g.ops=*/ \
      k2s.questions=3 k2s.ops=+-* \
      k2.questions=5 k2.ops=* \
      k3s.questions=1 k3s.ops=+- \
      k3.questions=2 k3.max=5 k3.ops=* \
      rpolzer.questions=1 rpolzer.max=19 rpolzer.ops=+-*/

Each command-line argument is of the form `.field=value` or
`user.field=value`, whereas the former sets a default and the latter
overrides it for that specific user.

The following fields can be set:

| Field     | Default | Meaning                                                                                                     |
|-----------|---------|---------------------------------------------------------------------------------------------------------------|
| questions | 3       | Number of questions to ask (set to 0 to disable).                                                             |
| attempts  | 3       | Number of attempts per question (exceeding this fails authentication).                                        |
| amin      | 0       | Minimum number to occur in additive math problems posed.                                                      |
| amax      | 10      | Maximum number to occur in additive math problems posed.                                                      |
| mmin      | 2       | Minimum number to occur in multiplicative math problems posed.                                                |
| mmax      | 9       | Maximum number to occur in multiplicative math problems posed.                                                |
| ops       |         | String of math operators to use in problems posed (use "+-\*/mr" to include all, and leave unset to disable). |

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

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for details.

## License

This project can be used under the 3-clause BSD license or the GPL; see
[`LICENSE`](LICENSE) for details.
