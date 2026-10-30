package javax.bluetooth;

/** Failure to register or update a Bluetooth service. */
public class ServiceRegistrationException extends java.io.IOException {
    public ServiceRegistrationException() {}
    public ServiceRegistrationException(String message) { super(message); }
}
