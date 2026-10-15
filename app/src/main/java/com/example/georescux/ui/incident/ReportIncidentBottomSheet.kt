package com.example.georescux.ui.incident

import android.content.Context
import android.graphics.Bitmap
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.FileProvider
import androidx.lifecycle.lifecycleScope
import com.example.georescux.R
import com.example.georescux.data.incident.ImageStorageHelper
import com.example.georescux.data.incident.IncidentReport
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import com.google.android.material.chip.ChipGroup
import com.google.android.material.textfield.TextInputEditText
import com.google.android.material.textfield.TextInputLayout
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.util.Locale
import java.util.UUID

class ReportIncidentBottomSheet : BottomSheetDialogFragment() {

    private var initialLat: Double = 0.0
    private var initialLng: Double = 0.0
    private var currentLat: Double = 0.0
    private var currentLng: Double = 0.0

    private var selectedImageUri: Uri? = null
    private var tempCameraUri: Uri? = null
    private var tempCameraFile: File? = null

    private lateinit var textCoordinates: TextView
    private lateinit var cardImagePreview: View
    private lateinit var imagePreview: ImageView
    private lateinit var editDescription: TextInputEditText
    private lateinit var layoutDescription: TextInputLayout
    private lateinit var chipGroupType: ChipGroup
    private lateinit var chipGroupSeverity: ChipGroup
    private lateinit var buttonSubmit: Button

    var onIncidentReportedListener: ((IncidentReport) -> Unit)? = null
    var onRefreshGpsRequested: (() -> Pair<Double, Double>)? = null

    // Photo pickers
    private val pickFromGalleryLauncher = registerForActivityResult(
        ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        if (uri != null) {
            selectedImageUri = uri
            displayImagePreview(uri)
        }
    }

    private val takePhotoLauncher = registerForActivityResult(
        ActivityResultContracts.TakePicture()
    ) { success: Boolean ->
        if (success && tempCameraUri != null) {
            selectedImageUri = tempCameraUri
            displayImagePreview(tempCameraUri!!)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        initialLat = arguments?.getDouble(ARG_LAT, 0.0) ?: 0.0
        initialLng = arguments?.getDouble(ARG_LNG, 0.0) ?: 0.0
        currentLat = initialLat
        currentLng = initialLng
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? {
        return inflater.inflate(R.layout.bottom_sheet_report_incident, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        view.findViewById<ImageButton>(R.id.buttonCloseSheet).setOnClickListener {
            dismiss()
        }

        textCoordinates = view.findViewById(R.id.textIncidentCoordinates)
        cardImagePreview = view.findViewById(R.id.cardImagePreview)
        imagePreview = view.findViewById(R.id.imageIncidentPreview)
        editDescription = view.findViewById(R.id.editTextDescription)
        layoutDescription = view.findViewById(R.id.inputLayoutDescription)
        chipGroupType = view.findViewById(R.id.chipGroupIncidentType)
        chipGroupSeverity = view.findViewById(R.id.chipGroupSeverity)
        buttonSubmit = view.findViewById(R.id.buttonSubmitIncident)

        // Select default chips
        view.findViewById<com.google.android.material.chip.Chip>(R.id.chipTypeFlood)?.isChecked = true
        view.findViewById<com.google.android.material.chip.Chip>(R.id.chipSevHigh)?.isChecked = true

        updateCoordinatesDisplay()

        view.findViewById<Button>(R.id.buttonRefreshGps).setOnClickListener {
            val fresh = onRefreshGpsRequested?.invoke()
            if (fresh != null && fresh.first != 0.0) {
                currentLat = fresh.first
                currentLng = fresh.second
                updateCoordinatesDisplay()
            } else {
                Toast.makeText(requireContext(), "Acquiring GPS fix...", Toast.LENGTH_SHORT).show()
            }
        }

        // Camera Button
        view.findViewById<Button>(R.id.buttonTakePhoto).setOnClickListener {
            launchCamera()
        }

        // Gallery Button
        view.findViewById<Button>(R.id.buttonPickGallery).setOnClickListener {
            pickFromGalleryLauncher.launch("image/*")
        }

        // Remove Photo
        view.findViewById<ImageButton>(R.id.buttonRemoveImage).setOnClickListener {
            selectedImageUri = null
            cardImagePreview.visibility = View.GONE
        }

        // Submit Incident
        buttonSubmit.setOnClickListener {
            submitReport()
        }
    }

    fun updateCoordinates(lat: Double, lng: Double) {
        currentLat = lat
        currentLng = lng
        if (::textCoordinates.isInitialized) {
            updateCoordinatesDisplay()
        }
    }

    private fun updateCoordinatesDisplay() {
        if (currentLat != 0.0 && currentLng != 0.0) {
            textCoordinates.text = String.format(
                Locale.US,
                "%.5f° %s, %.5f° %s",
                Math.abs(currentLat),
                if (currentLat >= 0) "N" else "S",
                Math.abs(currentLng),
                if (currentLng >= 0) "E" else "W"
            )
        } else {
            textCoordinates.text = "GPS Fix Pending (Tap Refresh)"
        }
    }

    private fun launchCamera() {
        try {
            val context = requireContext()
            val file = ImageStorageHelper.createTempCameraFile(context)
            tempCameraFile = file
            val authority = "${context.packageName}.fileprovider"
            val uri = FileProvider.getUriForFile(context, authority, file)
            tempCameraUri = uri
            takePhotoLauncher.launch(uri)
        } catch (e: Exception) {
            Toast.makeText(requireContext(), "Camera error: ${e.message}", Toast.LENGTH_SHORT).show()
        }
    }

    private fun displayImagePreview(uri: Uri) {
        lifecycleScope.launch {
            val bitmap = withContext(Dispatchers.IO) {
                ImageStorageHelper.decodeSampledBitmapFromUri(requireContext(), uri, 600, 400)
            }
            if (bitmap != null) {
                imagePreview.setImageBitmap(bitmap)
                cardImagePreview.visibility = View.VISIBLE
            } else {
                cardImagePreview.visibility = View.GONE
            }
        }
    }

    private fun submitReport() {
        val description = editDescription.text?.toString()?.trim().orEmpty()
        if (description.isEmpty()) {
            layoutDescription.error = "Please provide an incident description."
            editDescription.requestFocus()
            return
        }
        layoutDescription.error = null

        val selectedTypeId = chipGroupType.checkedChipId
        val type = when (selectedTypeId) {
            R.id.chipTypeFlood -> "FLOOD"
            R.id.chipTypeEarthquake -> "EARTHQUAKE"
            R.id.chipTypeLandslide -> "LANDSLIDE"
            R.id.chipTypeFire -> "FIRE"
            R.id.chipTypeRoadBlock -> "ROUTE_BLOCK"
            R.id.chipTypeCollapse -> "BUILDING_COLLAPSE"
            R.id.chipTypeMedical -> "MEDICAL"
            else -> "OTHER"
        }

        val selectedSevId = chipGroupSeverity.checkedChipId
        val severity = when (selectedSevId) {
            R.id.chipSevLow -> "LOW"
            R.id.chipSevMedium -> "MEDIUM"
            R.id.chipSevCritical -> "CRITICAL"
            else -> "HIGH"
        }

        buttonSubmit.isEnabled = false
        buttonSubmit.text = "Saving Report…"

        val incidentId = UUID.randomUUID().toString()
        val context = requireContext().applicationContext

        lifecycleScope.launch {
            var savedImagePath: String? = null
            var base64Thumb: String? = null

            val imageUriToProcess = selectedImageUri
            if (imageUriToProcess != null) {
                val result = withContext(Dispatchers.IO) {
                    ImageStorageHelper.saveIncidentImage(context, imageUriToProcess, incidentId)
                }
                savedImagePath = result.first
                base64Thumb = result.second
            }

            val title = when (type) {
                "FLOOD" -> "Flood Emergency"
                "EARTHQUAKE" -> "Earthquake Damage"
                "LANDSLIDE" -> "Landslide Block"
                "FIRE" -> "Fire Danger"
                "ROUTE_BLOCK" -> "Roadway Blocked"
                "BUILDING_COLLAPSE" -> "Structure Collapse"
                "MEDICAL" -> "Medical Crisis"
                else -> "Hazard Observation"
            }

            val incident = IncidentReport(
                id = incidentId,
                type = type,
                title = title,
                description = description,
                latitude = currentLat,
                longitude = currentLng,
                severity = severity,
                status = "REPORTED",
                timestampMs = System.currentTimeMillis(),
                imageUri = savedImagePath,
                imageBase64 = base64Thumb,
                reportedBy = "User (Direct)",
            )

            onIncidentReportedListener?.invoke(incident)

            Toast.makeText(context, "Incident reported and placed on map!", Toast.LENGTH_SHORT).show()
            dismiss()
        }
    }

    companion object {
        private const val ARG_LAT = "arg_lat"
        private const val ARG_LNG = "arg_lng"

        fun newInstance(lat: Double, lng: Double): ReportIncidentBottomSheet {
            return ReportIncidentBottomSheet().apply {
                arguments = Bundle().apply {
                    putDouble(ARG_LAT, lat)
                    putDouble(ARG_LNG, lng)
                }
            }
        }
    }
}
