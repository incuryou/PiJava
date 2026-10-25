package javax.bluetooth;
public interface L2CAPConnection extends javax.microedition.io.Connection {
 int DEFAULT_MTU=672,MINIMUM_MTU=48;
 int getReceiveMTU() throws java.io.IOException;
 int getTransmitMTU() throws java.io.IOException;
 void send(byte[] data) throws java.io.IOException;
 int receive(byte[] data) throws java.io.IOException;
 boolean ready() throws java.io.IOException;
}
