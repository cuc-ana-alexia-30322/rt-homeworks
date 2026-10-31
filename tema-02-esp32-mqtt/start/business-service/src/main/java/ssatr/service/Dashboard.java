package ssatr.service;

import java.util.Locale;
import java.util.Map;
import java.util.TreeMap;
/**
 * Ultima stare cunoscută a fiecărui device: ultima temperatură, momentul
 * ultimului mesaj, numărul de alarme etc.
 *
 * Este actualizată de thread-urile MessageProcessor și citită de CommandConsole,
 * deci accesul trebuie sincronizat.
 */
public class Dashboard {

    // TODO 5: alegeți structura de date pentru starea device-urilor
    //   (de exemplu un Map<String, ...> cu cheia deviceId)
    //   și adăugați metodele de actualizare apelate din MessageProcessor.
    private static class DeviceState {
        double temperature = Double.NaN;   // NaN = nu a sosit încă telemetrie
        long lastMessageMillis;
        int alarms;
        String lastStatus = "-";
    }

    // TreeMap: device-urile apar sortate după ID în tabel
    private final Map<String, DeviceState> devices = new TreeMap<>();

    private DeviceState state(String deviceId) {
        return devices.computeIfAbsent(deviceId, id -> new DeviceState());
    }

    public synchronized void onTelemetry(String deviceId, double temperature) {
        DeviceState s = state(deviceId);
        s.temperature = temperature;
        s.lastMessageMillis = System.currentTimeMillis();
    }

    public synchronized void onAlarm(String deviceId) {
        DeviceState s = state(deviceId);
        s.alarms++;
        s.lastMessageMillis = System.currentTimeMillis();
    }

    public synchronized void onStatus(String deviceId, String text) {
        DeviceState s = state(deviceId);
        s.lastStatus = text;
        s.lastMessageMillis = System.currentTimeMillis();
    }
    /** Afișează un tabel cu starea tuturor device-urilor. */
    public void print() {
        // TODO 5: afișați câte un rând pentru fiecare device, de exemplu:
        //   DEVICE     TEMP    ULTIMUL MESAJ   ALARME
        //   device-1   23.41   acum 1 s        2
        if (devices.isEmpty()) {
            System.out.println("(niciun device nu a trimis mesaje încă)");
            return;
        }

        long now = System.currentTimeMillis();
        System.out.printf("%-12s %8s   %-14s %6s   %s%n", "DEVICE", "TEMP", "ULTIMUL MESAJ", "ALARME", "ULTIMA CONFIRMARE");
        for (Map.Entry<String, DeviceState> e : devices.entrySet()) {
            DeviceState s = e.getValue();
            String temp = Double.isNaN(s.temperature) ? "-" : String.format(Locale.US, "%.2f", s.temperature);
            System.out.printf("%-12s %8s   %-14s %6d   %s%n",
                    e.getKey(), temp, age(now - s.lastMessageMillis), s.alarms, s.lastStatus);
        }
    }

    private static String age(long millis) {
        long s = Math.max(0, millis) / 1000;
        if (s < 60) {
            return "acum " + s + " s";
        }
        if (s < 3600) {
            return "acum " + (s / 60) + " min";
        }
        return "acum " + (s / 3600) + " h";
    }
    // System.out.println("(dashboard neimplementat)");
}

