package ssatr.service;

import java.util.concurrent.BlockingQueue;

/**
 * Preia mesajele de un anumit tip ("telemetry", "alarm" sau "status") din coada
 * inbox, le salvează și actualizează dashboard-ul.
 * Formatul mesajelor (câmpuri separate prin ";"):
 *   telemetry:  deviceId;timestamp;temperatura
 *   alarm:      deviceId;timestamp;temperatura;prag
 *   status:     deviceId;timestamp;text
 */
public class MessageProcessor implements Runnable {

    private final String kind;
    private final BlockingQueue<String> inbox;
    private final CsvStorage storage;
    private final Dashboard dashboard;

    public MessageProcessor(String kind, BlockingQueue<String> inbox, CsvStorage storage, Dashboard dashboard) {
        this.kind = kind;
        this.inbox = inbox;
        this.storage = storage;
        this.dashboard = dashboard;
    }

    @Override
    public void run() {
        // TODO 4: într-o buclă:
        //   - preluați un mesaj: inbox.take()
        //   - salvați mesajul în fișierul CSV: storage.append(kind, mesaj)
        //     (CsvStorage este gata implementată)
        //   - actualizați dashboard-ul, în funcție de kind
        //   - alarmele și confirmările se afișează imediat în consolă
        //   - un mesaj greșit (format invalid) nu trebuie să oprească thread-ul
        try {
            while (true) {
                String message = inbox.take();

                // mesajul se salvează exact cum a sosit, chiar dacă formatul e greșit
                storage.append(kind, message);

                try {
                    process(message);
                } catch (RuntimeException e) {
                    // format invalid: îl semnalăm și continuăm cu următorul mesaj
                    System.out.println("[" + kind + "] mesaj invalid ignorat: \"" + message + "\" (" + e + ")");
                }
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
    private void process(String message) {
        String[] f = message.split(";", -1);

        switch (kind) {
            case "telemetry":
                // deviceId;timestamp;temperatura
                requireFields(f, 3);
                dashboard.onTelemetry(f[0], Double.parseDouble(f[2]));
                break;
            case "alarm":
                // deviceId;timestamp;temperatura;prag
                requireFields(f, 4);
                double temperature = Double.parseDouble(f[2]);
                double threshold = Double.parseDouble(f[3]);
                dashboard.onAlarm(f[0]);
                System.out.printf(java.util.Locale.US, "!!! ALARMĂ %s: %.2f °C (prag %.2f)%n", f[0], temperature, threshold);
                break;
            case "status":
                // deviceId;timestamp;text
                requireFields(f, 3);
                dashboard.onStatus(f[0], f[2]);
                System.out.println("<<< " + f[0] + ": " + f[2]);
                break;
            default:
                throw new IllegalArgumentException("tip necunoscut: " + kind);
        }
    }

    private static void requireFields(String[] fields, int expected) {
        if (fields.length != expected || fields[0].isBlank()) {
            throw new IllegalArgumentException("se așteptau " + expected + " câmpuri, sunt " + fields.length);
        }
    }
}
