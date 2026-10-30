/** Standard JSR-75 contact API contract; this runtime has no PIM backend. */
package javax.microedition.pim;

public interface ContactList extends PIMList {

    Contact createContact();

    Contact importContact(Contact contact);

    void removeContact(Contact contact) throws PIMException;
}
