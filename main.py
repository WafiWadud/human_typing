import random
import re
import sys
import time

sentence_end: set[str] = set(".!?")
clause_pause: set[str] = set(",;:")
symbols: set[str] = set("1234567890-=[]\\\\;\\',./ ~!@#\\$%^&*()_+{}|:\"<>?\n")

word_count: int = 0
next_pause_at: int = random.randint(1, 5)
prev_ch: str | None = None
ANSI_ESCAPE = re.compile(r"\x1b(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])")


def split_with_escapes(text: str) -> list[str]:
    """Split text into a list of individual chars and whole ANSI escape sequences."""
    tokens = []
    last = 0
    for m in ANSI_ESCAPE.finditer(text):
        for ch in text[last : m.start()]:
            tokens.append(ch)
        tokens.append(m.group())
        last = m.end()
    for ch in text[last:]:
        tokens.append(ch)
    return tokens


for token in split_with_escapes(sys.stdin.read()):
    sys.stdout.write(token)
    sys.stdout.flush()

    is_escape: bool = len(token) > 1
    is_repeat: bool = token == prev_ch

    if not token.isascii() or is_escape:
        pass  # no delay for escape sequences
    elif token in (" ", "\n") and prev_ch not in (" ", "\n"):
        word_count += 1
        if word_count >= next_pause_at:
            time.sleep(random.uniform(0.5, 1.0))
            word_count = 0
            next_pause_at = random.randint(1, 5)
        else:
            time.sleep(random.uniform(0.04, 0.14))
    elif token in sentence_end:
        time.sleep(random.uniform(0.4, 0.9))
    elif token in clause_pause:
        time.sleep(random.uniform(0.2, 0.5))
    elif token in symbols:
        time.sleep(
            random.uniform(0.05, 0.15) if is_repeat else random.uniform(0.1, 0.3)
        )
    else:
        time.sleep(
            random.uniform(0.01, 0.05) if is_repeat else random.uniform(0.03, 0.10)
        )

    prev_ch = token
