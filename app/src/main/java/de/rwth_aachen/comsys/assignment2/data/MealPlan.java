package de.rwth_aachen.comsys.assignment2.data;

import android.net.Uri;
import android.util.Log;

import org.w3c.dom.Document;
import org.w3c.dom.Node;
import org.xml.sax.SAXException;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Date;
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
        public String day;
        List<Item> menus = new ArrayList<>();
    }

    public class Item {
        String category, menu;
        double price;
    }

    public final List<Day> days = new ArrayList<>();

    /**
     * Don't call this shit on the main thread
     * @param url
     */
    public MealPlan(String url) {
        Uri u = Uri.parse(url);
        byte[] data = requestUrl(u.getHost(), u.getPath());
        try {
            parseData(data);
        } catch (Exception e) {
            Log.e(TAG, "Error while parsing data", e);
        }
    }

    /**
     * Probably also don't call this on the main thread
     * @param data
     */
    public MealPlan(byte[] data) {
        try {
            parseData(data);
        } catch (Exception e) {
            Log.e(TAG, "Error while parsing data", e);
        }
    }

    private static XPathFactory X_FACTORY = XPathFactory.newInstance();
    private synchronized void parseData(byte[] data) throws Exception {
        DocumentBuilderFactory builderFactory =
                DocumentBuilderFactory.newInstance();
        DocumentBuilder builder = builderFactory.newDocumentBuilder();
        Document document = builder.parse(new ByteArrayInputStream(data));

        final String weekdays[] = {"montag", "dienstag", "mittwoch", "donnerstag", "freitag"};
        final String nextSuffix = "Naechste";
        XPath xPath =  X_FACTORY.newXPath();
        String expression = "/Employees/Employee[@emplid='3333']/email";

        //read a string value
        Node email = (Node) xPath.compile(expression).evaluate(document, XPathConstants.NODE);

    }

    static {
        System.loadLibrary("hello"); // Load native library at runtime
        // hello.dll (Windows) or libhello.so (Unixes)
    }
    private native byte[] requestUrl(String host, String path);
}
