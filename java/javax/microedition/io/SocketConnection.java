package javax.microedition.io;
public interface SocketConnection extends StreamConnection {
 byte DELAY=0,LINGER=1,KEEPALIVE=2,RCVBUF=3,SNDBUF=4;
 void setSocketOption(byte option,int value) throws java.io.IOException;
 int getSocketOption(byte option) throws java.io.IOException;
 String getLocalAddress() throws java.io.IOException;
 int getLocalPort() throws java.io.IOException;
 String getAddress() throws java.io.IOException;
 int getPort() throws java.io.IOException;
}
