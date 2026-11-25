#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/io.h>
#include <unistd.h>

#define HWM_BASE 0x800
#define HWM_PAGE 0x8ff
#define HWM_HOST_CONTROL 0x0004
#define LOCK_PATH "/run/lock/z440-hwmio.lock"
#define DMI_PRODUCT_PATH "/sys/class/dmi/id/product_name"

static int lock_fd = -1;

static void die(const char *message)
{
    fprintf(stderr, "z440-hwmio: %s: %s\n", message, strerror(errno));
    exit(EXIT_FAILURE);
}

static int file_contains(const char *path, const char *needle)
{
    char buffer[256];
    FILE *file = fopen(path, "r");

    if (file == NULL)
        return 0;
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        fclose(file);
        return 0;
    }
    fclose(file);
    return strstr(buffer, needle) != NULL;
}

static void require_z440(void)
{
    const char *override = getenv("Z440_HWMIO_UNSAFE_ALLOW_OTHER_PLATFORM");

    if (file_contains(DMI_PRODUCT_PATH, "Z440"))
        return;
    if (override != NULL && strcmp(override, "YES-I-ACCEPT-THE-RISK") == 0) {
        fprintf(stderr, "z440-hwmio: WARNING: bypassing the HP Z440 DMI guard\n");
        return;
    }
    fprintf(stderr,
            "z440-hwmio: refusing raw I/O because DMI does not identify an HP Z440\n"
            "z440-hwmio: see docs/PORTING.md; never copy Z440 writes blindly\n");
    exit(EXIT_FAILURE);
}

static unsigned long parse_number(const char *text, unsigned long maximum)
{
    char *end = NULL;
    unsigned long value;

    errno = 0;
    value = strtoul(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || value > maximum) {
        fprintf(stderr, "z440-hwmio: invalid number: %s\n", text);
        exit(EXIT_FAILURE);
    }
    return value;
}

static void select_page(uint8_t page)
{
    outb(page & 0x0f, HWM_PAGE);
}

static uint8_t read_register(uint16_t reg)
{
    uint8_t value;

    select_page((uint8_t)(reg >> 8));
    value = inb(HWM_BASE + (reg & 0xff));
    if (value == 0xff)
        value = inb(HWM_BASE + (reg & 0xff));
    return value;
}

/* Exact byte-write protocol recovered from HP's HhmDxe firmware module. */
static uint8_t write_register(uint16_t reg, uint8_t value)
{
    uint8_t saved_host_control;

    saved_host_control = read_register(HWM_HOST_CONTROL);
    select_page(0);
    outb(0, HWM_BASE + 4);
    select_page((uint8_t)(reg >> 8));
    outb(value, HWM_BASE + (reg & 0xff));
    select_page(0);
    outb(saved_host_control, HWM_BASE + 4);
    return saved_host_control;
}

static uint16_t read_word(uint16_t reg)
{
    uint16_t low = read_register(reg);
    uint16_t high = read_register(reg + 1);
    return (uint16_t)(low | (high << 8));
}

static void print_snapshot(void)
{
    static const uint16_t pwm_regs[] = {0x0198, 0x019a, 0x019b, 0x019c, 0x019d};
    static const uint16_t tach_regs[] = {0x0064, 0x0066, 0x006a, 0x006e, 0x0072};
    size_t i;

    printf("host_control=0x%02x\n", read_register(HWM_HOST_CONTROL));
    for (i = 0; i < sizeof(pwm_regs) / sizeof(pwm_regs[0]); i++)
        printf("reg_0x%04x=0x%02x\n", pwm_regs[i], read_register(pwm_regs[i]));
    for (i = 0; i < sizeof(tach_regs) / sizeof(tach_regs[0]); i++)
        printf("tach_0x%04x=%u\n", tach_regs[i], read_word(tach_regs[i]));
    select_page(0);
}

static int is_z440_fan_register(uint16_t reg)
{
    return reg == 0x0198 || reg == 0x019a || reg == 0x019b ||
           reg == 0x019c || reg == 0x019d;
}

static int is_valid_fan_value(uint16_t reg, uint8_t value)
{
    static const uint8_t standard_values[] = {0x0e, 0x25, 0x37, 0x49, 0x5b, 0x6c, 0x7e};
    static const uint8_t high_range_values[] = {0x15, 0x2c, 0x48, 0x64, 0x7f, 0x9b, 0xb7};
    static const uint8_t rear_b_values[] = {0x0e, 0x23, 0x35, 0x47, 0x5a, 0x6c, 0x7e};
    const uint8_t *values = standard_values;
    size_t count = sizeof(standard_values) / sizeof(standard_values[0]);
    size_t i;

    if (reg == 0x019a || reg == 0x019b) {
        values = high_range_values;
        count = sizeof(high_range_values) / sizeof(high_range_values[0]);
    } else if (reg == 0x019c) {
        values = rear_b_values;
        count = sizeof(rear_b_values) / sizeof(rear_b_values[0]);
    }
    for (i = 0; i < count; i++) {
        if (values[i] == value)
            return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    uint16_t reg;
    uint8_t value;
    uint8_t before;
    uint8_t after;
    uint8_t saved_host_control;

    if (geteuid() != 0) {
        fprintf(stderr, "z440-hwmio: must run as root\n");
        return EXIT_FAILURE;
    }
    require_z440();

    lock_fd = open(LOCK_PATH, O_CREAT | O_CLOEXEC | O_RDWR, 0600);
    if (lock_fd < 0)
        die("open lock");
    if (flock(lock_fd, LOCK_EX) < 0)
        die("lock");
    if (iopl(3) < 0)
        die("iopl");

    if (argc == 2 && strcmp(argv[1], "snapshot") == 0) {
        print_snapshot();
    } else if (argc == 3 && strcmp(argv[1], "read") == 0) {
        reg = (uint16_t)parse_number(argv[2], 0x0fff);
        printf("0x%02x\n", read_register(reg));
        select_page(0);
    } else if (argc == 3 && strcmp(argv[1], "read-word") == 0) {
        reg = (uint16_t)parse_number(argv[2], 0x0ffe);
        printf("%u\n", read_word(reg));
        select_page(0);
    } else if (argc == 4 && strcmp(argv[1], "write-fan") == 0) {
        reg = (uint16_t)parse_number(argv[2], 0x0fff);
        value = (uint8_t)parse_number(argv[3], 0xff);
        if (!is_z440_fan_register(reg)) {
            fprintf(stderr, "z440-hwmio: write denied for non-fan register 0x%04x\n", reg);
            return EXIT_FAILURE;
        }
        if (!is_valid_fan_value(reg, value)) {
            fprintf(stderr,
                    "z440-hwmio: value 0x%02x is not an HP-validated preset for register 0x%04x\n",
                    value, reg);
            return EXIT_FAILURE;
        }
        before = read_register(reg);
        saved_host_control = write_register(reg, value);
        after = read_register(reg);
        select_page(0);
        printf("reg=0x%04x before=0x%02x requested=0x%02x after=0x%02x host_control=0x%02x\n",
               reg, before, value, after, saved_host_control);
        if (after != value)
            return 2;
    } else {
        fprintf(stderr,
                "usage: %s snapshot | read REGISTER | read-word REGISTER | write-fan REGISTER VALUE\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    if (iopl(0) < 0)
        die("iopl reset");
    return EXIT_SUCCESS;
}
