# Threads test
import threading

import magic

MAX_BYTES = 4096


def thread_function():
    with open("/tmp/foo.csv", "rb") as bytes_file:
        print(magic.detect_from_content(
            bytes_file.read(MAX_BYTES)
        ).encoding)


if __name__ == '__main__':
    threads = list()
    for index in range(3):
        thread = threading.Thread(target=thread_function)
        threads.append(thread)
        thread.start()

    for index, thread in enumerate(threads):
        thread.join()
