#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

/* Max. digits of BigN in decimal is 39. */
#define LEN_BN_STR 40

struct BigN {
    unsigned long long lower, upper;
};

/* Reverse given string */
static void reverse_string(char *str)
{
    if (!str)
        return;

    for (int i = 0; i < (strlen(str) / 2); i++) {
        char c = str[i];
        str[i] = str[strlen(str) - 1 - i];
        str[strlen(str) - 1 - i] = c;
    }
}

/* Add str_num and str_addend, and store result in str_sum.
 * digit which exceed value of "digits" will be truncated.
 * return -1 if any failed, or string length of result. */
static int string_of_number_add(char *str_sum,
                                char *str_addend,
                                unsigned int digits)
{
    int length;
    int idx = 0, carry = 0;
    char str_tmp[LEN_BN_STR] = "";

    if (!str_sum || !str_addend || (digits == 0))
        return -1;

    /* Reverse string because we process it from lowest digit. */
    reverse_string(str_sum);
    reverse_string(str_addend);

    /* Take the longer length, because we need to go through all digits. */
    length = (strlen(str_sum) > strlen(str_addend)) ? strlen(str_sum)
                                                    : strlen(str_addend);

    /* Keep calculate if both str_sum and str_addend does not meet '\0'. */
    for (int i = 0, j = 0; (i < length) && (j < length);) {
        int sum = 0;

        if (str_sum[i] != '\0') {
            sum += (str_sum[i++] - '0');
        }
        if (str_addend[j] != '\0') {
            sum += (str_addend[j++] - '0');
        }

        if (idx < digits)
            str_tmp[idx++] = ((sum + carry) % 10) + '0';

        carry = (sum + carry) / 10;
    }

    if ((idx < digits) && carry)
        str_tmp[idx++] = carry + '0';

    str_tmp[idx] = '\0';

    /* Reverse string back */
    reverse_string(str_tmp);
    reverse_string(str_addend);

    snprintf(str_sum, digits, "%s", str_tmp);

    return (int) strlen(str_sum);
}

/* Multiply multiplicand with miltiplier, and store result in multiplicand.
 * digit which exceed value of "digits" will be truncated.
 * return -1 if any failed, or string length of result. */
static int string_of_number_multiply(char *multiplicand,
                                     char *multiplier,
                                     unsigned int digits)
{
    char str_tmp[LEN_BN_STR] = "";
    char str_product[LEN_BN_STR] = "";
    int carry = 0;

    if (!multiplicand || !multiplier || (digits == 0) ||
        (strlen(multiplicand) == 0) || (strlen(multiplier) == 0))
        return -1;

    /* Sepcial case of "0" */
    if ((strlen(multiplier) == 1 && multiplier[0] == '0') ||
        (strlen(multiplicand) == 1 && multiplicand[0] == '0')) {
        snprintf(multiplicand, digits, "%s", "0");
        return (int) strlen(multiplicand);
    }

    /* Reverse string because we process it from lowest digit. */
    reverse_string(multiplicand);
    reverse_string(multiplier);

    for (int i = 0; i < strlen(multiplier); i++) {
        int idx = 0;

        for (int decuple = i; (idx < digits) && (decuple > 0); decuple--)
            str_tmp[idx++] = '0';

        for (int j = 0; j < strlen(multiplicand); j++) {
            int product = (multiplicand[j] - '0') * (multiplier[i] - '0');
            if (idx < digits)
                str_tmp[idx++] = ((product + carry) % 10) + '0';
            carry = (product + carry) / 10;
        }

        if ((idx < digits) && carry) {
            str_tmp[idx++] = carry + '0';
            carry = 0;
        }
        str_tmp[idx] = '\0';

        reverse_string(str_tmp);
        string_of_number_add(str_product, str_tmp, sizeof(str_product) - 1);
    }

    /* Reverse back */
    reverse_string(multiplier);
    snprintf(multiplicand, digits, "%s", str_product);

    return (int) strlen(multiplicand);
}

/* Convert BigN to string which represent in decimal */
static void print_BigN_string(struct BigN fib_seq,
                              char *bn_out,
                              unsigned int digits)
{
    char bn_scale[LEN_BN_STR] = "18446744073709551616";
    char bn_lower[LEN_BN_STR] = "";
    char bn_upper[LEN_BN_STR] = "";

    if (!bn_out)
        return;

    snprintf(bn_out, digits, "%d", 0);
    snprintf(bn_lower, sizeof(bn_lower), "%llu", fib_seq.lower);
    snprintf(bn_upper, sizeof(bn_upper), "%llu", fib_seq.upper);

    string_of_number_multiply(bn_scale, bn_upper, sizeof(bn_scale));
    string_of_number_add(bn_out, bn_scale, digits);
    string_of_number_add(bn_out, bn_lower, digits);
}

int main()
{
    long long sz;

    struct BigN bn_buf;
    char fib_str_buf[LEN_BN_STR] = "";
    char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        if ((sz = read(fd, &bn_buf, sizeof(bn_buf))) < 0) {
            printf("Reading failed at offset %d\n", i);
        } else {
            print_BigN_string(bn_buf, fib_str_buf, sizeof(fib_str_buf));
            printf("Reading from " FIB_DEV
                   " at offset %d, returned the sequence "
                   "%s.\n",
                   i, fib_str_buf);
        }
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        if ((sz = read(fd, &bn_buf, sizeof(bn_buf))) < 0) {
            printf("Reading failed at offset %d\n", i);
        } else {
            print_BigN_string(bn_buf, fib_str_buf, sizeof(fib_str_buf));
            printf("Reading from " FIB_DEV
                   " at offset %d, returned the sequence "
                   "%s.\n",
                   i, fib_str_buf);
        }
    }

    close(fd);
    return 0;
}
