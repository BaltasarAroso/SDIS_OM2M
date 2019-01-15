package app.org.eclipse.om2m.app;

import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpServer;
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

import java.io.IOException;
import java.io.InputStream;
import java.io.PrintWriter;
import java.net.*;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.*;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;


/**
 * Modified by Carlos Pereira on 11/08/2017.
 * Created by João Cardoso on 11/03/2016.
 */

public class M2MApp {
    public static M2MApp instance = null;
    private static ExecutorService notService;
    private static InfluxDB influxDB;

    private static long avgNetworkDelayNanos = 0;
    private static double alpha = 0.1;

    public static String filesFolder = System.getProperty("user.dir");

    public static int numberThreads = 1;
    public static int frequency = 100;
    public static int size = 1;
    public static int run = 1;

    public static int counterReceptions = 0;

    public static HashMap<String, Long> tEpochsSub = null;

    private static HttpServer server = null;

    private static String originator = "admin:admin";
    private static String cseProtocol = "http";
    private static String cseIp = "192.168.137.1";
    private static int csePort = 8080;
    private static String cseId = "in-cse";
    private static String cseName = "dartes";

//    private static String aeNamePub = "Actuation";
//    private static int appPubId = 12345;
//
//    private static String aeNameMaster = "master";
//    private static int appMasterId = 67891;
//
//    private static String cntName = "HR";
//    private static String cntNameMaster = "ctrlcmd";

    private static String aeMonitorName = "Monitor";
    private static String aeProtocol = "http";
    private static String aeIp = "192.168.137.98";
    private static int aePort = 1600;
    private static String subName = aeMonitorName + "_sub";
    private static String targetCse = "in-cse";
    private static String aeName = "ESP8266";

    private static String csePoa = cseProtocol + "://" + cseIp + ":" + csePort;
    private static String appPoa = aeProtocol + "://" + aeIp + ":" + aePort;

    private static String dbPoa = "http://" + aeIp + ":8086";

    public static boolean flagSubscription = false;


    public M2MApp() {

    }

    public static M2MApp getInstance() {
        if (instance == null) {
            instance = new M2MApp();
        }
        return instance;
    }

    /**
     * Initializes Hashmaps
     * Creates notservice threads
     * Creates Server
     */
    public void startServer() {
        tEpochsSub = new HashMap<String, Long>();
        //create more threads for receptions...
        notService = Executors.newFixedThreadPool(numberThreads * 10);

        System.out.print("Starting server... ");

        server = null;
        try {
            server = HttpServer.create(new InetSocketAddress(aePort), 0);
        } catch (IOException e) {
            e.printStackTrace();
        }
        server.createContext("/", new MyHandlerM2M());
        server.setExecutor(notService);
        server.start();

        System.out.println("started.");
    }

    /**
     * Creates an Handler for receiving and processing notifications
     * <p>
     * Responds with an OK to the request
     */
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

//                System.out.println("Received: \n" + requestBody);  // DEBUG

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
                                        counterReceptions++;
                                        insertDB(influxDB, "SDIS", "default", aeName, cin.getString("con"));
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
     * Stops the server
     */
    public void stopServer() {

        try {
            TimeUnit.SECONDS.sleep(10);
        } catch (InterruptedException e) {
            System.out.println(e.getMessage());
        }
        server.stop(0);
    }

    /**
     * Writes files with metrics
     * Ends notservice threads
     */
    public void writeSubToFile() {
        System.out.println("tEpochsSub size: " + tEpochsSub.size());
        System.out.println("Wait a moment for notifications threads to end!");
        notService.shutdown();
        try {
            notService.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
            System.out.println("All notifications threads ended!");
            PrintWriter tSubWriter = null;

            try {
                tSubWriter = new PrintWriter(
                        filesFolder + "sub_2_" + "frequency_" + frequency + "_" + "size_" + size + "_Run" + run + ".txt",
                        "UTF-8"
                );
            } catch (Exception e) {
                e.printStackTrace();
            }

            Iterator subIt = tEpochsSub.entrySet().iterator();
            while (subIt.hasNext()) {
                Map.Entry pair = (Map.Entry) subIt.next();
                tSubWriter.println(pair.getKey() + "," + pair.getValue());
                subIt.remove(); // avoids a ConcurrentModificationException
            }

            tSubWriter.close();
            System.out.println("Ended!");
        } catch (InterruptedException e) {
            System.err.println("Exception: " + e.getMessage());
        }
    }

    /**
     * Sets a counter
     *
     * @param secCounter seconds
     */
    public void counterTime(int secCounter) throws InterruptedException {
        System.out.println("Counter");

        int timet = secCounter;
        long delay = timet * 1000;
        do {
            int minutes = timet / 60;
            int seconds = timet % 60;
            System.out.println(minutes + " minute(s), " + seconds + " second(s)");
            Thread.sleep(1000);
            timet = timet - 1;
            delay = delay - 1000;
        }
        while (delay != 0);
        System.out.println("Time's Up!");
    }


    public void addToEpochSub(String edge, long millis) {
        tEpochsSub.put(edge, millis);
    }

    /**
     * Creates a new application 
     *
     * @param resourceName          application
     * @param applicationId         application
     */
    public void createApplication(String resourceName, int applicationId) {
        System.out.print("Creating Application... ");

        JSONObject obj = new JSONObject();
        obj.put("rn", resourceName);
        obj.put("api", applicationId);
        obj.put("rr", false);
        JSONObject resource = new JSONObject();
        resource.put("m2m:ae", obj);
        HttpResponse httpResponse = RestHttpClient.post(originator, csePoa + "/~/" + cseId + "/" + cseName, resource.toString(), 2);

        checkHttpCode(httpResponse.getStatusCode());
    }

    /**
     * Creates a new container
     *
     * @param cntName container name
     */
    public int createContainer(String aeName, String cntName) {
        System.out.print("Creating Container \"" + aeName + "/" + cntName + "\"... ");

        JSONObject resource = new JSONObject()
                .put("m2m:cnt", new JSONObject()
                    .put("rn", cntName));

        HttpResponse httpResponse = RestHttpClient.post(
                originator,
                csePoa + "/~/" + cseId + "/" + cseName + "/" + aeName,
                resource.toString(),
                3
        );

        checkHttpCode(httpResponse.getStatusCode());

        return httpResponse.getStatusCode();
    }

    /**
     * Issues a creation of a new Monitor application
     *
     */
    public void createMonitor(String aeMonitorName) {

        System.out.print("Creating Monitor... ");
        JSONObject ae = new JSONObject()
                .put("m2m:ae", new JSONObject()
                    .put("rn", aeMonitorName)
                    .put("api", 12346)
                    .put("rr", true)
                    .put("poa", new JSONArray()
                            .put(appPoa)));

        HttpResponse httpResponse;
        int code;
        do {
            httpResponse = RestHttpClient.post(
                    originator,
                    csePoa + "/~/" + cseId + "/" + cseName,
                    ae.toString(),
                    2
            );
            code = httpResponse.getStatusCode();

            if (code == 409) {
                checkHttpCode(code);
                System.out.print("Deleting and trying again... ");
                deleteMonitor();
            }
        } while (code != 201);
        checkHttpCode(code);
    }

    /**
     * Delete Monitor application
     */
    public void deleteMonitor() {
        RestHttpClient.delete(
                originator,
                csePoa + "/~/" + cseId + "/" + cseName + "/" + aeMonitorName
        );
    }

    /**
     * Creates a new subscription
     *
     */
    public void createSub(String aeName, String cntName) {
        System.out.print("Creating subscription for \"" + aeName + "/" + cntName + "\"... ");

        JSONObject sub = new JSONObject()
                .put("m2m:sub", new JSONObject()
                        .put("nu", new JSONArray()
                                .put("/" + cseId + "/" + cseName + "/" + aeMonitorName))
                        .put("rn", subName)
                        .put("nct", 2));

        HttpResponse httpResponse;
        int code;
        do {
            httpResponse = RestHttpClient.post(
                    originator,
                    csePoa + "/~/" + targetCse + "/" + cseName + "/" + aeName + "/" + cntName,
                    sub.toString(),
                    23
            );
            code = httpResponse.getStatusCode();

            if (code == 409) {
                checkHttpCode(code);
                System.out.print("Deleting and trying again... ");
                deleteSub(aeName, cntName);
            }
        } while (code != 201 && !flagSubscription);
        checkHttpCode(code);
    }

    /**
     * Deletes subscription
     */
    public void deleteSub(String aeName, String cntName) {
        RestHttpClient.delete(
                originator,
                csePoa + "/~/" + targetCse + "/" + cseName + "/" + aeName + "/" + cntName + "/" + subName
        );
    }

    /**
     * Creates a new contentInstance and store there data
     *
     * @param data              data
     * @param containerName       container
     * @param contentName contentInstance
     */
    public long createContentInstance(String data, String aeName, String containerName, String contentName, boolean verbose) {
        if (verbose) {
            System.out.print("Creating content instance \"" + contentName + "\"... ");
        }

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

        if (verbose) {
            checkHttpCode(httpResponse.getStatusCode());
        }
        long nanoTimerEnd = System.nanoTime();

        return nanoTimerEnd - nanoTimerStart;
    }

    /**
     * Encounters the first available Wi-Fi IPv4 address, if any
     *
     * @return A string or null
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
     * Checks an HTTP response code and appends a corresponding message to stdout
     *
     * @param code  HTTP response code
     */
    private void checkHttpCode(int code) {
        if (code == 201) {
            System.out.println("created.");
        } else if (code == 409) {
            System.out.println("already exists.");
        } else {
            System.out.println("error. (" + code + ")");
        }
    }

    /**
     * Executes a query in the given database
     *
     * @param db        Database connection
     * @param query     Query to execute
     * @param database  Database
     */
    private static void execQuery(InfluxDB db, String query, String database) {

        db.query(new Query(query, database), queryResult -> {
//            System.out.println(queryResult.toString());  // DEBUG
        }, throwable -> {
            System.out.print("Query error: ");
            System.out.println(throwable.toString());
        });

    }

    /**
     * Establishes a connection to the database
     *
     * @param database  Database name
     * @param username  Database username
     * @param password  Database password
     *
     * @return InfluxDB connection
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
        db.setLogLevel(InfluxDB.LogLevel.BASIC);
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
     * @param db        InfluxDB connection
     * @param database  Database name
     * @param rpName    Retention policy
     * @param sensor    Sensor name
     * @param con       Content (measure)
     */
    private static void insertDB(InfluxDB db, String database, String rpName, String sensor, String con) {

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

        System.out.println("Writing to database: ");
        System.out.println("\tsensor = " + sensor);
        System.out.println("\tmeasurement = " + value);
        System.out.println("\tsensor_delay = " + delay * 1000);
        System.out.println("\tmonitor_delay = " + avgNetworkDelayNanos + "\n");

        try {
            db.write(
                    database,
                    rpName,
                    Point.measurement("Heartbeats")
                            .time(System.currentTimeMillis(), TimeUnit.MILLISECONDS)
                            .addField("sensor", sensor)
                            .addField("measurement", value)
                            .addField("sensor_delay", delay * 1000)
                            .addField("monitor_delay", avgNetworkDelayNanos)
                            .build()
            );
        } catch (InfluxDBException.DatabaseNotFoundException e) {
            System.out.print("Database appears to have vanished. Recreating... ");
            influxDB = getDatabase("SDIS", "sdis", "sdis_admin");
            System.out.println("recreated.");
        } catch (InfluxDBException e) {
            System.out.print("Retention policy not found. Recreating... ");
            influxDB = getDatabase("SDIS", "sdis", "sdis_admin");
            System.out.println("recreated.");
        }
    }


    private static void updateNetworkDelay(long nanosElapsed) {
        double newAvg = avgNetworkDelayNanos * (1 - alpha) + (nanosElapsed / 2.0) * alpha;
        avgNetworkDelayNanos = (long) newAvg;
    }


    public static void main(String[] args) {
    	
//    	System.out.println("Working Directory = " + System.getProperty("user.dir"));

//        if (args.length == 3) {
//            try {
//                frequency = Integer.parseInt(args[0]);
//                size = Integer.parseInt(args[1]);
//                run = Integer.parseInt(args[2]);
//            } catch (NumberFormatException e) {
//                System.err.println("Arguments" + args[0] + args[1] + args[2] + " must be integers.");
//                System.exit(1);
//            }
//        }

//        System.out.println("oM2M PoA: " + csePoa);
//        System.out.println("Publisher/Subscriber PoA: " + appPoa);
//        System.out.println("Number of Threads: " + numberThreads);
//        System.out.println("Folder for files: " + filesFolder);

        String wirelessAddress;
        if ((wirelessAddress = M2MApp.getInstance().getWirelessAddress()) != null) {
            System.out.println("Using address: " + wirelessAddress);
            aeIp = wirelessAddress;
        	appPoa = aeProtocol + "://" + aeIp + ":" + aePort; // update app poa
            dbPoa = "http://" + aeIp + ":8086";
        }

        influxDB = getDatabase("SDIS", "sdis", "sdis_admin");

        M2MApp.getInstance().startServer();

        // Create Monitor application
        M2MApp.getInstance().createMonitor("Monitor");

        // Subscribe to sensor data
        M2MApp.getInstance().createSub("ESP8266", "HR");

        // Start measuring network delay
        Executors.newSingleThreadExecutor().execute(() -> {
            if(M2MApp.getInstance().createContainer("Monitor", "Pong") == 201) {
                int i = 1;
                while (true) {
                    long nanosElapsed = M2MApp.getInstance().createContentInstance(
                            String.valueOf(System.nanoTime()),
                            "Monitor",
                            "Pong",
                            String.valueOf(i++),
                            false
                    );

                    updateNetworkDelay(nanosElapsed);

                    try {
                        Thread.sleep(16);
                    } catch (InterruptedException ignored) {}
                }
            } else {
                System.err.println("Could not start delay measurement.");
            }
        });

//        String data = "Chuck Testa";
//
//        Random rand = new Random();
//        for (int i = 0; i < 10; i++) {
//            M2MApp.getInstance().createContentInstance(data, aeName, aeNamePub, String.valueOf(rand.nextInt()), true);
//            Thread.sleep(5000);
//        }

        //M2MApp.getInstance().createApplication(aeNameMaster, appMasterId);

        // M2MApp.getInstance().createContainer(aeNameMaster, cntNameMaster);
        // M2MApp.getInstance().createContainer(aeNamePub, cntNamePub);


        // M2MApp.getInstance().createContainer(aeNameMaster, cntNameMaster);


        // M2MApp.getInstance().createContentInstance(String.valueOf(System.currentTimeMillis()), aeNameMaster, cntNameMaster, Long.toString(System.currentTimeMillis()));

        //System.currentTimeMillis()


        // M2MApp.getInstance().createContentInstance("0x", aeNameMaster, cntNameMaster, "Modem-sleep");


        //M2MApp.getInstance().createMonitor();
        
        

      /*  flagSubscription = true;  // start collecting times

        try {
            M2MApp.getInstance().counterTime(340); //120 265
        }catch (InterruptedException e){
            System.out.println(e.getMessage());
        }

        M2MApp.getInstance().stopServer();
        M2MApp.getInstance().writeSubToFile();
        System.exit(0);

        return;*/
    }

}