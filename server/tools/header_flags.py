"""Select instrument preprocessor flags for the host metadata compiler."""
import shlex
import sys


def header_flags(command):
    words = iter(shlex.split(command))
    selected = []
    for word in words:
        if word.startswith(('-I', '-D', '-U')):
            selected.append(word)
            if word in ('-I', '-D', '-U'):
                argument = next(words, None)
                if argument is not None:
                    selected.append(argument)
    return shlex.join(selected)


if __name__ == '__main__':
    print(header_flags(sys.argv[1]))
