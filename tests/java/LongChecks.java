/** Self-contained CLDC Long conversion checks. */
public class LongChecks {
    private static int assertions;
    private static void check(boolean condition, String reason) {
        assertions++;
        if (!condition) throw new RuntimeException(reason);
    }
    private static void rejects(String text, int radix) {
        boolean rejected = false;
        try { Long.parseLong(text, radix); }
        catch (NumberFormatException expected) { rejected = true; }
        check(rejected, "Long parse rejection");
    }
    public static int run() {
        assertions = 0;
        check(Long.toString(0).equals("0"), "Long zero format");
        check(Long.toString(Long.MIN_VALUE).equals("-9223372036854775808"), "Long minimum format");
        check(Long.toString(Long.MAX_VALUE).equals("9223372036854775807"), "Long maximum format");
        check(Long.toString(-255, 16).equals("-ff"), "Long hexadecimal sign");
        check(Long.toString(Long.MIN_VALUE, 16).equals("-8000000000000000"), "Long minimum hexadecimal");
        check(Long.toString(Long.MAX_VALUE, 16).equals("7fffffffffffffff"), "Long maximum hexadecimal");
        check(Long.toString(Long.MAX_VALUE, 36).equals("1y2p0ij32e8e7"), "Long maximum base36");
        check(Long.toString(Long.MIN_VALUE, 36).equals("-1y2p0ij32e8e8"), "Long minimum base36");
        check(Long.toString(255, 1).equals("255"), "Long low format radix fallback");
        check(Long.toString(-255, 37).equals("-255"), "Long high format radix fallback");
        check(Long.toString(128, 2).equals("10000000"), "Long binary format");
        check(Long.toString(511, 8).equals("777"), "Long octal format");
        check(Long.parseLong("-9223372036854775808") == Long.MIN_VALUE, "Long minimum parse");
        check(Long.parseLong("9223372036854775807") == Long.MAX_VALUE, "Long maximum parse");
        check(Long.parseLong("-8000000000000000", 16) == Long.MIN_VALUE, "Long minimum hex parse");
        check(Long.parseLong("7FFFFFFFFFFFFFFF", 16) == Long.MAX_VALUE, "Long maximum uppercase hex parse");
        check(Long.parseLong("Hazelnut", 36) == 1356099454469L, "Long base36 parse");
        check(Long.parseLong("1100110", 2) == 102, "Long binary parse");
        check(Long.parseLong("00017") == 17, "Long leading zeros");
        check(Long.parseLong("-0") == 0, "Long negative zero");
        check(Long.parseLong("L", 23) == 21, "Long radix letter digit");
        long[] values = { Long.MIN_VALUE, Long.MIN_VALUE + 1, -4294967296L, -2147483649L,
                -1, 0, 1, 2147483648L, 4294967296L, Long.MAX_VALUE - 1, Long.MAX_VALUE };
        for (int radix = 2; radix <= 36; radix++) {
            for (int i = 0; i < values.length; i++) {
                check(Long.parseLong(Long.toString(values[i], radix), radix) == values[i],
                        "Long all radix round trip");
            }
        }
        rejects(null, 10);
        rejects("", 10);
        rejects("-", 10);
        rejects("+1", 10); // CLDC 1.1 allows only a leading minus sign.
        rejects(" 1", 10);
        rejects("1 ", 10);
        rejects("1L", 10);
        rejects("0x10", 16);
        rejects("--1", 10);
        rejects("1-", 10);
        rejects("99", 8);
        rejects("2", 2);
        rejects("z", 35);
        rejects("1", 1);
        rejects("1", 37);
        rejects("1", -2);
        rejects("9223372036854775808", 10);
        rejects("-9223372036854775809", 10);
        rejects("18446744073709551616", 10);
        rejects("8000000000000000", 16);
        rejects("-8000000000000001", 16);
        rejects("ffffffffffffffff", 16);
        rejects("1y2p0ij32e8e8", 36);
        rejects("-1y2p0ij32e8e9", 36);
        rejects("1000000000000000000000000000000000000000000000000000000000000000", 2);
        check(new Long(-17).toString().equals("-17"), "Long instance format");
        check(Long.valueOf(Long.MIN_VALUE).longValue() == Long.MIN_VALUE, "Long boxed minimum");
        check(Long.valueOf(Long.MAX_VALUE).longValue() == Long.MAX_VALUE, "Long boxed maximum");
        check(new Long(7).equals(Long.valueOf(7)), "Long wrapper equality");
        check(!new Long(7).equals(new Long(8)), "Long unequal values");
        check(!new Long(7).equals(null), "Long null inequality");
        check(!new Long(7).equals(new Integer(7)), "Long other type inequality");
        check(new Long(0x123456789abcdef0L).hashCode() == (int) 0x88888888L, "Long hash halves");
        check(new Long(-1).hashCode() == 0, "Long negative hash");
        check(new Long(Long.MIN_VALUE).floatValue() == -9223372036854775808.0f, "Long float minimum");
        check(new Long(Long.MAX_VALUE).doubleValue() == 9223372036854775807.0,
                "Long double maximum");
        return assertions;
    }
}
