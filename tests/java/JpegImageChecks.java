import java.io.InputStream;
import javax.microedition.lcdui.Image;

public class JpegImageChecks {
    private static int checks;
    private static void check(boolean value) {
        if (!value) throw new RuntimeException("JPEG image check " + checks);
        checks++;
    }
    private static byte[] bytes(String resource) throws Exception {
        InputStream in = JpegImageChecks.class.getResourceAsStream(resource);
        check(in != null);
        java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
        int value;
        while ((value = in.read()) >= 0) out.write(value);
        in.close();
        return out.toByteArray();
    }
    public static int run() throws Exception {
        Image baseline = Image.createImage("/jpeg-baseline.jpg");
        check(baseline.getWidth() == 17 && baseline.getHeight() == 11);
        check(!baseline.isMutable());
        int[] pixels = new int[17 * 11];
        baseline.getRGB(pixels, 0, 17, 0, 0, 17, 11);
        for (int i = 0; i < pixels.length; i++) check((pixels[i] >>> 24) == 255);
        boolean immutable = false;
        try { baseline.getGraphics(); } catch (IllegalStateException expected) { immutable = true; }
        check(immutable);
        byte[] data = bytes("/jpeg-baseline.jpg");
        byte[] padded = new byte[data.length + 13];
        System.arraycopy(data, 0, padded, 7, data.length);
        Image slice = Image.createImage(padded, 7, data.length);
        int[] copy = new int[pixels.length];
        slice.getRGB(copy, 0, 17, 0, 0, 17, 11);
        for (int i = 0; i < copy.length; i++) check(copy[i] == pixels[i]);
        InputStream in = JpegImageChecks.class.getResourceAsStream("/jpeg-progressive.jpg");
        Image progressive = Image.createImage(in); in.close();
        check(progressive.getWidth() == 24 && progressive.getHeight() == 19);
        Image gray = Image.createImage("/jpeg-grayscale.jpg");
        int[] grayPixel = new int[1];gray.getRGB(grayPixel, 0, 1, 4, 3, 1, 1);
        check((grayPixel[0] & 255) == ((grayPixel[0] >>> 8) & 255));
        check((grayPixel[0] & 255) == ((grayPixel[0] >>> 16) & 255));
        boolean truncatedResource = false, truncatedBytes = false, oversized = false;
        try { Image.createImage("/jpeg-truncated.jpg"); } catch (java.io.IOException expected) { truncatedResource = true; }
        byte[] truncated = bytes("/jpeg-truncated.jpg");
        try { Image.createImage(truncated, 0, truncated.length); } catch (IllegalArgumentException expected) { truncatedBytes = true; }
        try { Image.createImage("/jpeg-oversized.jpg"); } catch (java.io.IOException expected) { oversized = true; }
        check(truncatedResource && truncatedBytes && oversized);
        return checks;
    }
}
