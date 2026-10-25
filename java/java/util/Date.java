package java.util;

public class Date {
    private long time;
    private static final String[] DAYS = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    private static final String[] MONTHS = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    public Date() { this(System.currentTimeMillis()); }
    public Date(long time) { this.time = time; }
    public long getTime() { return time; }
    public void setTime(long time) { this.time = time; }
    public boolean equals(Object other) {
        return other instanceof Date && getTime() == ((Date) other).getTime();
    }
    public int hashCode() {
        long millis = getTime();
        return (int) (millis ^ (millis >>> 32));
    }
    public String toString() {
        Calendar calendar = Calendar.getInstance();
        calendar.setTimeInMillis(getTime());
        StringBuffer result = new StringBuffer();
        result.append(DAYS[calendar.get(Calendar.DAY_OF_WEEK) - 1]);
        result.append(' ').append(MONTHS[calendar.get(Calendar.MONTH)]).append(' ');
        appendTwo(result, calendar.get(Calendar.DAY_OF_MONTH));
        result.append(' ');
        appendTwo(result, calendar.get(Calendar.HOUR_OF_DAY));
        result.append(':');
        appendTwo(result, calendar.get(Calendar.MINUTE));
        result.append(':');
        appendTwo(result, calendar.get(Calendar.SECOND));
        result.append(' ').append(calendar.getTimeZone().getID()).append(' ');
        int year = calendar.get(Calendar.YEAR);
        if (year >= 0 && year < 1000) {
            result.append('0');
            if (year < 100) result.append('0');
            if (year < 10) result.append('0');
        }
        return result.append(year).toString();
    }
    private static void appendTwo(StringBuffer result, int value) {
        if (value < 10) result.append('0');
        result.append(value);
    }
}
