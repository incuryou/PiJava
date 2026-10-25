/** Active-use initialization, declaring classes, and erroneous-class regressions. */
public class ClassInitChecks {
    private static int assertions;
    private static RuntimeException nonErrorCause;
    private static Error errorCause;
    private static Error superCause;
    private static int literalRuns;
    private static int getRuns;
    private static int putRuns;
    private static int newRuns;
    private static int forNameRuns;
    private static int fieldParentRuns;
    private static int fieldChildRuns;
    private static int methodParentRuns;
    private static int methodChildRuns;
    private static int order;
    private static int failedChildRuns;

    private static void check(boolean condition, String reason) {
        assertions++;
        if (!condition) throw new RuntimeException(reason);
    }
    private static int failNonError() { throw nonErrorCause; }
    private static int failError() { throw errorCause; }
    private static int failSuper() { throw superCause; }

    public static class NonErrorFailure { static int value = failNonError(); }
    public static class ErrorFailure { static int value = failError(); }

    private static void failures() {
        nonErrorCause = new RuntimeException("non-Error initializer cause");
        boolean caught = false;
        try { int unused = NonErrorFailure.value; }
        catch (ExceptionInInitializerError expected) {
            caught = true;
            check(expected.getException() == nonErrorCause, "initializer wrapper retains original exception");
            check(expected.getCause() == nonErrorCause, "initializer wrapper exposes original cause");
        }
        check(caught, "first non-Error initialization failure is wrapped");
        caught = false;
        try { int unused = NonErrorFailure.value; }
        catch (NoClassDefFoundError expected) { caught = true; }
        check(caught, "second use of failed non-Error class is NoClassDefFoundError");

        errorCause = new Error("unwrapped initializer Error");
        caught = false;
        try { int unused = ErrorFailure.value; }
        catch (Error expected) {
            caught = true;
            check(expected == errorCause, "initializer Error is propagated without wrapping");
        }
        check(caught, "first Error initialization failure is thrown");
        caught = false;
        try { int unused = ErrorFailure.value; }
        catch (NoClassDefFoundError expected) { caught = true; }
        check(caught, "second use of failed Error class is NoClassDefFoundError");
    }

    private static int literalValue() { literalRuns++; return 40; }
    private static int getterValue() { getRuns++; return 41; }
    private static int putterValue() { putRuns++; return 42; }
    public static class LiteralOnly { static int value = literalValue(); }
    public static class GetOnly { static int value = getterValue(); }
    public static class PutOnly { static int value = putterValue(); }
    public static class NewOnly {
        static { newRuns++; }
        public NewOnly() { }
    }
    public static class ForNameOnly {
        static { forNameRuns++; }
        public ForNameOnly() { }
    }

    private static void activeUses() throws Exception {
        Class literal = LiteralOnly.class;
        check(literalRuns == 0, "class literal does not initialize class");
        check(LiteralOnly.value == 40 && literalRuns == 1, "getstatic initializes literal class once");
        check(LiteralOnly.class == literal && literalRuns == 1, "repeated literal keeps canonical class");
        check(getRuns == 0, "unused getstatic class is uninitialized");
        check(GetOnly.value == 41 && getRuns == 1, "getstatic triggers initializer");
        check(GetOnly.value == 41 && getRuns == 1, "getstatic does not repeat initializer");
        check(putRuns == 0, "unused putstatic class is uninitialized");
        PutOnly.value = 99;
        check(putRuns == 1 && PutOnly.value == 99, "putstatic initializes before applying store");
        check(newRuns == 0, "unused new class is uninitialized");
        Object instance = new NewOnly();
        check(instance != null && newRuns == 1, "new triggers initializer before constructor");
        new NewOnly();
        check(newRuns == 1, "second new does not repeat initializer");
        Class namedLiteral = ForNameOnly.class;
        check(forNameRuns == 0, "forName target literal leaves class uninitialized");
        Class named = Class.forName("ClassInitChecks$ForNameOnly");
        check(named == namedLiteral && forNameRuns == 1, "forName actively initializes canonical class");
        check(Class.forName("ClassInitChecks$ForNameOnly") == named && forNameRuns == 1,
                "repeated forName does not repeat initializer");
    }

    private static int parentFieldValue() { fieldParentRuns++; return 17; }
    private static int childFieldValue() { fieldChildRuns++; return 18; }
    public static class FieldParent { static int value = parentFieldValue(); }
    public static class FieldChild extends FieldParent { static int own = childFieldValue(); }
    public static class MethodParent {
        static { methodParentRuns++; }
        static int answer() { return 23; }
    }
    public static class MethodChild extends MethodParent { static { methodChildRuns++; } }
    private static int ordered(int digit) { order = order * 10 + digit; return digit; }
    public static class OrderParent { static int parent = ordered(1); }
    public static class OrderChild extends OrderParent {
        static int child = ordered(2);
        public OrderChild() { }
    }

    private static void declarationAndOrder() {
        check(fieldParentRuns == 0 && fieldChildRuns == 0, "field classes initially unused");
        check(FieldChild.value == 17, "inherited static field value comes from parent");
        check(fieldParentRuns == 1 && fieldChildRuns == 0,
                "inherited static field initializes declaring parent only");
        check(methodParentRuns == 0 && methodChildRuns == 0, "method classes initially unused");
        check(MethodChild.answer() == 23, "inherited static method executes parent body");
        check(methodParentRuns == 1 && methodChildRuns == 0,
                "inherited static method initializes declaring parent only");
        check(order == 0, "ordered superclass and subclass initially unused");
        new OrderChild();
        check(order == 12 && OrderParent.parent == 1 && OrderChild.child == 2,
                "superclass initializer runs before subclass initializer");
    }

    public static class RecursiveParent { static int parent = RecursiveChild.child; }
    public static class RecursiveChild extends RecursiveParent { static int child = 42; }
    public static class FailingParent { static int value = failSuper(); }
    private static int forbiddenChildValue() { failedChildRuns++; return 5; }
    public static class FailingChild extends FailingParent { static int value = forbiddenChildValue(); }

    private static void superclassReentryAndFailure() {
        check(RecursiveChild.child == 42, "subclass completes after superclass recursive active use");
        check(RecursiveParent.parent == 0,
                "recursive superclass use observes subclass default while subclass is initializing");
        superCause = new Error("superclass initialization failure");
        boolean caught = false;
        try { int unused = FailingChild.value; }
        catch (Error expected) {
            caught = true;
            check(expected == superCause, "subclass first use propagates original superclass Error");
        }
        check(caught && failedChildRuns == 0, "failed superclass prevents subclass initializer");
        caught = false;
        try { int unused = FailingChild.value; }
        catch (NoClassDefFoundError expected) { caught = true; }
        check(caught && failedChildRuns == 0, "failed superclass makes subclass erroneous");
        caught = false;
        try { int unused = FailingParent.value; }
        catch (NoClassDefFoundError expected) { caught = true; }
        check(caught, "failed superclass itself remains erroneous");
    }

    public static int run() throws Exception {
        assertions = 0;
        failures();
        activeUses();
        declarationAndOrder();
        superclassReentryAndFailure();
        return assertions;
    }
}
