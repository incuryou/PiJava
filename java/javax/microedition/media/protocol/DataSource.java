package javax.microedition.media.protocol;
import java.io.IOException;
import javax.microedition.media.Controllable;
public abstract class DataSource implements Controllable {
    private final String locator;
    public DataSource(String source) { locator=source; }
    public String getLocator() { return locator; }
    public abstract String getContentType();
    public abstract void connect() throws IOException;
    public abstract void disconnect();
    public abstract void start() throws IOException;
    public abstract void stop() throws IOException;
    public abstract SourceStream[] getStreams();
    public abstract javax.microedition.media.Control getControl(String controlType);
    public abstract javax.microedition.media.Control[] getControls();
}
