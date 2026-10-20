package com.example.georescux.qtprototype;

import android.content.Context;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.os.Looper;
import android.util.Log;

/**
 * Android system LocationManager bridge for GeoRescuX Qt Prototype.
 * Provides hardware GNSS & network location telemetry directly to C++ via JNI.
 */
public class AndroidLocationBridge implements LocationListener {
    private static final String TAG = "GeoRescuX_Location";
    private static volatile AndroidLocationBridge sInstance;

    private final Context mContext;
    private final LocationManager mLocationManager;
    private boolean mIsTracking = false;

    // Native C++ JNI exports
    public static native void onNativeLocationChanged(
            double lat,
            double lon,
            double altitude,
            float accuracy,
            float speed,
            long timestampUtcMs
    );

    public static native void onNativeStateChanged(int stateCode);

    public static AndroidLocationBridge getInstance(Context context) {
        if (sInstance == null) {
            synchronized (AndroidLocationBridge.class) {
                if (sInstance == null) {
                    sInstance = new AndroidLocationBridge(context.getApplicationContext());
                }
            }
        }
        return sInstance;
    }

    private AndroidLocationBridge(Context context) {
        mContext = context;
        mLocationManager = (LocationManager) context.getSystemService(Context.LOCATION_SERVICE);
    }

    public boolean isLocationServicesEnabled() {
        if (mLocationManager == null) {
            return false;
        }
        try {
            boolean gps = mLocationManager.isProviderEnabled(LocationManager.GPS_PROVIDER);
            boolean network = mLocationManager.isProviderEnabled(LocationManager.NETWORK_PROVIDER);
            return gps || network;
        } catch (Exception e) {
            Log.e(TAG, "Error checking location providers: " + e.getMessage());
            return false;
        }
    }

    public synchronized void startLocationUpdates(long minTimeMs, float minDistanceM) {
        if (mLocationManager == null) {
            Log.w(TAG, "LocationManager not available on device.");
            onNativeStateChanged(7); // LocationUnavailable
            return;
        }

        try {
            mLocationManager.removeUpdates(this);
        } catch (Exception ignored) {
        }

        // 1. Immediately query last known location if fresh and available
        Location lastGps = null;
        Location lastNet = null;
        try {
            if (mLocationManager.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
                lastGps = mLocationManager.getLastKnownLocation(LocationManager.GPS_PROVIDER);
            }
        } catch (SecurityException se) {
            Log.w(TAG, "SecurityException reading GPS last known location (approximate-only permission): " + se.getMessage());
        } catch (Exception e) {
            Log.w(TAG, "Exception reading GPS last known location: " + e.getMessage());
        }

        try {
            if (mLocationManager.isProviderEnabled(LocationManager.NETWORK_PROVIDER)) {
                lastNet = mLocationManager.getLastKnownLocation(LocationManager.NETWORK_PROVIDER);
            }
        } catch (SecurityException se) {
            Log.w(TAG, "SecurityException reading Network last known location: " + se.getMessage());
        } catch (Exception e) {
            Log.w(TAG, "Exception reading Network last known location: " + e.getMessage());
        }

        Location best = lastGps;
        if (best == null || (lastNet != null && lastNet.getTime() > best.getTime())) {
            best = lastNet;
        }

        if (best != null) {
            onLocationChanged(best);
        }

        // 2. Register for updates on Main Looper independently for GPS and Network providers
        boolean gpsRegistered = false;
        boolean netRegistered = false;

        try {
            if (mLocationManager.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
                mLocationManager.requestLocationUpdates(
                        LocationManager.GPS_PROVIDER,
                        minTimeMs,
                        minDistanceM,
                        this,
                        Looper.getMainLooper()
                );
                gpsRegistered = true;
                Log.i(TAG, "Registered GPS_PROVIDER listener (interval: " + minTimeMs + "ms, minDistance: " + minDistanceM + "m)");
            }
        } catch (SecurityException se) {
            Log.w(TAG, "GPS_PROVIDER not permitted (user granted approximate-only location): " + se.getMessage());
        } catch (Exception e) {
            Log.w(TAG, "Could not register GPS_PROVIDER: " + e.getMessage());
        }

        try {
            if (mLocationManager.isProviderEnabled(LocationManager.NETWORK_PROVIDER)) {
                mLocationManager.requestLocationUpdates(
                        LocationManager.NETWORK_PROVIDER,
                        minTimeMs,
                        minDistanceM,
                        this,
                        Looper.getMainLooper()
                );
                netRegistered = true;
                Log.i(TAG, "Registered NETWORK_PROVIDER listener (interval: " + minTimeMs + "ms, minDistance: " + minDistanceM + "m)");
            }
        } catch (SecurityException se) {
            Log.w(TAG, "NETWORK_PROVIDER not permitted: " + se.getMessage());
        } catch (Exception e) {
            Log.w(TAG, "Could not register NETWORK_PROVIDER: " + e.getMessage());
        }

        if (!gpsRegistered && !netRegistered) {
            if (!isLocationServicesEnabled()) {
                Log.w(TAG, "No location providers enabled on system.");
                onNativeStateChanged(3); // LocationServicesDisabled
            } else {
                Log.w(TAG, "All location providers failed or denied permission.");
                onNativeStateChanged(2); // PermissionDenied
            }
        } else {
            mIsTracking = true;
            if (best == null) {
                onNativeStateChanged(4); // AcquisitionPending
            }
        }
    }

    public synchronized void stopLocationUpdates() {
        if (mLocationManager != null) {
            try {
                mLocationManager.removeUpdates(this);
                Log.i(TAG, "Unregistered Android location updates.");
            } catch (Exception e) {
                Log.e(TAG, "Error removing location updates: " + e.getMessage());
            }
        }
        mIsTracking = false;
    }

    @Override
    public void onLocationChanged(Location location) {
        if (location == null) {
            return;
        }

        final double lat = location.getLatitude();
        final double lon = location.getLongitude();
        final double alt = location.hasAltitude() ? location.getAltitude() : 0.0;
        final float acc = location.hasAccuracy() ? location.getAccuracy() : 25.0f;
        final float speed = location.hasSpeed() ? location.getSpeed() : 0.0f;
        final long time = location.getTime() > 0 ? location.getTime() : System.currentTimeMillis();

        try {
            onNativeLocationChanged(lat, lon, alt, acc, speed, time);
        } catch (UnsatisfiedLinkError ule) {
            Log.e(TAG, "UnsatisfiedLinkError calling onNativeLocationChanged: " + ule.getMessage());
        }
    }

    @Override
    public void onStatusChanged(String provider, int status, Bundle extras) {
        // Maintained for backward compatibility on API 28
    }

    @Override
    public void onProviderEnabled(String provider) {
        Log.i(TAG, "Provider enabled: " + provider);
        if (mIsTracking && mLocationManager != null) {
            try {
                if (LocationManager.GPS_PROVIDER.equals(provider)) {
                    mLocationManager.requestLocationUpdates(
                            LocationManager.GPS_PROVIDER,
                            2000,
                            2.0f,
                            this,
                            Looper.getMainLooper()
                    );
                    Log.i(TAG, "Re-registered GPS_PROVIDER after provider was re-enabled");
                }
            } catch (SecurityException se) {
                Log.w(TAG, "Cannot register re-enabled GPS provider: " + se.getMessage());
            } catch (Exception e) {
                Log.w(TAG, "Error registering re-enabled GPS provider: " + e.getMessage());
            }
        }
    }

    @Override
    public void onProviderDisabled(String provider) {
        Log.w(TAG, "Provider disabled: " + provider);
        if (!isLocationServicesEnabled()) {
            onNativeStateChanged(3); // LocationServicesDisabled
        }
    }
}
