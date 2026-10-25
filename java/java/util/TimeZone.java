package java.util;

/**
 * CLDC time zones. Built-in IDs are GMT, UTC and Asia/Shanghai, plus custom
 * GMT+HH:mm/GMT-HH:mm offsets. These zones are fixed offsets: historical
 * Shanghai changes and daylight-saving rules are intentionally not modeled.
 */
public abstract class TimeZone {
    private String id = "GMT";
    public TimeZone() {}
    public String getID() { return id; }
    public abstract int getOffset(int era, int year, int month, int day,
            int dayOfWeek, int millis);
    public abstract int getRawOffset();
    public abstract boolean useDaylightTime();

    public static TimeZone getDefault() {
        String id = System.getProperty("microedition.timezone");
        return getTimeZone(id == null ? "GMT" : id);
    }

    public static String[] getAvailableIDs() {
        return new String[] { "GMT", "UTC", "Asia/Shanghai" };
    }

    public static TimeZone getTimeZone(String id) {
        if (id == null) throw new NullPointerException("time zone ID");
        if (id.equals("GMT") || id.equals("UTC")) return fixed(id, 0);
        if (id.equals("Asia/Shanghai")) return fixed(id, 28800000);
        if (id.length() == 9 && id.startsWith("GMT")
                && (id.charAt(3) == '+' || id.charAt(3) == '-')
                && id.charAt(6) == ':') {
            int hour = twoDigits(id, 4);
            int minute = twoDigits(id, 7);
            if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
                int offset = (hour * 60 + minute) * 60000;
                return fixed(id, id.charAt(3) == '-' ? -offset : offset);
            }
        }
        return fixed("GMT", 0);
    }

    private static int twoDigits(String id, int start) {
        int first = id.charAt(start) - '0';
        int second = id.charAt(start + 1) - '0';
        return first < 0 || first > 9 || second < 0 || second > 9
                ? -1 : first * 10 + second;
    }

    private static TimeZone fixed(String id, int offset) {
        TimeZone zone = new FixedTimeZone(offset);
        zone.id = id;
        return zone;
    }
}

final class FixedTimeZone extends TimeZone {
    private final int offset;
    FixedTimeZone(int offset) { this.offset = offset; }
    public int getRawOffset() { return offset; }
    public boolean useDaylightTime() { return false; }
    public int getOffset(int era, int year, int month, int day,
            int dayOfWeek, int millis) {
        long actualYear = era == 0 ? 1L - year : year;
        if ((era != 0 && era != 1) || month < 0 || month > 11
                || day < 1 || day > GregorianMath.monthDays(actualYear, month)
                || dayOfWeek < Calendar.SUNDAY || dayOfWeek > Calendar.SATURDAY
                || millis < 0 || millis >= 86400000) {
            throw new IllegalArgumentException("time zone date fields");
        }
        return offset;
    }
}
