package org.eclipse.om2m.app;

import org.apache.http.HttpEntity;
import org.apache.http.client.methods.CloseableHttpResponse;
import org.apache.http.client.methods.HttpDelete;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.entity.StringEntity;
import org.apache.http.impl.client.CloseableHttpClient;
import org.apache.http.impl.client.HttpClients;
import org.apache.http.util.EntityUtils;

/**
 * Created by Carlos Pereira on 11/08/2017.
 */

/**
 * Implements GET; POST; DELETE;
 * PUT is currently not implemented as OM2M does not allow to update contentInstances
 */

public class RestHttpClient {

    /**
     * HTTP GET
     *
     * @param originator user and pass
     * @param uri        destination
     * @return HTTPResponse
     */
    public static HttpResponse get(String originator, String uri) {
        CloseableHttpClient httpclient = HttpClients.createDefault();
        HttpGet httpGet = new HttpGet(uri);

        httpGet.addHeader("X-M2M-Origin", originator);
        httpGet.addHeader("Accept", "application/json");

        HttpResponse httpResponse = new HttpResponse();

        try {
            CloseableHttpResponse closeableHttpResponse = httpclient.execute(httpGet);
            try {
                httpResponse.setStatusCode(closeableHttpResponse.getStatusLine().getStatusCode());
                httpResponse.setBody(EntityUtils.toString(closeableHttpResponse.getEntity()));
                HttpEntity entityHTTP = closeableHttpResponse.getEntity();
                EntityUtils.consume(entityHTTP);
            } finally {
                closeableHttpResponse.close();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return httpResponse;
    }

    /**
     * HTTP POST
     *
     * @param originator user and pass
     * @param uri        destination
     * @param body       data
     * @param ty         resource type
     * @return HTTPResponse
     */
    public static HttpResponse post(String originator, String uri, String body, int ty) {

        CloseableHttpClient httpclient = HttpClients.createDefault();
        HttpPost httpPost = new HttpPost(uri);

        httpPost.addHeader("X-M2M-Origin", originator);
        httpPost.addHeader("Content-Type", "application/json;ty=" + ty);
        httpPost.addHeader("Accept", "application/json");

        HttpResponse httpResponse = new HttpResponse();
        try {
            CloseableHttpResponse closeableHttpResponse = null;
            try {
                httpPost.setEntity(new StringEntity(body));
                closeableHttpResponse = httpclient.execute(httpPost);
                httpResponse.setStatusCode(closeableHttpResponse.getStatusLine().getStatusCode());
                httpResponse.setBody(EntityUtils.toString(closeableHttpResponse.getEntity()));
                HttpEntity entityHTTP = closeableHttpResponse.getEntity();
                EntityUtils.consume(entityHTTP);
            } finally {
                closeableHttpResponse.close();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return httpResponse;
    }

    /**
     * HTTP Delete
     *
     * @param originator user and pass
     * @param uri        destination
     * @return HTTPResponse
     */
    public static HttpResponse delete(String originator, String uri) {
        System.out.println("HTTP DELETE " + uri);

        CloseableHttpClient httpclient = HttpClients.createDefault();
        HttpDelete httpDelete = new HttpDelete(uri);

        httpDelete.addHeader("X-M2M-Origin", originator);
        httpDelete.addHeader("Accept", "application/json");

        HttpResponse httpResponse = new HttpResponse();
        try {
            CloseableHttpResponse closeableHttpResponse = null;
            try {
                closeableHttpResponse = httpclient.execute(httpDelete);
                httpResponse.setStatusCode(closeableHttpResponse.getStatusLine().getStatusCode());
                httpResponse.setBody(EntityUtils.toString(closeableHttpResponse.getEntity()));
                HttpEntity entityHTTP = closeableHttpResponse.getEntity();
                EntityUtils.consume(entityHTTP);
            } finally {
                closeableHttpResponse.close();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        System.out.println("HTTP Response " + httpResponse.getStatusCode() + "\n" + httpResponse.getBody());
        return httpResponse;
    }
}