package de.rwth_aachen.comsys.assignment2;

import android.animation.LayoutTransition;
import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;
import android.app.ListFragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.BaseAdapter;
import android.widget.ListView;
import android.widget.TextView;


import de.rwth_aachen.comsys.assignment2.data.Mensa;

public class MensaListFragment extends ListFragment {

    private static final String STATE_ACTIVATED_POSITION = "activated_position";

    /**
     * The fragment's current callback object, which is notified of list item
     * clicks.
     */
    private Callbacks mCallbacks = sDummyCallbacks;

    /**
     * The current activated item position. Only used on tablets.
     */
    private int mActivatedPosition = ListView.INVALID_POSITION;

    /**
     * A callback interface that all activities containing this fragment must
     * implement. This mechanism allows activities to be notified of item
     * selections.
     */
    public interface Callbacks {
        public void onMensaSelected(String id);
        public void onAboutSelected();
    }

    private static Callbacks sDummyCallbacks = new Callbacks() {
        @Override
        public void onMensaSelected(String id) {
        }

        @Override
        public void onAboutSelected() {
        }
    };

    /**
     * Mandatory empty constructor for the fragment manager to instantiate the
     * fragment (e.g. upon screen orientation changes).
     */
    public MensaListFragment() {
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setListAdapter(new MensaAdapter());
        /*setListAdapter(new ArrayAdapter<Mensa>(
                getActivity(),
                R.layout.simple_list_item_activated_1,
                android.R.id.text1,
                Mensa.ITEMS));*/
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        // Restore the previously serialized activated item position.
        if (savedInstanceState != null
                && savedInstanceState.containsKey(STATE_ACTIVATED_POSITION)) {
            setActivatedPosition(savedInstanceState.getInt(STATE_ACTIVATED_POSITION));
        }
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);

        // Activities containing this fragment must implement its callbacks.
        if (!(activity instanceof Callbacks)) {
            throw new IllegalStateException("Activity must implement fragment's callbacks.");
        }

        mCallbacks = (Callbacks) activity;
    }

    @Override
    public void onDetach() {
        super.onDetach();

        // Reset the active callbacks interface to the dummy implementation.
        mCallbacks = sDummyCallbacks;
    }

    @Override
    public void onListItemClick(ListView listView, View view, int position, long id) {
        super.onListItemClick(listView, view, position, id);

        if (position < Mensa.ITEMS.size()) {
            mCallbacks.onMensaSelected(Mensa.ITEMS.get(position).id);
        } else {
            mCallbacks.onAboutSelected();
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        if (mActivatedPosition != ListView.INVALID_POSITION) {
            // Serialize and persist the activated item position.
            outState.putInt(STATE_ACTIVATED_POSITION, mActivatedPosition);
        }
    }

    /**
     * Turns on activate-on-click mode. When this mode is on, list items will be
     * given the 'activated' state when touched.
     */
    public void setActivateOnItemClick(boolean activateOnItemClick) {
        // When setting CHOICE_MODE_SINGLE, ListView will automatically
        // give items the 'activated' state when touched.
        getListView().setChoiceMode(activateOnItemClick
                ? ListView.CHOICE_MODE_SINGLE
                : ListView.CHOICE_MODE_NONE);
    }

    public void setActivatedPosition(int position) {
        if (position == ListView.INVALID_POSITION) {
            getListView().setItemChecked(mActivatedPosition, false);
            //getListView().setSelection(mActivatedPosition);
        } else {
            //getListView().setSelection(position);
            getListView().setItemChecked(position, true);
        }
        mActivatedPosition = position;
    }

    private class MensaAdapter extends BaseAdapter {

        @Override
        public int getCount() {
            return Mensa.ITEMS.size()+1;
        }

        @Override
        public Object getItem(int position) {
            return position <  Mensa.ITEMS.size() ? Mensa.ITEMS.get(position) : "About";
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            if (convertView == null) {
                convertView = LayoutInflater.from(getActivity())
                        .inflate(R.layout.simple_list_item_activated_1, parent, false);
            }
            TextView textView = (TextView)convertView.findViewById(android.R.id.text1);
            Object item = getItem(position);
            if (item instanceof Mensa) {
                textView.setText(((Mensa)item).id);
            } else {
                textView.setText(R.string.about);
                textView.setCompoundDrawablesWithIntrinsicBounds(
                        R.drawable.ic_action_about, 0, 0, 0);
            }
            return convertView;
        }
    }
}
