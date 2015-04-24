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
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
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

    //private static XPathFactory X_FACTORY = XPathFactory.newInstance();
    private void parseData(InputStream in) throws Exception {
        DocumentBuilderFactory builderFactory =
                DocumentBuilderFactory.newInstance();
        DocumentBuilder builder = builderFactory.newDocumentBuilder();
        builder.setEntityResolver(new EntityResolver() {
            @Override
            public InputSource resolveEntity(String publicId, String systemId)
                    throws SAXException, IOException {
                if (systemId.contains("foo.dtd")) {
                    return new InputSource(new StringReader(""));
                } else {
                    return null;
                }
            }
        });

        Document document = builder.parse(in);

        final String weekdayIds[] = {"montag", "dienstag", "mittwoch", "donnerstag", "freitag"};
        final String nextSuffix = "Naechste";
        //XPath xPath =  X_FACTORY.newXPath();

        for (String dayId : weekdayIds) {
            parseDayElement(document, dayId);
        }
        for (String dayId : weekdayIds) {
            parseDayElement(document, dayId+nextSuffix);
        }
        /*String exp = "/Employees/Employee[@emplid='3333']/email";
        Node email = (Node) xPath.compile(exp).evaluate(document, XPathConstants.NODE);*/
    }

    private void parseDayElement(Document document, String dayId) {
        Day day = new Day();
        day.name = capitalize(dayId);// Todo

        Element el = document.getElementById(dayId);
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
        days.add(day);
    }

    static {
        //System.loadLibrary("hello"); // Load native library at runtime
    }
    private native byte[] requestUrl(String host, String path);

    private String capitalize(final String line) {
        return Character.toUpperCase(line.charAt(0)) + line.substring(1);
    }

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
