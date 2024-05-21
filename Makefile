CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -fPIC -O3
LDFLAGS = -fPIC -shared -lpam
PAM_LIBRARY_PATH = $(shell ./detect_pam_library_path.sh)

.PHONY: all
all: pam_math.so

.PHONY: test
test: pam_math.so
	./test.sh

.PHONY: install
install: pam_math.so
	install -m755 pam_math.so $(DESTDIR)$(PAM_LIBRARY_PATH)/

.PHONY: clean
clean:
	$(RM) *.o *.so

.PHONY: iwyu
iwyu:
	for x in *.[ch]; do \
		iwyu $$x; \
	done

.PHONY: clang-format
clang-format:
	clang-format -i *.[ch]

pam_math.so: pam_module.o asprintf.o math_questions.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c $(wildcard *.h)
	$(CC) $(CFLAGS) -c -o $@ $<
