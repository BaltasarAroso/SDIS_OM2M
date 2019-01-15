package app.org.eclipse.om2m.app;

import java.io.IOException;
import java.io.OutputStream;

public class SerialWriter implements Runnable {
    OutputStream out;

    public SerialWriter (OutputStream out) {
        this.out = out;
    }

    public void run () {
        try {
            int c;
            while ((c = System.in.read()) > -1) {
                this.out.write(c);
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
