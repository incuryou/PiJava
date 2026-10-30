package javax.microedition.io;

import java.io.IOException;

/** MIDP HTTPS contract; no TLS transport is supplied by Connector. */
public interface HttpsConnection extends HttpConnection {
    SecurityInfo getSecurityInfo() throws IOException;
    int getPort();
}
