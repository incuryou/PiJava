package javax.microedition.io;

import java.io.IOException;

/** No push delivery or alarm launch capability is installed. */
public class PushRegistry {
    private PushRegistry() {}

    public static String[] listConnections(boolean available) {
        return new String[0];
    }
    public static String getMIDlet(String connection) { return null; }
    public static String getFilter(String connection) { return null; }
    public static boolean unregisterConnection(String connection) { return false; }

    public static void registerConnection(String connection, String midlet, String filter)
            throws ClassNotFoundException, IOException {
        if (connection == null || filter == null || connection.indexOf(':') <= 0 || filter.length() == 0)
            throw new IllegalArgumentException("Invalid push connection or filter");
        int colon = connection.indexOf(':');
        for (int i = 0; i < colon; i++) {
            char c = connection.charAt(i);
            boolean letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
            if (!letter && (i == 0 || !((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.')))
                throw new IllegalArgumentException("Invalid push protocol");
        }
        if (midlet == null || midlet.length() == 0) throw new ClassNotFoundException(midlet);
        throw new ConnectionNotFoundException("Push delivery is unavailable");
    }
    public static long registerAlarm(String midlet, long time)
            throws ClassNotFoundException, ConnectionNotFoundException {
        if (midlet == null || midlet.length() == 0) throw new ClassNotFoundException(midlet);
        throw new ConnectionNotFoundException("Alarm launch is unavailable");
    }
}
