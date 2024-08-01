CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -O3
LDFLAGS ?=

LDLIBS = -lpam -lm
PAM_LIBRARY_PATH = $(shell ./detect_pam_library_path.sh)

# Make a library.
CFLAGS_LIB = -fPIC -fvisibility=hidden
LDFLAGS_LIB = -fPIC -shared

# Enable LTO.
LD = $(CC)
CFLAGS_LIB += -flto
LDFLAGS_LIB += $(CFLAGS) $(CFLAGS_LIB)

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
		iwyu -Xiwyu --mapping_file=iwyu.imp -Xiwyu --update_comments $$x; \
	done

.PHONY: clang-format
clang-format:
	clang-format -i *.[ch]

pam_math.so: pam_module.o helpers.o math_questions.o
	$(LD) $(LDFLAGS) $(LDFLAGS_LIB) -o $@ $^ $(LDLIBS)

%.o: %.c $(wildcard *.h)
	$(CC) $(CFLAGS) $(CFLAGS_LIB) -c -o $@ $<
