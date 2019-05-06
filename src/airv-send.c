#include <err.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    if (argc != 2)
        errx(1, "USAGE: %s /dev/input/event*", argv[0]);

    int device = open(argv[1], O_RDONLY);
    if (device < 0)
        err(1, "Failed to open %s", argv[1]);

    struct input_event event;
    while (1) {
        if (read(device, &event, sizeof(event)) < sizeof(event))
            err(1, "Failed to read input event from %s", argv[1]);
        if (write(1, &event, sizeof(event)) < sizeof(event))
            err(1, "Failed to write input event to stdout");
    }
}

