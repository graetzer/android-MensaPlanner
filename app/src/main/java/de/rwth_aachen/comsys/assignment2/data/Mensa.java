package de.rwth_aachen.comsys.assignment2.data;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * (C) Copyright 2015 Simon Peter Grätzer
 * All rights reserved.
 */
public class Mensa {
    public final String id, street, url;

    public Mensa(String id, String street, String url) {
        this.id = id;
        this.street = street;
        this.url = url;
    }

    @Override
    public String toString() {
        return id;
    }

    public static List<Mensa> ITEMS = new ArrayList<Mensa>();
    private static Map<String, Mensa> ITEMS_MAP = new HashMap<String, Mensa>();
    static {
        addItem(new Mensa("Mensa Academica", "Aachen Pontwall 3", "http://www.studentenwerk-aachen.de/speiseplaene/academica-w.html"));
        addItem(new Mensa("Mensa Ahornstraße", "Aachen Ahornstraße 55", "http://www.studentenwerk-aachen.de/speiseplaene/ahornstrasse-w.html"));
        addItem(new Mensa("Mensa Bistro Templergraben", "Aachen Templergraben 55", "http://www.studentenwerk-aachen.de/speiseplaene/templergraben-w.html"));
        addItem(new Mensa("Mensa Bayernallee", "Aachen Bayernallee 9", "http://www.studentenwerk-aachen.de/speiseplaene/bayernallee-w.html"));
        addItem(new Mensa("Mensa Eupener Straße", "Aachen Eupener Straße 70", "http://www.studentenwerk-aachen.de/speiseplaene/eupenerstrasse-w.html"));
        addItem(new Mensa("Mensa Goethestraße", "Aachen Goethestraße 3", "http://www.studentenwerk-aachen.de/speiseplaene/goethestrasse-w.html"));
        addItem(new Mensa("Mensa Vita", "Aachen Helmertweg 1", "http://www.studentenwerk-aachen.de/speiseplaene/vita-w.html"));
        addItem(new Mensa("Mensa Jülich", "Aachen Heinrich-Mußmann-Str. 1", "http://www.studentenwerk-aachen.de/speiseplaene/juelich-w.html"));
    }
    private static void addItem(Mensa m) {
        ITEMS.add(m);
        ITEMS_MAP.put(m.id, m);
    }
    public static Mensa getMensaWithId(String id) {
        return ITEMS_MAP.get(id);
    }
}
