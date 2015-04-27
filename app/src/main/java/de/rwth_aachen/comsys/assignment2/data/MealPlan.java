package de.rwth_aachen.comsys.assignment2.data;

import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.EntityResolver;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.StringReader;
import java.io.UnsupportedEncodingException;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.xpath.XPath;
import javax.xml.xpath.XPathConstants;
import javax.xml.xpath.XPathExpressionException;
import javax.xml.xpath.XPathFactory;

/**
 * (C) Copyright 2015 Simon Peter Grätzer
 * All rights reserved.
 */
public class MealPlan {
    private static final String TAG = MealPlan.class.getSimpleName();

    public class Day {
        public String name;
        public final List<Menu> menus = new ArrayList<>();
    }

    public class Menu {
        public String category, title;
        public double price;
    }

    public int todayId = 0;
    public final List<Day> days = new ArrayList<>();

    /**
     * Don't call this shit on the main thread
     * @param url
     */
    public MealPlan(String url) {
        Uri u = Uri.parse(url);
        byte[] data = requestUrl(u.getHost(), u.getPath());

        if (data == null) {
            Log.d(TAG, "Data is null");
            return;
        }
        /*try {
            String str = new String(data, "UTF-8");
            Log.d(TAG, str);
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
        }*/
        try {
            ByteArrayInputStream in = new ByteArrayInputStream(data);
            parseData(sanitizeStream(in));
        } catch (Exception e) {
            Log.e(TAG, "Error while parsing data", e);
        }
    }

    /**
     * Probably also don't call this on the main thread
     * @param in
     */
    public MealPlan(InputStream in) {
        try {
            parseData(sanitizeStream(in));
        } catch (Exception e) {
            Log.e(TAG, "Error while parsing data", e);
        }
    }

    XPath xPath =  XPathFactory.newInstance().newXPath();
    DocumentBuilderFactory builderFactory = DocumentBuilderFactory.newInstance();
    private void parseData(InputStream in) throws Exception {
        DocumentBuilder builder = builderFactory.newDocumentBuilder();
        XPath xPath =  XPathFactory.newInstance().newXPath();
        Document document = builder.parse(in);

        final String weekdayIds[] = {"montag", "dienstag", "mittwoch", "donnerstag", "freitag"};
        final String nextSuffix = "Naechste";
        Calendar c = Calendar.getInstance();
        todayId = Math.max(c.get(Calendar.DAY_OF_WEEK), 5) ;

        for (String dayId : weekdayIds) {
            Day d = parseDayElement(document, dayId);
            if (d != null) days.add(d);

        }
        for (String dayId : weekdayIds) {
            Day d = parseDayElement(document, dayId+nextSuffix);
            if (d != null) days.add(d);
        }
    }

    private Day parseDayElement(Document document, String dayId) throws XPathExpressionException {
        Day day = new Day();

        String expr = String.format("//a[@data-anchor='#%s']", dayId);
        Node node = (Node)xPath.compile(expr).evaluate(document, XPathConstants.NODE);
        if (node instanceof Element) {
            day.name = ((Element)node).getTextContent();
            if (day.name != null) {
                day.name = day.name.trim();
            }
        }

        Element el = document.getElementById(dayId);
        if (el == null) return null;
        NodeList trs = el.getElementsByTagName("tr");
        for (int i = 0; i < trs.getLength(); i++) {
            Element tr = (Element) trs.item(i);
            NodeList tds = tr.getElementsByTagName("td");

            MealPlan.Menu menu = new Menu();
            for (int x = 0; x < tds.getLength(); x++) {
                Element td = (Element) tds.item(x);
                String type = td.getAttribute("class");
                switch (type) {
                    case "category":
                        menu.category = sanitize(td.getTextContent());
                        break;
                    case "menue":
                        menu.title = sanitize(td.getTextContent());
                        break;
                    case "price":
                        String in = td.getTextContent();
                        in = in.replace('€', ' ');
                        in = in.replace(',', '.');
                        menu.price = Double.parseDouble(in);
                        break;
                }
            }
            day.menus.add(menu);
        }
        return day;
    }

    static {
        System.loadLibrary("HttpClient"); // Load native library at runtime
    }
    private native byte[] requestUrl(String serverhost, String path);

    private String sanitize(String in) {
        return in != null ? in.trim() : "";
    }

    /**
     * No 3th party libraries means we gotta fix the malformed xml/html by ourself
     * @param in
     * @return new stream
     * @throws IOException
     */
    private InputStream sanitizeStream(InputStream in) throws IOException {
        InputStreamReader is = new InputStreamReader(in);
        StringBuilder sb=new StringBuilder();
        BufferedReader br = new BufferedReader(is);
        String read;
        while((read = br.readLine()) != null) {
            // See
            sb.append(read.replace("& ", "&amp; "));
        }
        return new ByteArrayInputStream(sb.toString().getBytes("UTF-8"));
    }
}
