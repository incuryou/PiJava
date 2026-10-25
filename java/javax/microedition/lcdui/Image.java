package javax.microedition.lcdui;
public class Image {
    public int width;
    public int height;
    public static native Image createImage(int width, int height);
    public static native Image createImage(String name) throws java.io.IOException;
    public static native Image createImage(byte[] data, int offset, int length);
    public static Image createImage(java.io.InputStream stream) throws java.io.IOException {
        if (stream == null) throw new NullPointerException();
        java.io.ByteArrayOutputStream output = new java.io.ByteArrayOutputStream();
        byte[] buffer = new byte[4096];
        while (true) {
            int count = stream.read(buffer, 0, buffer.length);
            if (count < 0) break;
            if (count == 0) {
                int value = stream.read();
                if (value < 0) break;
                output.write(value);
            } else output.write(buffer, 0, count);
        }
        byte[] data = output.toByteArray();
        return createImage(data, 0, data.length);
    }
    public native Graphics getGraphics();
    public static native Image createRGBImage(int[] rgb, int width, int height, boolean processAlpha);
    public native void getRGB(int[] rgbData, int offset, int scanlength, int x, int y, int width, int height);
    public int getWidth() { return width; }
    public int getHeight() { return height; }
    private boolean mutable;
    public boolean isMutable() { return mutable; }
    public static Image createImage(Image source) {
        if (source == null) throw new NullPointerException();
        return createImage(source, 0, 0, source.getWidth(), source.getHeight(), 0);
    }
    public static Image createImage(Image image, int x, int y, int width, int height, int transform) {
        if (image == null) throw new NullPointerException();
        if (width <= 0 || height <= 0 || x < 0 || y < 0 ||
            x > image.getWidth() - width || y > image.getHeight() - height || transform < 0 || transform > 7)
            throw new IllegalArgumentException();
        int[] rgb = new int[width * height];
        image.getRGB(rgb, 0, width, x, y, width, height);
        if (transform == 0) return createRGBImage(rgb, width, height, true);
        boolean swap = transform >= 4;
        int ow = swap ? height : width;
        int oh = swap ? width : height;
        int[] out = new int[ow * oh];
        int yy = 0;
        while (yy < height) {
            int xx = 0;
            while (xx < width) {
                int dx = xx, dy = yy;
                if (transform == 1) dy = height - 1 - yy;
                else if (transform == 2) dx = width - 1 - xx;
                else if (transform == 3) { dx = width - 1 - xx; dy = height - 1 - yy; }
                else if (transform == 4) { dx = yy; dy = xx; }
                else if (transform == 5) { dx = height - 1 - yy; dy = xx; }
                else if (transform == 6) { dx = yy; dy = width - 1 - xx; }
                else if (transform == 7) { dx = height - 1 - yy; dy = width - 1 - xx; }
                out[dy * ow + dx] = rgb[yy * width + xx];
                xx = xx + 1;
            }
            yy = yy + 1;
        }
        return createRGBImage(out, ow, oh, true);
    }
}
