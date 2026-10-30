/*
 * Copyright 2017 Nikita Shakarun
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Modified for Fit11: no device implementation; discovery is always empty.
package javax.microedition.sensor;

/** Standard discovery facade for a platform with no sensor devices. */
public final class SensorManager {
    private SensorManager() {}

    public static void addSensorListener(SensorListener listener, String quantity) {
        if (listener == null || quantity == null) throw new NullPointerException();
        // With no present or future platform devices, registration produces no
        // events. Duplicate registration and removal remain silent.
    }

    public static void addSensorListener(SensorListener listener, SensorInfo info) {
        if (listener == null || info == null) throw new NullPointerException();
        // No SensorInfo has been provided by this platform, so every nonnull
        // caller-supplied instance fails the standard platform-identity check.
        throw new IllegalArgumentException();
    }

    public static void removeSensorListener(SensorListener listener) {
        if (listener == null) throw new NullPointerException();
    }

    public static SensorInfo[] findSensors(String url) {
        if (url == null) throw new NullPointerException();
        validateUri(url);
        if (!url.startsWith("sensor:")) throw new IllegalArgumentException();
        String query = url.substring(7);
        if (query.indexOf(':') >= 0 || query.indexOf('?') >= 0) throw new IllegalArgumentException();
        int end = query.indexOf(';');
        if (end < 0) end = query.length();
        if (end == 0) throw new IllegalArgumentException();
        if (end < query.length()) validateProperties(query.substring(end + 1));
        return new SensorInfo[0];
    }

    public static SensorInfo[] findSensors(String quantity, String contextType) {
        if (contextType != null && !validContext(contextType)) throw new IllegalArgumentException();
        return new SensorInfo[0];
    }

    private static boolean validContext(String context) {
        return SensorInfo.CONTEXT_TYPE_AMBIENT.equals(context)
            || SensorInfo.CONTEXT_TYPE_DEVICE.equals(context)
            || SensorInfo.CONTEXT_TYPE_USER.equals(context)
            || SensorInfo.CONTEXT_TYPE_VEHICLE.equals(context);
    }

    // RFC2396 URI characters, not a list of known sensor quantities. Escaped
    // octets may name custom quantities; literal spaces/controls are not URIs.
    private static void validateUri(String url) {
        for (int i = 0; i < url.length(); i++) {
            char c = url.charAt(i);
            if (c == '%') {
                if (i + 2 >= url.length() || !hex(url.charAt(i + 1)) || !hex(url.charAt(i + 2)))
                    throw new IllegalArgumentException();
                i += 2;
            } else if (c <= 32 || c >= 127 || "<>\"#{}|\\^[]`".indexOf(c) >= 0) {
                throw new IllegalArgumentException();
            }
        }
    }

    private static boolean hex(char c) {
        return c >= '0' && c <= '9' || c >= 'A' && c <= 'F' || c >= 'a' && c <= 'f';
    }

    // Ordinary SensorConnection URL grammar. PushRegistry's '?' channel
    // condition extension is not an ordinary discovery URL.
    private static void validateProperties(String query) {
        if (query.length() == 0) throw new IllegalArgumentException();
        int start = 0;
        int previous = 0;
        while (start < query.length()) {
            int end = query.indexOf(';', start);
            if (end < 0) end = query.length();
            int equals = query.indexOf('=', start);
            if (equals < 0 || equals >= end) throw new IllegalArgumentException();
            String key = query.substring(start, equals);
            String value = query.substring(equals + 1, end);
            int selector;
            if (key.equals("contextType")) {
                selector = 1;
                if (!validContext(value)) throw new IllegalArgumentException();
            } else if (key.equals("model")) {
                selector = 2;
                validateSelectorValue(value);
            } else if (key.equals("location")) {
                selector = 3;
                validateSelectorValue(value);
            } else throw new IllegalArgumentException();
            if (selector <= previous) throw new IllegalArgumentException();
            previous = selector;
            if (end == query.length() - 1) throw new IllegalArgumentException();
            start = end + 1;
        }
    }

    private static void validateSelectorValue(String value) {
        for (int i = 0; i < value.length(); i++) {
            char c = value.charAt(i);
            if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9'))
                throw new IllegalArgumentException();
        }
    }
}
