package com.example.georescux.ui.incident

import android.content.Intent
import android.graphics.Bitmap
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.example.georescux.R
import com.example.georescux.data.incident.ImageStorageHelper
import com.example.georescux.data.incident.IncidentReport
import com.example.georescux.domain.hazard.HazardCorroboration
import com.example.georescux.ui.routing.RouteActivity
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.util.Locale

class IncidentDetailsBottomSheet : BottomSheetDialogFragment() {

    private var incident: IncidentReport? = null
    private var userLat: Double? = null
    private var userLng: Double? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val rawJson = arguments?.getString(ARG_INCIDENT_JSON)
        if (rawJson != null) {
            incident = IncidentReport.fromJson(JSONObject(rawJson))
        }
        if (arguments?.containsKey(ARG_USER_LAT) == true) {
            userLat = arguments?.getDouble(ARG_USER_LAT)
            userLng = arguments?.getDouble(ARG_USER_LNG)
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? {
        return inflater.inflate(R.layout.bottom_sheet_incident_details, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val inc = incident ?: return

        view.findViewById<ImageButton>(R.id.buttonCloseDetailsSheet).setOnClickListener {
            dismiss()
        }

        // Title and emoji
        view.findViewById<TextView>(R.id.textDetailsTypeEmoji).text = inc.getTypeEmoji()
        view.findViewById<TextView>(R.id.textDetailsTitle).text = "${inc.getTypeDisplayName()} Report"

        // Severity pill styling
        val severityPill = view.findViewById<TextView>(R.id.textDetailsSeverityPill)
        severityPill.text = "${inc.severity.uppercase()} SEVERITY"
        when (inc.severity.uppercase()) {
            "CRITICAL" -> {
                severityPill.setBackgroundResource(R.drawable.bg_pill_high_priority)
                severityPill.setTextColor(ContextCompat.getColor(requireContext(), R.color.sos_red))
            }
            "HIGH" -> {
                severityPill.setBackgroundResource(R.drawable.bg_pill_pending)
                severityPill.setTextColor(ContextCompat.getColor(requireContext(), R.color.hazard_orange))
            }
            "MEDIUM" -> {
                severityPill.setBackgroundResource(R.drawable.bg_pill_pending)
                severityPill.setTextColor(ContextCompat.getColor(requireContext(), R.color.warn_amber))
            }
            else -> {
                severityPill.setBackgroundResource(R.drawable.bg_pill_low_risk)
                severityPill.setTextColor(ContextCompat.getColor(requireContext(), R.color.safe_green))
            }
        }

        // Status pill
        val statusPill = view.findViewById<TextView>(R.id.textDetailsStatusPill)
        statusPill.text = inc.status.uppercase()

        // Description
        view.findViewById<TextView>(R.id.textDetailsDescription).text = inc.description

        // Coordinates & Distance
        val coordsText = view.findViewById<TextView>(R.id.textDetailsCoordinates)
        coordsText.text = String.format(
            Locale.US,
            "%.5f° %s, %.5f° %s",
            Math.abs(inc.latitude),
            if (inc.latitude >= 0) "N" else "S",
            Math.abs(inc.longitude),
            if (inc.longitude >= 0) "E" else "W"
        )

        val distanceText = view.findViewById<TextView>(R.id.textDetailsDistance)
        val uLat = userLat
        val uLng = userLng
        if (uLat != null && uLng != null && uLat != 0.0) {
            val distMeters = HazardCorroboration.haversineMeters(uLat, uLng, inc.latitude, inc.longitude)
            distanceText.text = if (distMeters < 1000) {
                "${distMeters.toInt()}m from your location"
            } else {
                String.format(Locale.US, "%.1f km from your location", distMeters / 1000.0)
            }
            distanceText.visibility = View.VISIBLE
        } else {
            distanceText.visibility = View.GONE
        }

        // Time
        view.findViewById<TextView>(R.id.textDetailsTime).text = inc.getFormattedTime()
        view.findViewById<TextView>(R.id.textDetailsRelativeTime).text = inc.getRelativeTime()

        // Reported By
        view.findViewById<TextView>(R.id.textDetailsReportedBy).text = "Source: ${inc.reportedBy}"

        // Load Photo if available
        val photoCard = view.findViewById<View>(R.id.cardDetailsPhoto)
        val photoImage = view.findViewById<ImageView>(R.id.imageDetailsPhoto)
        loadIncidentPhoto(inc, photoCard, photoImage)

        // Safe Route button
        view.findViewById<Button>(R.id.buttonSafeRouteAround).setOnClickListener {
            dismiss()
            startActivity(Intent(requireContext(), RouteActivity::class.java))
        }
    }

    private fun loadIncidentPhoto(
        inc: IncidentReport,
        photoCard: View,
        photoImage: ImageView,
    ) {
        val imagePath = inc.imageUri
        val base64 = inc.imageBase64

        if (imagePath.isNullOrEmpty() && base64.isNullOrEmpty()) {
            photoCard.visibility = View.GONE
            return
        }

        lifecycleScope.launch {
            val bitmap: Bitmap? = withContext(Dispatchers.IO) {
                if (!imagePath.isNullOrEmpty()) {
                    ImageStorageHelper.decodeSampledBitmapFromFile(imagePath, 800, 600)
                } else if (!base64.isNullOrEmpty()) {
                    ImageStorageHelper.decodeBase64(base64)
                } else {
                    null
                }
            }

            if (bitmap != null) {
                photoImage.setImageBitmap(bitmap)
                photoCard.visibility = View.VISIBLE
            } else {
                photoCard.visibility = View.GONE
            }
        }
    }

    companion object {
        private const val ARG_INCIDENT_JSON = "arg_incident_json"
        private const val ARG_USER_LAT = "arg_user_lat"
        private const val ARG_USER_LNG = "arg_user_lng"

        fun newInstance(incident: IncidentReport, userLat: Double? = null, userLng: Double? = null): IncidentDetailsBottomSheet {
            return IncidentDetailsBottomSheet().apply {
                arguments = Bundle().apply {
                    putString(ARG_INCIDENT_JSON, incident.toJson().toString())
                    if (userLat != null && userLng != null) {
                        putDouble(ARG_USER_LAT, userLat)
                        putDouble(ARG_USER_LNG, userLng)
                    }
                }
            }
        }
    }
}
