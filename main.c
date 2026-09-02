#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>

enum ansi_state { ANSI_NONE, ANSI_ESC, ANSI_CSI_PARAMS, ANSI_CSI_INTERMEDIATE };

struct ansi_parser {
  enum ansi_state state;
};

static int ansi_feed(struct ansi_parser *p, unsigned char c) {
  switch (p->state) {
  case ANSI_NONE:
    if (c == 0x1b) {
      p->state = ANSI_ESC;
      return 1;
    }
    return 0;

  case ANSI_ESC:
    if (c == '[') {
      p->state = ANSI_CSI_PARAMS;
      return 1;
    }

    /*
     * Python:
     *   \x1b[@-Z\\-_]
     *
     * This is @ through Z, or \ through _.
     */
    if ((c >= '@' && c <= 'Z') || (c >= '\\' && c <= '_')) {
      p->state = ANSI_NONE;
      return 1;
    }

    /*
     * Not a valid ANSI sequence.
     * Treat this byte normally.
     */
    p->state = ANSI_NONE;
    return 0;

  case ANSI_CSI_PARAMS:
    if (c >= '0' && c <= '?')
      return 1;

    if (c >= ' ' && c <= '/') {
      p->state = ANSI_CSI_INTERMEDIATE;
      return 1;
    }

    if (c >= '@' && c <= '~') {
      p->state = ANSI_NONE;
      return 1;
    }

    p->state = ANSI_NONE;
    return 0;

  case ANSI_CSI_INTERMEDIATE:
    if (c >= ' ' && c <= '/')
      return 1;

    if (c >= '@' && c <= '~') {
      p->state = ANSI_NONE;
      return 1;
    }

    p->state = ANSI_NONE;
    return 0;
  }

  return 0;
}

static FILE *rng;

static int random_init(void) {
  rng = fopen("/dev/urandom", "rb");
  return rng != NULL;
}

static uint32_t random_u32(void) {
  uint32_t x;

  if (fread(&x, sizeof(x), 1, rng) != 1)
    abort();

  return x;
}

static double random_uniform(double min, double max) {
  uint32_t x = random_u32();
  double r = (double)x / ((double)UINT32_MAX + 1.0);

  return min + r * (max - min);
}

static void sleep_seconds(double seconds) {
  struct timespec ts;

  ts.tv_sec = (time_t)seconds;
  ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1000000000.0);

  thrd_sleep(&ts, NULL);
}

static void delay_random(double min, double max) {
  sleep_seconds(random_uniform(min, max));
}

static int is_sentence_end(unsigned char c) {
  return c == '.' || c == '!' || c == '?';
}

static int is_clause_pause(unsigned char c) {
  return c == ',' || c == ';' || c == ':';
}

static int is_symbol(unsigned char c) {
  /*
   * Python:
   *
   * set("1234567890-=[]\\\\;\\',./ ~!@#\\$%^&*()_+{}|:\"<>?\n")
   */
  return strchr("1234567890-=[]\\;',./ ~!@#$%^&*()_+{}|:\"<>?\n", c) != NULL;
}

int main(void) {
  struct ansi_parser parser = {.state = ANSI_NONE};

  random_init();

  unsigned long word_count = 0;
  unsigned long next_pause_at = 1 + random_u32() % 5; // SIGSEGV

  /*
   * We only need to distinguish:
   *
   *   previous ASCII character
   *   previous non-ASCII character
   *   previous ANSI escape sequence
   *
   * Repeat detection is only relevant for ASCII characters.
   */
  int prev_is_ascii = 0;
  unsigned char prev_c = 0;

  int ch;

  while ((ch = fgetc(stdin)) != EOF) {
    unsigned char c = (unsigned char)ch;

    putchar(c);
    fflush(stdout);

    /*
     * Feed the byte into the ANSI parser.
     *
     * Any byte belonging to an ANSI escape sequence gets no delay.
     */
    // int was_ansi = (parser.state != ANSI_NONE);

    if (ansi_feed(&parser, c)) {
      /*
       * ESC itself starts an ANSI token, and all bytes while
       * the parser is active are part of that token.
       */
      prev_is_ascii = 0;
      continue;
    }

    /*
     * A byte >= 0x80 is part of a UTF-8/non-ASCII character.
     *
     * Python's token isascii() would be false, so there is no delay.
     */
    if (c >= 0x80) {
      prev_is_ascii = 0;
      continue;
    }

    /*
     * ASCII character from here on.
     */
    int is_repeat = prev_is_ascii && c == prev_c;

    if ((c == ' ' || c == '\n')) {
      /*
       * Python:
       *
       * token in (" ", "\n") and prev_ch not in (" ", "\n")
       */
      int previous_was_space =
          prev_is_ascii && (prev_c == ' ' || prev_c == '\n');

      if (!previous_was_space) {
        word_count++;

        if (word_count >= next_pause_at) {
          delay_random(0.5, 1.0);

          word_count = 0;
          next_pause_at = 1 + random_u32() % 5;
        } else {
          delay_random(0.04, 0.14);
        }
      }
    } else if (is_sentence_end(c)) {
      delay_random(0.4, 0.9);
    } else if (is_clause_pause(c)) {
      delay_random(0.2, 0.5);
    } else if (is_symbol(c)) {
      if (is_repeat)
        delay_random(0.05, 0.15);
      else
        delay_random(0.1, 0.3);
    } else {
      if (is_repeat)
        delay_random(0.01, 0.05);
      else
        delay_random(0.03, 0.10);
    }

    prev_is_ascii = 1;
    prev_c = c;
  }

  return 0;
}
