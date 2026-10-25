import javax.microedition.lcdui.*;

/** High-level UI mutations, selection, input, ownership and command delivery in the VM. */
public final class UiChecks {
    private static void check(boolean value, String label) { if (!value) throw new RuntimeException(label); }
    public static int run() {
        List multiple = new List("many", Choice.MULTIPLE, new String[] {"A", "B", "C"}, null);
        Choice choice = multiple;
        check(choice.getSelectedIndex() == -1 && !choice.isSelected(0), "multiple initially clear");
        choice.setSelectedFlags(new boolean[] {true, false, true});
        boolean[] selected = new boolean[] {true, true, true, true, true};
        check(choice.getSelectedFlags(selected) == 2 && selected[0] && !selected[1] && selected[2] && !selected[3] && !selected[4], "multiple flags and result tail");
        choice.insert(1, "inserted", null);
        check(choice.size() == 4 && choice.getString(1).equals("inserted") && choice.isSelected(3), "choice insert shifts flags");
        choice.delete(0);
        check(choice.size() == 3 && choice.isSelected(2) && choice.getString(0).equals("inserted"), "choice delete shifts flags");
        choice.set(0, "changed", null);
        check(choice.getString(0).equals("changed"), "choice set");
        choice.setFitPolicy(Choice.TEXT_WRAP_ON);
        check(choice.getFitPolicy() == Choice.TEXT_WRAP_ON && choice.getFont(0) != null, "choice presentation metadata");
        boolean rejected = false;
        try { choice.getString(3); } catch (IndexOutOfBoundsException expected) { rejected = true; }
        check(rejected, "choice bounds");
        rejected = false; try { choice.append(null, null); } catch (NullPointerException expected) { rejected = true; }
        check(rejected, "choice rejects null text");
        rejected = false; try { choice.getSelectedFlags(new boolean[2]); } catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "choice result capacity");
        rejected = false; try { choice.setSelectedFlags(null); } catch (NullPointerException expected) { rejected = true; }
        check(rejected, "choice null flags");
        choice.deleteAll(); check(choice.size() == 0 && choice.getSelectedIndex() == -1, "empty choice");

        ChoiceGroup exclusive = new ChoiceGroup("exclusive", Choice.EXCLUSIVE, new String[] {"a", "b", "c"}, null);
        check(exclusive.getSelectedIndex() == 0, "exclusive initial selection");
        exclusive.setSelectedIndex(1, true); exclusive.setSelectedIndex(1, false);
        check(exclusive.getSelectedIndex() == 1, "exclusive false ignored");
        exclusive.insert(0, "zero", null); check(exclusive.getSelectedIndex() == 2, "exclusive insertion retains identity");
        exclusive.delete(2); check(exclusive.getSelectedIndex() >= 0, "exclusive selected deletion restores one");
        exclusive.setSelectedFlags(new boolean[] {false, false, false}); check(exclusive.getSelectedIndex() == 0, "exclusive zero flags defaults first");
        exclusive.setSelectedFlags(new boolean[] {false, true, true}); check(exclusive.getSelectedIndex() == 1, "exclusive first true wins");
        rejected = false; try { new List("invalid", Choice.POPUP); } catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "list type validation");
        rejected = false; try { new ChoiceGroup("invalid", Choice.IMPLICIT); } catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "choicegroup type validation");

        Form form = new Form("old"); Screen screen = form;
        screen.setTitle("new"); Ticker ticker = new Ticker("ticker"); screen.setTicker(ticker);
        check(form.getTitle().equals("new") && form.getTicker() == ticker, "screen title and ticker inheritance");
        StringItem a = new StringItem("label", "a"), b = new StringItem(null, "b");
        check(form.append(a) == 0, "form append index"); form.insert(0, b);
        check(form.get(0) == b && form.get(1) == a, "form insertion order");
        rejected = false; try { form.append(a); } catch (IllegalStateException expected) { rejected = true; }
        check(rejected, "same form item ownership");
        rejected = false; try { new Form("other").append(a); } catch (IllegalStateException expected) { rejected = true; }
        check(rejected, "cross form item ownership");
        rejected = false; try { form.append((Item)null); } catch (NullPointerException expected) { rejected = true; }
        check(rejected, "form null item");
        rejected = false; try { form.set(2, new StringItem(null, "invalid")); } catch (IndexOutOfBoundsException expected) { rejected = true; }
        check(rejected, "form bounds");
        form.set(1, new StringItem(null, "replacement")); new Form("released").append(a);
        form.deleteAll(); check(form.size() == 0, "form delete all"); new Form("released2").append(b);

        TextField text = new TextField("number", "12", 4, TextField.NUMERIC);
        text.insert("3", 99); check(text.getString().equals("123") && text.getCaretPosition() == 3, "text insert clamped");
        text.delete(1, 1); check(text.getString().equals("13") && text.getCaretPosition() == 1, "text delete caret");
        char[] buffer = new char[4]; check(text.getChars(buffer) == 2 && buffer[1] == '3', "text chars ABI");
        rejected = false; try { text.setString("abc"); } catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected && text.getString().equals("13"), "text constraints atomic");
        rejected = false; try { text.setString("12345"); } catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "text capacity rejected");
        check(text.setMaxSize(1) == 1 && text.getString().equals("1"), "text max size truncation");
        text.setConstraints(TextField.ANY); text.setString("x"); text.setConstraints(TextField.NUMERIC);
        check(text.size() == 0, "constraints clear invalid content");
        TextBox box = new TextBox("text", "", 4, TextField.NUMERIC); box.keyPressed(49); box.keyPressed(50); box.keyPressed(-3); box.keyPressed(51);
        check(box.getString().equals("132") && box.getCaretPosition() == 2, "textbox keypad caret insertion");
        box.keyPressed(-8); check(box.getString().equals("12"), "textbox backspace");
        box.setConstraints(TextField.NUMERIC | TextField.UNEDITABLE); box.keyPressed(52); check(box.getString().equals("12"), "uneditable input ignored");
        Form input = new Form("input"); TextField digits = new TextField(null, "", 5, TextField.NUMERIC); input.append(digits);
        UiState state = new UiState(); input.setItemStateListener(state); input.keyPressed(50); input.keyPressed(56);
        check(digits.getString().equals("28") && state.changed == 2, "form numeric input and state callbacks");

        Gauge gauge = new Gauge(null, true, 10, 20); check(gauge.getValue() == 10, "gauge clamp");
        gauge.setValue(-2); check(gauge.getValue() == 0, "gauge lower clamp"); gauge.setValue(8); gauge.setMaxValue(4); check(gauge.getValue() == 4, "gauge resized clamp");
        rejected = false; try { gauge.setMaxValue(Gauge.INDEFINITE); } catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "interactive indefinite gauge rejected");
        Gauge indefinite = new Gauge(null, false, Gauge.INDEFINITE, Gauge.CONTINUOUS_RUNNING);
        rejected = false; try { indefinite.setValue(4); } catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "indefinite gauge state validation");

        UiForm commands = new UiForm(); UiListener listener = new UiListener(); commands.setCommandListener(listener);
        Command late = new Command("late", Command.OK, 9), early = new Command("early", Command.OK, 1), back = new Command("back", Command.BACK, 3);
        commands.addCommand(late); commands.addCommand(early); commands.addCommand(back); commands.addCommand(early);
        check(commands.count() == 3 && commands.first() == early, "commands priority and identity deduplication");
        commands.keyPressed(-6); commands.keyPressed(-2); commands.keyPressed(-6); check(listener.last == back, "command menu delivery");
        commands.removeCommand(late); commands.keyPressed(-6); check(listener.last == early, "left softkey dispatch");
        commands.keyPressed(-7); check(listener.last == back, "right back softkey dispatch");
        for (int i = 0; i < 12; i++) commands.addCommand(new Command("many", Command.SCREEN, i));
        check(commands.count() == 14, "command storage grows beyond four");
        commands.removeCommand(early); check(commands.count() == 13, "command removal");
        rejected = false; try { commands.addCommand(null); } catch (NullPointerException expected) { rejected = true; }
        check(rejected, "null command rejected");

        List implicit = new List("implicit", Choice.IMPLICIT, new String[] {"a", "b"}, null); implicit.setCommandListener(listener);
        Command custom = new Command("choose", Command.OK, 0); implicit.setSelectCommand(custom); implicit.keyPressed(-2); implicit.keyPressed(-5);
        check(listener.last == custom && implicit.getSelectedIndex() == 1, "implicit custom select command");
        implicit.removeCommand(custom); int calls = listener.calls; implicit.keyPressed(-5); check(listener.calls == calls, "removed implicit command disabled");
        multiple.append("toggle", null); multiple.keyPressed(-5); check(multiple.isSelected(0), "multiple list fire toggles");

        Alert alert = new Alert("alert", "message", null, AlertType.INFO); alert.setCommandListener(listener); alert.keyPressed(-6);
        check(listener.last == Alert.DISMISS_COMMAND, "alert built in dismissal callback");
        alert.setTimeout(100); check(alert.getTimeout() == 100, "alert timeout");
        alert.addCommand(early); alert.addCommand(back); check(alert.getTimeout() == Alert.FOREVER, "multiple commands make alert modal");
        alert.removeCommand(back); alert.dismiss(); check(listener.last == early, "alert timeout delivers application command");
        alert.removeCommand(early); alert.dismiss(); check(listener.last == Alert.DISMISS_COMMAND, "alert restores built in command");
        rejected = false; try { alert.setTimeout(0); } catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "alert timeout validation");
        Gauge progress = new Gauge(null, false, 10, 2); alert.setIndicator(progress); check(alert.getIndicator() == progress, "alert indicator");
        rejected = false; try { progress.setLabel("forbidden"); } catch (IllegalStateException expected) { rejected = true; }
        check(rejected, "indicator appearance locked");
        alert.setIndicator(null); progress.setLabel("free"); check(progress.getLabel().equals("free"), "indicator ownership released");
        rejected = false; try { alert.setIndicator(gauge); } catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "interactive alert indicator rejected");
        return 1;
    }
}
final class UiForm extends Form {
    UiForm() { super("commands"); }
    int count() { return commandCount; }
    Command first() { return commands[0]; }
}
final class UiListener implements CommandListener {
    Command last; int calls;
    public void commandAction(Command command, Displayable displayable) { last = command; calls++; }
}
final class UiState implements ItemStateListener {
    int changed;
    public void itemStateChanged(Item item) { changed++; }
}
