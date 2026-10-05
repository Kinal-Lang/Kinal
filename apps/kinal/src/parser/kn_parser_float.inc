/* Decimal literals are rounded once, to nearest binary64 with ties to even.
 * Do not use strtod here: the frontend is also a no-CRT host library, and the
 * source language's decimal separator must not depend on the process locale.
 * The AST stores binary64 even when a later cast requests f16/f32/f128.
 */
typedef struct
{
    unsigned char digits[1075];
    size_t length;
    int sticky;
} KnFloatFraction;

static double float_literal_bits(uint64_t bits)
{
    double value;
    kn_memcpy(&value, &bits, sizeof(value));
    return value;
}

/* Multiply the exact decimal fraction by two, returning its integer bit. */
static int float_fraction_bit(KnFloatFraction *fraction)
{
    unsigned carry = 0;
    for (size_t i = fraction->length; i > 0; i--)
    {
        unsigned digit = fraction->digits[i - 1] * 2u + carry;
        fraction->digits[i - 1] = (unsigned char)(digit % 10u);
        carry = digit / 10u;
    }
    while (fraction->length && !fraction->digits[fraction->length - 1])
        fraction->length--;
    return (int)carry;
}

static int float_integer_bit(const uint32_t *integer, int bit)
{
    return (int)((integer[bit / 32] >> (bit % 32)) & 1u);
}

static double parse_float_decimal(const char *text, size_t length)
{
    if (length && (text[length - 1] == 'f' || text[length - 1] == 'F'))
        length--;
    size_t point = 0;
    while (point < length && text[point] != '.') point++;
    size_t first = 0;
    while (first < point && text[first] == '0') first++;
    /* 10^309 already exceeds binary64's finite range. */
    if (point - first > 309)
        return float_literal_bits(UINT64_C(0x7ff0000000000000));

    /* At most 309 significant integer digits need 1027 bits. */
    uint32_t integer[33] = {0};
    size_t words = 0;
    for (size_t i = first; i < point; i++)
    {
        uint64_t carry = (unsigned)(text[i] - '0');
        for (size_t j = 0; j < words; j++)
        {
            carry += (uint64_t)integer[j] * 10u;
            integer[j] = (uint32_t)carry;
            carry >>= 32;
        }
        if (carry) integer[words++] = (uint32_t)carry;
    }

    KnFloatFraction fraction;
    fraction.length = 0;
    fraction.sticky = 0;
    if (point < length)
    {
        for (size_t i = point + 1; i < length; i++)
        {
            unsigned char digit = (unsigned char)(text[i] - '0');
            if (fraction.length < sizeof(fraction.digits))
                fraction.digits[fraction.length++] = digit;
            else if (digit)
                fraction.sticky = 1;
        }
    }
    /* Every binary64 rounding boundary is a multiple of 2^-1075, hence
     * terminates within 1075 decimal places. Beyond that, only whether any
     * digit is nonzero can affect a tie. Work and storage stay bounded even
     * for very long literals; scanning their source is still linear. */
    while (fraction.length && !fraction.digits[fraction.length - 1])
        fraction.length--;

    int exponent = -1;
    if (words)
    {
        exponent = (int)(words - 1) * 32;
        uint32_t top = integer[words - 1];
        while (top >>= 1) exponent++;
        if (exponent > 1023)
            return float_literal_bits(UINT64_C(0x7ff0000000000000));
    }
    else
    {
        if (!fraction.length) return 0.0;
        /* Include the half-minimum-subnormal bit, for correct underflow ties. */
        while (exponent >= -1075 && !float_fraction_bit(&fraction)) exponent--;
        if (exponent < -1075) return 0.0;
    }

    int precision = exponent < -1022 ? exponent + 1075 : 53;
    uint64_t significand = precision ? 1 : 0;
    int bit = exponent - 1;
    for (int i = 1; i < precision; i++, bit--)
    {
        if (bit < 0 && !fraction.length)
        {
            significand <<= precision - i;
            bit -= precision - i;
            break;
        }
        int next = bit >= 0 ? float_integer_bit(integer, bit) : float_fraction_bit(&fraction);
        significand = (significand << 1) | (unsigned)next;
    }
    int guard = precision ? (bit >= 0 ? float_integer_bit(integer, bit) : float_fraction_bit(&fraction)) : 1;
    int sticky = fraction.length != 0 || fraction.sticky;
    for (int i = 0; i < bit && !sticky; i++)
        sticky = float_integer_bit(integer, i);
    if (guard && (sticky || (significand & 1))) significand++;

    if (exponent < -1022)
        return float_literal_bits(significand);
    if (significand == (UINT64_C(1) << 53))
    {
        significand >>= 1;
        exponent++;
    }
    if (exponent > 1023)
        return float_literal_bits(UINT64_C(0x7ff0000000000000));
    return float_literal_bits(((uint64_t)(exponent + 1023) << 52) |
                              (significand & UINT64_C(0x000fffffffffffff)));
}
