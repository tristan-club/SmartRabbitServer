#include <string.h>
#include <stdlib.h>
#include "base64.h"
static const char base64_table[] =
        { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
          'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
          'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
          'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
          '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', '\0'
        };

static const char base64_pad = '=';

static const short base64_reverse_table[256] = {
        -2, -2, -2, -2, -2, -2, -2, -2, -2, -1, -1, -2, -2, -1, -2, -2,
        -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
        -1, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, 62, -2, -2, -2, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -2, -2, -2, -2, -2, -2,
        -2,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -2, -2, -2, -2, -2,
        -2, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -2, -2, -2, -2, -2,
        -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
        -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
        -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
        -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
        -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
        -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
        -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
        -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2
};

unsigned char *base64_decode(const unsigned char *str, int length, int *ret_length)
{
        const unsigned char *current = str;

        int ch, i = 0, j = 0, k;

        unsigned char *result;

        result = (unsigned char *)malloc(length + 1);

        while ((ch = *current++) != '\0' && length-- > 0) {
                if (ch == base64_pad) {
                        if (*current != '=' && (i % 4) == 1) {
                                free(result);
                                return NULL;
                        }
                        continue;
                }

                ch = base64_reverse_table[ch];
                if (( ch < 0) || ch == -1) { /* a space or some other separator character, we simply skip over */
                        continue;
                } else if (ch == -2) {
                        free(result);
                        return NULL;
                }

                switch(i % 4) {
                case 0:
                        result[j] = ch << 2;
                        break;
                case 1:
                        result[j++] |= ch >> 4;
                        result[j] = (ch & 0x0f) << 4;
                        break;
                case 2:
                        result[j++] |= ch >>2;
                        result[j] = (ch & 0x03) << 6;
                        break;
                case 3:
                        result[j++] |= ch;
                        break;
                }
                i++;
        }

        k = j;
        /* mop things up if we ended on a boundary */
        if (ch == base64_pad) {
                switch(i % 4) {
                case 1:
                        free(result);
                        return NULL;
                case 2:
                        k++;
                case 3:
                        result[k++] = 0;
                }
	}
        if(ret_length) {
                *ret_length = j;
        }
        result[j] = '\0';
        return result;
}
