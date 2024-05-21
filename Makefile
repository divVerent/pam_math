CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -fPIC -O3
LDFLAGS = -fPIC -shared -lpam
PAM_LIBRARY_PATH = $(shell ./detect_pam_library_path.sh)

all: pam_math.so

test: pam_math.so
	./test.sh

install: pam_math.so
	install -m755 pam_math.so $(DESTDIR)$(PAM_LIBRARY_PATH)/

clean:
	$(RM) *.o *.so

pam_math.so: pam_math.o
	$(CC) $(LDFLAGS) -o $@ $<

pam_math.o: pam_math.c asprintf.h math_questions.h
	$(CC) $(CFLAGS) -c -o $@ $<