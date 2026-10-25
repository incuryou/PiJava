package javax.microedition.media.protocol;
public class ContentDescriptor {
    private final String contentType;
    public ContentDescriptor(String type) { contentType=type; }
    public String getContentType() { return contentType; }
}
