package de.rwth_aachen.comsys.assignment2.data;

import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.w3c.dom.ls.DOMImplementationLS;
import org.w3c.dom.ls.LSSerializer;
import org.xml.sax.EntityResolver;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Serializable;
import java.io.StringWriter;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.transform.TransformerConfigurationException;
import javax.xml.transform.TransformerException;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamResult;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.Transformer;
import javax.xml.xpath.XPath;
import javax.xml.xpath.XPathConstants;
import javax.xml.xpath.XPathExpressionException;
import javax.xml.xpath.XPathFactory;

/**
 * (C) Copyright 2015 Simon Peter Grätzer
 * All rights reserved.
 */
public class MealPlan implements Serializable {
    private static final String TAG = MealPlan.class.getSimpleName();

    public class Day implements Serializable {
        private Pattern datePattern = Pattern.compile("([0-9][0-9])\\.([0-9][0-9])\\.([0-9][0-9][0-9][0-9])");

        public boolean IsToday() {
            Calendar calendar = Calendar.getInstance();
            String test = String.format("%02d.%02d.%04d", calendar.get(Calendar.DAY_OF_MONTH), calendar.get(Calendar.MONTH)+1, calendar.get(Calendar.YEAR));
            Log.d(TAG, test);
            return name.contains(test);
        }

        public String name;
        public final List<Menu> menus = new ArrayList<>();
    }

    public class Menu {
        public String category = "", title = "";
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
        //XPath xPath =  XPathFactory.newInstance().newXPath();
        Document document = builder.parse(in);

        final String weekdayIds[] = {"montag", "dienstag", "mittwoch", "donnerstag", "freitag"};
        final String nextSuffix = "Naechste";

        for (String dayId : weekdayIds) {
            Day d = parseDayElement(document, dayId);
            if (d != null && d.menus.size() > 0) days.add(d);

        }
        for (String dayId : weekdayIds) {
            Day d = parseDayElement(document, dayId+nextSuffix);
            if (d != null && d.menus.size() > 0) days.add(d);
        }

        for(Day day : days) {
            if(!day.IsToday())
                continue;

            todayId = days.indexOf(day);
            break;
        }
    }

    private Day parseDayElement(Document document, String dayId) throws XPathExpressionException, TransformerConfigurationException, TransformerException {
        Day day = new Day();

        // Aquiring the date
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
        NodeList tds = el.getElementsByTagName("td");

        //NodeList tds = (NodeList)xPath.compile("//td[@class='menue-wrapper']")
        //        .evaluate(el, XPathConstants.NODESET);
        if (tds == null) return null;

        for (int i = 0; i < tds.getLength(); i++) {
            NodeList spans = ((Element)tds.item(i)).getElementsByTagName("span");
            if (spans == null) continue;

            MealPlan.Menu menu = new Menu();
            for (int x = 0; x < spans.getLength(); x++) {
                Element span = (Element) spans.item(x);
                String type = span.getAttribute("class");

                if (type != null && type.contains("menue-category")) {
                    menu.category = sanitize(span.getTextContent());
                } else if (type != null && type.contains("menue-desc")) {
                    menu.title = sanitize(span.getTextContent());
                } else if (type != null && type.contains("menue-price")) {
                    String in = span.getTextContent();
                    in = in.replace('€', ' ');
                    in = in.replace(',', '.');
                    menu.price = Double.parseDouble(in);
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
