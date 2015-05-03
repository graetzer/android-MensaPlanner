package de.rwth_aachen.comsys.assignment2;

import android.app.Fragment;
import android.app.ListFragment;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.support.v4.view.PagerAdapter;
import android.support.v4.view.ViewPager;
import android.text.Html;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ListView;
import android.widget.TextView;


import java.text.NumberFormat;
import java.util.List;
import java.util.Locale;

import de.rwth_aachen.comsys.assignment2.data.MealPlan;
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
            getActivity().setTitle(mMensa.id);
        }
        setHasOptionsMenu(true);
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

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        super.onCreateOptionsMenu(menu, inflater);
        inflater.inflate(R.menu.detail, menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.action_map) {
            String map = "http://maps.google.co.in/maps?q=" + mMensa.id;
            Intent i = new Intent(Intent.ACTION_VIEW, Uri.parse(map));
            startActivity(i);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private class MealPlanTask extends AsyncTask<Mensa, Void, MealPlan> {
        private ProgressDialog mProgress;

        @Override
        protected void onPreExecute() {
            super.onPreExecute();
            mProgress = ProgressDialog.show(getActivity(), "Pläne werden geladen",
                    "Bitte warten", true, false);
        }

        @Override
        protected MealPlan doInBackground(Mensa... params) {
            return new MealPlan(params[0].url);
        }

        @Override
        protected void onPostExecute(MealPlan mealPlan) {
            super.onPostExecute(mealPlan);
            mProgress.cancel();
            if (getActivity() != null
                    && mealPlan != null
                    && mealPlan.days.size() > 0) {

                DaysPagerAdapter adapter = new DaysPagerAdapter(getActivity(), mealPlan.days);
                ViewPager pager = (ViewPager) getView();
                pager.setAdapter(adapter);
                pager.setCurrentItem(mealPlan.todayId);
            }
        }
    }

    private static class DaysPagerAdapter extends PagerAdapter {
        private Context mCtx;
        private List<MealPlan.Day> mDays;

        public DaysPagerAdapter(Context ctx, List<MealPlan.Day> days) {
            this.mCtx = ctx;
            this.mDays = days;
        }

        @Override
        public int getCount() {
            return mDays.size();
        }

        @Override
        public boolean isViewFromObject(View view, Object object) {
            return view == object;
        }

        @Override
        public Object instantiateItem(ViewGroup container, int position) {
            MealPlan.Day day = mDays.get(position);
            View view = LayoutInflater.from(mCtx).inflate(R.layout.menu_list, container, false);
            ListView list = (ListView)view.findViewById(android.R.id.list);
            list.setAdapter(new MenuListAdapter(mCtx, day.menus));
            list.setEmptyView(view.findViewById(android.R.id.empty));
            container.addView(view);
            return view;
        }

        @Override
        public void destroyItem(ViewGroup container, int position, Object object) {
            container.removeView((View) object);
        }

        @Override
        public CharSequence getPageTitle(int position) {
            return mDays.get(position).name;
        }
    }

    private static class MenuListAdapter extends BaseAdapter {
        Context ctx;
        private List<MealPlan.Menu> mMenus;
        NumberFormat format = NumberFormat.getCurrencyInstance(Locale.GERMANY);

        public MenuListAdapter(Context ctx, List<MealPlan.Menu> menus) {
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
                convertView = LayoutInflater.from(ctx).inflate(R.layout.menu_list_item, parent, false);
            }
            TextView categoryTV = (TextView) convertView.findViewById(R.id.textView_category);
            TextView menuTV = (TextView) convertView.findViewById(R.id.textView_menu);
            TextView priceTV = (TextView) convertView.findViewById(R.id.textView_price);

            MealPlan.Menu m = mMenus.get(position);
            categoryTV.setText(m.category);
            menuTV.setText(Html.fromHtml(m.title));
            if (m.price > 0) priceTV.setText(format.format(m.price));
            else priceTV.setText("");

            if(position % 2 == 0)
                convertView.setBackgroundColor(0);
            else
                convertView.setBackgroundColor(Color.argb(0X66, 0xBB, 0xBB, 0xBB));

            return convertView;
        }
    }

}
