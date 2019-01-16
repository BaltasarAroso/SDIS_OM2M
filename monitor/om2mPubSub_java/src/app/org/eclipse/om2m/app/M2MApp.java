package app.org.eclipse.om2m.app;

import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpServer;
import gnu.io.*;
import org.apache.http.HttpEntity;
import org.apache.http.client.methods.CloseableHttpResponse;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.conn.HttpHostConnectException;
import org.apache.http.impl.client.CloseableHttpClient;
import org.apache.http.impl.client.HttpClients;
import org.apache.http.util.EntityUtils;
import org.influxdb.InfluxDB;
import org.influxdb.InfluxDBException;
import org.influxdb.InfluxDBFactory;
import org.influxdb.dto.Point;
import org.influxdb.dto.Query;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.*;
import java.net.*;
import java.util.*;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/**
 * Modified by André Oliveira, Baltasar Aroso & Renato Cruz on 16/01/2019.
 * Modified by Carlos Pereira on 11/08/2017.
 * Created by João Cardoso on 11/03/2016.
 */

public class M2MApp {
    // this
    private static M2MApp instance = null;
    private static HttpServer server = null;
    private static boolean VERBOSE = true;

    // IN-CSE
    private static String originator = "admin:admin";
    private static String cseProtocol = "http";
    private static String cseIp = "192.168.137.1";
    private static int csePort = 8080;
    private static String cseId = "in-cse";
    private static String cseName = "dartes";
    private static String csePoa = cseProtocol + "://" + cseIp + ":" + csePort;

    // IN-AE (this)
    private static String aeMonitorName = "Monitor";
    private static String aeProtocol = "http";
    private static String aeIp = "192.168.137.98";
    private static int aePort = 1600;
    private static String aePoa = aeProtocol + "://" + aeIp + ":" + aePort;
    private static String subName = aeMonitorName + "_sub";

    // ADN-AE
    private static String aeName = "ESP8266";

    // Database (InfluxDB + Chronograf)
    private static String dbPoa = "http://" + aeIp + ":8086";  // change aeIp to whoever hosts the database
    private static InfluxDB influxDB;

    // Auxiliaries
    private static HashMap<String, Long> usbTimes;
    private static long avgNetworkDelayNanos = 0;
    private static boolean flagSubscription = false;
    private static int counter = 1;

    private M2MApp() {}

    public static M2MApp getInstance() {
        if (instance == null) {
            instance = new M2MApp();
        }
        return instance;
    }

    /**
     *
     * @param numberThreads
     */
    public void startServer(int numberThreads) {
        usbTimes = new HashMap<>();

        //create more threads for receptions...
        ExecutorService notService = Executors.newFixedThreadPool(numberThreads * 10);

        printOut("Starting server... ", 0);

        server = null;
        try {
            server = HttpServer.create(new InetSocketAddress(aePort), 0);
        } catch (IOException e) {
            e.printStackTrace();
        }
        server.createContext("/", new MyHandlerM2M());
        server.setExecutor(notService);
        server.start();

        printOut("started.", 1);
    }

    static class MyHandlerM2M implements HttpHandler {

        public void handle(HttpExchange httpExchange) {
//            long epoch = System.currentTimeMillis();

            try {
                InputStream in = httpExchange.getRequestBody();

                StringBuilder requestBody = new StringBuilder();
                int i;
                char c;
                while ((i = in.read()) != -1) {
                    c = (char) i;
                    requestBody.append(c);
                }

                JSONObject json = new JSONObject(requestBody.toString());
                if (json.getJSONObject("m2m:sgn").has("m2m:vrq")) {
                    flagSubscription = true;
                } else {
                    if (flagSubscription) {
                        if (json.getJSONObject("m2m:sgn").has("m2m:nev")) {
                            if (json.getJSONObject("m2m:sgn").getJSONObject("m2m:nev").has("m2m:rep")) {
                                if (json.getJSONObject("m2m:sgn").getJSONObject("m2m:nev").getJSONObject("m2m:rep").has("m2m:cin")) {
                                    JSONObject cin = json.getJSONObject("m2m:sgn").getJSONObject("m2m:nev").getJSONObject("m2m:rep").getJSONObject("m2m:cin");
                                    int ty = cin.getInt("ty");

                                    if (ty == 4) {
                                        String rn = cin.getString("rn");
                                        Long oldTime = usbTimes.get(rn);
                                        if(oldTime != null) {
                                            insertDB(
                                                    influxDB,
                                                    "SDIS",
                                                    "default",
                                                    aeName,
                                                    cin.getString("con"),
                                                    System.nanoTime() - usbTimes.get(rn)
                                            );
                                        } else {
                                            insertDB(
                                                    influxDB,
                                                    "SDIS",
                                                    "default",
                                                    aeName,
                                                    cin.getString("con"),
                                                    0
                                            );
                                        }
                                        //M2MApp.getInstance().addToEpochSub(ciName, epoch);
                                    }
                                }
                            }
                        }
                    }
                }

                // Server needs the response. Otherwise, it issues the following in the terminal:
                // org.apache.http.NoHttpResponseException: IPXXX:PORTYYY failed to respond
                httpExchange.sendResponseHeaders(200, -1);
                httpExchange.close();

            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    /**
     *
     * @param message
     * @param lfNum
     */
    public static void printOut(String message, int lfNum) {
        if (!VERBOSE) return;

        System.out.print(message);
        if (lfNum > 0) {
            StringBuilder lf = new StringBuilder();
            while (lfNum > 0) {
                lf.append("\n");
                lfNum--;
            }
            System.out.print(lf);
        }
    }

    /**
     *
     */
    public void stopServer() {

        try {
            TimeUnit.SECONDS.sleep(10);
        } catch (InterruptedException e) {
            System.err.println(e.getMessage());
        }
        server.stop(0);
    }

    /**
     *
     * @param appName
     * @param appId
     * @param rr
     * @param appPoa
     * @return
     */
    public int createApplication(String appName, int appId, boolean rr, String appPoa) {
        printOut("Creating Application \"" + appName + "\"... ", 0);
        JSONObject obj = new JSONObject()
                .put("rn", appName)
                .put("api", appId)
                .put("rr", rr);
        if (appPoa != null) {
            obj.put("poa", appPoa);
        }
        JSONObject resource = new JSONObject()
                .put("m2m:ae", obj);

        HttpResponse httpResponse;
        int code, tries = 0;
        do {
            httpResponse = RestHttpClient.post(
                    originator,
                    csePoa + "/~/" + cseId + "/" + cseName,
                    resource.toString(),
                    2
            );
            code = httpResponse.getStatusCode();

            if (code == 409) {
                    checkHttpCode(code);
                    printOut("Deleting and trying again... ", 0);
                deleteMonitor();
            }
            tries++;
        } while (code != 201 && tries < 10);

        return checkHttpCode(code);
    }

    /**
     *
     * @param appName
     * @param cntName
     * @return
     */
    public int createContainer(String appName, String cntName) {
        printOut("Creating Container \"" + appName + "/" + cntName + "\"... ", 0);

        JSONObject resource = new JSONObject()
                .put("m2m:cnt", new JSONObject()
                    .put("rn", cntName));

        HttpResponse httpResponse = RestHttpClient.post(
                originator,
                csePoa + "/~/" + cseId + "/" + cseName + "/" + appName,
                resource.toString(),
                3
        );

        return checkHttpCode(httpResponse.getStatusCode());
    }

    /**
     *
     */
    public void deleteMonitor() {
        RestHttpClient.delete(
                originator,
                csePoa + "/~/" + cseId + "/" + cseName + "/" + aeMonitorName
        );
    }

    /**
     *
     * @param aeName
     * @param cntName
     * @return
     */
    public int createSub(String aeName, String cntName) {
        printOut("Creating subscription for \"" + aeName + "/" + cntName + "\"... ", 0);

        JSONObject sub = new JSONObject()
                .put("m2m:sub", new JSONObject()
                        .put("nu", new JSONArray()
                                .put("/" + cseId + "/" + cseName + "/" + aeMonitorName))
                        .put("rn", subName)
                        .put("nct", 2));

        HttpResponse httpResponse;
        int code, tries = 0;
        do {
            httpResponse = RestHttpClient.post(
                    originator,
                    csePoa + "/~/" + cseId + "/" + cseName + "/" + aeName + "/" + cntName,
                    sub.toString(),
                    23
            );
            code = httpResponse.getStatusCode();

            if (code == 409) {
                checkHttpCode(code);
                printOut("Deleting and trying again... ", 0);
                deleteSub(aeName, cntName);
            }
            tries++;
        } while (code != 201 && !flagSubscription && tries < 10);

        return checkHttpCode(code);
    }

    /**
     *
     * @param aeName
     * @param cntName
     */
    public void deleteSub(String aeName, String cntName) {
        RestHttpClient.delete(
                originator,
                csePoa + "/~/" + cseId + "/" + cseName + "/" + aeName + "/" + cntName + "/" + subName
        );
    }

    /**
     *
     * @param data
     * @param aeName
     * @param containerName
     * @param contentName
     * @return
     */
    public long createContentInstance(String data, String aeName, String containerName, String contentName) {
        printOut("Creating content instance \"" + contentName + "\"... ", 0);

        JSONObject obj = new JSONObject();
        obj.put("rn", contentName);
        obj.put("pc", "cenas_teste");
        obj.put("cnf", "application/json");
        obj.put("con", data);
        JSONObject resource = new JSONObject();
        resource.put("m2m:cin", obj);

        long nanoTimerStart = System.nanoTime();

        HttpResponse httpResponse = RestHttpClient.post(
                originator,
                csePoa + "/~/" + cseId + "/" + cseName + "/" + aeName + "/" + containerName, resource.toString(),
                4
        );

        checkHttpCode(httpResponse.getStatusCode());
        long nanoTimerEnd = System.nanoTime();

        return nanoTimerEnd - nanoTimerStart;
    }

    /**
     *
     * @return
     */
    private String getWirelessAddress() {
        String ip = null;

        String displayName;

        try {
            Enumeration<NetworkInterface> interfaces = NetworkInterface.getNetworkInterfaces();
            while (interfaces.hasMoreElements()) {
                NetworkInterface iface = interfaces.nextElement();
                // filters out 127.0.0.1 and inactive interfaces
                if (iface.isLoopback() || !iface.isUp())
                    continue;

                Enumeration<InetAddress> addresses = iface.getInetAddresses();
                while(addresses.hasMoreElements()) {
                    InetAddress addr = addresses.nextElement();

                    if (addr instanceof Inet6Address)
                        continue;

                    displayName = iface.getDisplayName().toLowerCase();
                    if (
                        displayName.contains("wireless")
                        || displayName.contains("wifi")
                        || displayName.contains("wi-fi")
                        || displayName.contains("802.11")
                    ) {
                        ip = addr.getHostAddress();
                        break;
                    }
                }
            }
        } catch (SocketException e) {
            throw new RuntimeException(e);
        }

        return ip;
    }

    /**
     *
     * @param code
     * @return
     */
    private int checkHttpCode(int code) {
        if (code == 201) {
            printOut("created.", 1);
        } else if (code == 409) {
            printOut("already exists.", 1);
        } else {
            printOut("failed. (" + code + ")", 1);
        }

        return code;
    }

    /**
     *
     * @param db
     * @param query
     * @param database
     */
    private static void execQuery(InfluxDB db, String query, String database) {

        db.query(new Query(query, database), queryResult -> {
        }, throwable -> {
            System.err.print("Query error: ");
            System.err.println(throwable.toString());
        });

    }

    /**
     *
     * @param database
     * @param username
     * @param password
     * @return
     */
    private static InfluxDB getDatabase(String database, String username, String password) {

        CloseableHttpClient httpClient = HttpClients.createDefault();
        HttpGet httpGet = new HttpGet(dbPoa + "/ping");

        try {
            try (CloseableHttpResponse closeableHttpResponse = httpClient.execute(httpGet)) {
                HttpEntity entityHTTP = closeableHttpResponse.getEntity();
                EntityUtils.consume(entityHTTP);
            }
        } catch (HttpHostConnectException e) {
            System.err.println("Can't connect to database.");
            System.exit(-1);
        } catch (IOException e) {
            e.printStackTrace();
        }

        InfluxDB db = InfluxDBFactory.connect(dbPoa, username, password);
//        db.setLogLevel(InfluxDB.LogLevel.BASIC);
        execQuery(
                db,
                "CREATE DATABASE \"" + database + "\"", ""
        );
        execQuery(
                db,
                "CREATE RETENTION POLICY \"default\" ON \"" + database + "\" DURATION 30d REPLICATION 1 DEFAULT",
                database
        );

        db.setDatabase(database);

        return db;
    }

    /**
     *
     * @param db
     * @param database
     * @param rpName
     * @param sensor
     * @param con
     * @param e2e
     */
    private static void insertDB(InfluxDB db, String database, String rpName, String sensor, String con, long e2e) {

        String[] values = con.split(":");
        double value;
        try {
            value = Double.parseDouble(values[0]);
        } catch (NumberFormatException e) {
            System.err.println("Couldn't parse measurement. (" + values[0] + ")");
            return;
        }
        long delay;
        try {
            delay = (long) Double.parseDouble(values[1]);
        } catch (NumberFormatException e) {
            System.err.println("Couldn't parse delay. (" + values[1] + ")");
            return;
        }

        String message = "Writing sample #" + (counter++) + " to database: "
                + "\tsensor = " + sensor
                + "\tmeasurement = " + value
                + "\tsensor_delay = " + delay
                + "\tmonitor_delay = " + avgNetworkDelayNanos
                + "\te2e_delay = " + e2e;
        printOut(message, 2);

        try {
            db.write(
                    database,
                    rpName,
                    Point.measurement("Heartbeats")
                            .time(System.currentTimeMillis(), TimeUnit.MILLISECONDS)
                            .addField("sensor", sensor)
                            .addField("measurement", value)
                            .addField("sensor_delay", delay)
                            .addField("monitor_delay", avgNetworkDelayNanos)
                            .addField("e2e_delay", e2e)
                            .build()
            );
        } catch (InfluxDBException.DatabaseNotFoundException e) {
            printOut("Database appears to have vanished. Recreating... ", 0);
            influxDB = getDatabase("SDIS", "sdis", "sdis_admin");
            printOut("created.", 1);
        } catch (InfluxDBException e) {
            printOut("Retention policy not found. Recreating... ", 0);
            influxDB = getDatabase("SDIS", "sdis", "sdis_admin");
            printOut("created.", 1);
        }
    }

    /**
     *
     * @param nanosElapsed
     * @param alpha
     */
    private static void updateNetworkDelay(long nanosElapsed, double alpha) {
        double newAvg = avgNetworkDelayNanos * (1 - alpha) + (nanosElapsed / 2.0) * alpha;
        avgNetworkDelayNanos = (long) newAvg;
    }

    /**
     *
     * @param portName
     * @throws Exception
     */
    private static void connect(String portName) throws Exception {

        CommPortIdentifier portIdentifier = CommPortIdentifier.getPortIdentifier(portName);

        if (portIdentifier.isCurrentlyOwned()) {
            System.err.println("USB port is currently in use. (" + portName + ")");
        } else {
            CommPort commPort = portIdentifier.open("M2MApp", 2000);

            if (commPort instanceof SerialPort) {
                SerialPort serialPort = (SerialPort) commPort;
                serialPort.setSerialPortParams(115200, SerialPort.DATABITS_8, SerialPort.STOPBITS_1,
                        SerialPort.PARITY_NONE);

                BufferedReader in = new BufferedReader(new InputStreamReader(serialPort.getInputStream()));

                String read;
                do {
                    long timestamp;
                    try {
                        read = in.readLine();
                    } catch (IOException e) {
                        continue;
                    }
                    timestamp = System.nanoTime();
                    printOut(read, 1);

                    String[] fields = read.split(";");
                    if (!fields[0].equals("Publish"))
                        continue;

                    try {
                        String info = fields[1].split(":")[1];
//                        long ts = (long) Double.parseDouble(fields[2].split(":")[1]);  // not used
                        usbTimes.put(info, timestamp);
                    } catch (NumberFormatException ignored) {}

                } while (true);
            } else {
                System.err.println("Trying to read a port that is not serial. (" + portName + ")");
            }
        }
    }


    public static void main(String[] args) {

        printOut("oM2M PoA: " + csePoa, 1);
        printOut("Publisher/Subscriber PoA: " + aePoa, 1);

        String wirelessAddress;
        if ((wirelessAddress = M2MApp.getInstance().getWirelessAddress()) != null) {
            printOut("Using address: " + wirelessAddress, 1);
            aeIp = wirelessAddress;
        	aePoa = aeProtocol + "://" + aeIp + ":" + aePort; // update app poa
            dbPoa = "http://" + aeIp + ":8086";
        }

        influxDB = getDatabase("SDIS", "sdis", "sdis_admin");

        M2MApp.getInstance().startServer(1);

        // Create Monitor application
        M2MApp.getInstance().createApplication("Monitor", 123456, true, aePoa);

        // Subscribe to sensor data
        M2MApp.getInstance().createSub("ESP8266", "HR");

        // Start measuring network delay
        Executors.newSingleThreadExecutor().execute(() -> {
            int retCode = M2MApp.getInstance().createContainer("Monitor", "Pong");
            if(retCode == 201) {
                int i = 1;
                while (true) {
                    long nanosElapsed = M2MApp.getInstance().createContentInstance(
                            String.valueOf(System.nanoTime()),
                            "Monitor",
                            "Pong",
                            String.valueOf(i++)
                    );

                    updateNetworkDelay(nanosElapsed, 0.1);

                    try {
                        Thread.sleep(500);
                    } catch (InterruptedException ignored) {}
                }
            } else {
                System.err.println("Could not start delay measurement: \n\tfailed to create \"Pong\" (" + retCode + ").");
                M2MApp.getInstance().stopServer();
                System.exit(-1);
            }
        });

        if (flagSubscription) {
            printOut("Opening USB serial port... ", 0);
            try {
                connect("COM4");
            } catch (Exception e) {
                System.err.println("\n" + e.getMessage());
                printOut("Shutting down... ", 0);
                M2MApp.getInstance().stopServer();
                printOut("bye.", 1);
                System.exit(-1);
            }
            printOut("done.", 1);
        }

//        String data = "Chuck Testa";
//        Random rand = new Random();
//        for (int i = 0; i < 10; i++) {
//            M2MApp.getInstance().createContentInstance(data, aeName, aeNamePub, String.valueOf(rand.nextInt()), true);
//            Thread.sleep(5000);
//        }

//        M2MApp.getInstance().stopServer();
//        System.exit(0);
    }

}