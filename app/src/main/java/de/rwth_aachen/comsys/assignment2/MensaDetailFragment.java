package de.rwth_aachen.comsys.assignment2;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;


import de.rwth_aachen.comsys.assignment2.data.Mensa;

public class MensaDetailFragment extends Fragment {
    public static final String ARG_ITEM_ID = "mensa_id";
    private Mensa mMensa;

    public static MensaDetailFragment newInstance(String id) {
        Bundle arguments = new Bundle();
        arguments.putString(ARG_ITEM_ID, id);
        MensaDetailFragment fragment = new MensaDetailFragment();
        fragment.setArguments(arguments);
        return fragment;
    }

    public MensaDetailFragment() {
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (getArguments().containsKey(ARG_ITEM_ID)) {
            // Load the dummy content specified by the fragment
            // arguments. In a real-world scenario, use a Loader
            // to load content from a content provider.
            mMensa = Mensa.getMensaWithId(getArguments().getString(ARG_ITEM_ID));
        }
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        View rootView = inflater.inflate(R.layout.fragment_mensa_detail, container, false);

        // Show the dummy content as text in a TextView.
        if (mMensa != null) {
            ((TextView) rootView.findViewById(R.id.mensa_detail)).setText(mMensa.name);
        }

        return rootView;
    }
}
