package de.rwth_aachen.comsys.assignment2;

import android.app.ListFragment;
import android.app.ProgressDialog;
import android.content.Context;
import android.os.AsyncTask;
import android.os.Bundle;
import android.app.Fragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.BaseAdapter;
import android.widget.TextView;


import java.io.IOException;
import java.text.NumberFormat;
import java.util.List;
import java.util.Locale;

import de.rwth_aachen.comsys.assignment2.data.MealPlan;
import de.rwth_aachen.comsys.assignment2.data.Mensa;

public class MensaDetailFragment extends ListFragment {
    private static final String TAG = MensaDetailFragment.class.getSimpleName();
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
        return inflater.inflate(R.layout.fragment_mensa_detail, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        if (mMensa != null) {
            new MealPlanTask().execute(mMensa);
        }
    }

    private class MealPlanTask extends AsyncTask<Mensa, Void, MealPlan> {
        ProgressDialog progress;

        @Override
        protected void onPreExecute() {
            super.onPreExecute();
            progress = ProgressDialog.show(getActivity(), "Loading the meal plan",
                    "please wait a little...", true, false);
        }

        @Override
        protected MealPlan doInBackground(Mensa... params) {
            try {
                return new MealPlan(getResources().getAssets().open("mensa.html"));
            } catch (IOException e) {
                Log.e(TAG, "Error while reading asset mensa.html", e);
            }
            return null;
            //return new MealPlan(params[0].url);
        }

        @Override
        protected void onPostExecute(MealPlan mealPlan) {
            super.onPostExecute(mealPlan);
            progress.cancel();
            if (getActivity() != null
                    && mealPlan != null
                    && mealPlan.days.size() > 0) {
                List<MealPlan.Menu> menus = mealPlan.days.get(mealPlan.todayId).menus;
                setListAdapter(new MenuAdapter(getActivity(), menus));
            }
        }
    }

    private static class MenuAdapter extends BaseAdapter {
        Context ctx;
        private List<MealPlan.Menu> mMenus;
        NumberFormat format = NumberFormat.getCurrencyInstance(Locale.GERMANY);

        public MenuAdapter(Context ctx, List<MealPlan.Menu> menus) {
            this.ctx = ctx;
            this.mMenus = menus;
        }

        @Override
        public int getCount() {
            return mMenus.size();
        }

        @Override
        public Object getItem(int position) {
            return mMenus.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            if (convertView == null) {
                convertView = LayoutInflater.from(ctx).inflate(R.layout.course_list_item, parent, false);
            }
            TextView categoryTV = (TextView) convertView.findViewById(R.id.textView_category);
            TextView menuTV = (TextView) convertView.findViewById(R.id.textView_menu);
            TextView priceTV = (TextView) convertView.findViewById(R.id.textView_price);

            MealPlan.Menu m = mMenus.get(position);
            categoryTV.setText(m.category);
            menuTV.setText(m.title);
            priceTV.setText(format.format(m.price));

            return convertView;
        }
    }

}
