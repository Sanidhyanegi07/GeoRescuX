package com.example.georescux.ui.map

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.view.View
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.example.georescux.GeoRescuXApplication
import com.example.georescux.R
import com.example.georescux.data.incident.IncidentFilter
import com.example.georescux.data.incident.IncidentReport
import com.example.georescux.data.maps.MapRegionCatalog
import com.example.georescux.domain.hazard.HazardCorroboration
import com.example.georescux.domain.incident.IncidentSeverity
import com.example.georescux.domain.incident.IncidentType
import com.example.georescux.domain.routing.MapRegion
import com.example.georescux.domain.sos.SosEmergency
import com.example.georescux.ui.common.ActiveSosBanner
import com.example.georescux.ui.common.BottomNav
import com.example.georescux.ui.common.ConnectivityBadge
import com.example.georescux.ui.common.HelpLauncher
import com.example.georescux.ui.common.OfflineMapSetup
import com.example.georescux.ui.incident.FilterIncidentsBottomSheet
import com.example.georescux.ui.incident.IncidentDetailsBottomSheet
import com.example.georescux.ui.incident.IncidentIconFactory
import com.example.georescux.ui.incident.ReportIncidentBottomSheet
import com.example.georescux.ui.region.RegionSelectionActivity
import com.example.georescux.ui.routing.RouteActivity
import org.osmdroid.util.GeoPoint
import org.osmdroid.views.MapView
import org.osmdroid.views.overlay.Marker
import org.osmdroid.views.overlay.Polyline
import java.util.Locale

/**
 * The MAP tab (spec §14): a modern, offline-first map view.
 * Exclusively focused on the 4 official states: Uttarakhand (Primary),
 * Himachal Pradesh, Uttar Pradesh, and Haryana.
 *
 * Provides real-time disaster and hazard orientation, active SOS incident pins,
 * one-click emergency routing, verified vs unverified report distinction, and
 * instantaneous switching between the 4 official states.
 */
class MapActivity : AppCompatActivity() {

    private val container by lazy { (application as GeoRescuXApplication).appContainer }
    private lateinit var activeRegion: MapRegion
    private lateinit var mapView: MapView
    private lateinit var connectivityBadge: ConnectivityBadge
    private var locationMarker: Marker? = null
    private var boundaryOverlay: Polyline? = null

    private var currentUserLat: Double? = null
    private var currentUserLng: Double? = null
    private val incidentMarkers = mutableListOf<Marker>()
    private val sosMarkers = mutableListOf<Marker>()
    private val incidents = mutableListOf<IncidentReport>()
    private var currentFilter: IncidentFilter = IncidentFilter()

    private val locationPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission(),
    ) { granted ->
        if (granted) startLocationAcquisition()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        activeRegion = MapRegionCatalog.byId(container.activeRegionId)
            ?: MapRegionCatalog.uttarakhand

        OfflineMapSetup.prepareOsmdroid(this)
        setContentView(R.layout.activity_map)

        findViewById<TextView>(R.id.topBarTitle).setText(R.string.map_title)
        mapView = findViewById(R.id.mapView)
        connectivityBadge = ConnectivityBadge(this)

        setupMapForRegion(activeRegion)
        setupStateSwitcher()
        setupFloatingControls()
        setupIncidentReporting()
        setupQuickFilters()
        loadAndObserveIncidents()
        bindConnectionBadge()
        wireHelpAndNavigation()

        if (hasLocationPermission()) {
            startLocationAcquisition()
        } else {
            locationPermissionLauncher.launch(Manifest.permission.ACCESS_FINE_LOCATION)
        }
    }

    private fun setupMapForRegion(region: MapRegion) {
        val regionLabel = findViewById<TextView>(R.id.textMapRegion)
        regionLabel.text = "${getString(R.string.map_region_label)} ${region.displayName}"

        val result = OfflineMapSetup.configure(this, mapView, region)
        findViewById<TextView>(R.id.textMapCacheState).text = result.tier.badgeText

        val offlineMapInstalled = MapRegionCatalog.isOfflineMapInstalled(this, region)
        val notice = findViewById<TextView>(R.id.textNoOfflineMap)
        if (offlineMapInstalled) {
            notice.visibility = View.GONE
        } else {
            notice.visibility = View.VISIBLE
            notice.text = "Offline package for ${region.displayName} not installed. Tap here to manage downloads."
            notice.setOnClickListener {
                startActivity(Intent(this, RegionSelectionActivity::class.java))
            }
        }

        setupRegionBoundaryOverlay()
    }

    private fun setupStateSwitcher() {
        val clickListener = View.OnClickListener {
            showStateSelectionDialog()
        }
        findViewById<View>(R.id.buttonSwitchState)?.setOnClickListener(clickListener)
        findViewById<View>(R.id.textMapRegion)?.setOnClickListener(clickListener)
    }

    private fun showStateSelectionDialog() {
        val states = MapRegionCatalog.availableRegions
        val currentIndex = states.indexOfFirst { it.id == activeRegion.id }.coerceAtLeast(0)
        val names = states.map { state ->
            val isBundled = state.id == "uttarakhand"
            val isInstalled = MapRegionCatalog.isOfflineMapInstalled(this, state)
            when {
                isBundled -> "🏔️ ${state.displayName} (Bundled • Primary)"
                isInstalled -> "✓ ${state.displayName} (Installed)"
                else -> "○ ${state.displayName} (Downloadable)"
            }
        }.toTypedArray()

        AlertDialog.Builder(this)
            .setTitle("Select Official State Map")
            .setSingleChoiceItems(names, currentIndex) { dialog, which ->
                val selected = states[which]
                if (selected.id != activeRegion.id) {
                    activeRegion = selected
                    container.setActiveRegion(selected.id)
                    setupMapForRegion(selected)
                    Toast.makeText(this, "Switched map to ${selected.displayName}", Toast.LENGTH_SHORT).show()
                }
                dialog.dismiss()
            }
            .setNeutralButton("Manage Packages") { _, _ ->
                startActivity(Intent(this, RegionSelectionActivity::class.java))
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun bindConnectionBadge() {
        connectivityBadge.bind(findViewById(R.id.textMapConnectivity))
    }

    private fun setupFloatingControls() {
        findViewById<View>(R.id.buttonMyLocation).setOnClickListener {
            if (!hasLocationPermission()) {
                locationPermissionLauncher.launch(Manifest.permission.ACCESS_FINE_LOCATION)
                return@setOnClickListener
            }
            val position = locationMarker?.position
            if (position != null) {
                mapView.controller.animateTo(position)
                mapView.controller.setZoom(17.0)
            } else {
                Toast.makeText(this, "Acquiring GPS fix...", Toast.LENGTH_SHORT).show()
                startLocationAcquisition()
            }
        }
        findViewById<View>(R.id.buttonZoomIn).setOnClickListener { mapView.controller.zoomIn() }
        findViewById<View>(R.id.buttonZoomOut).setOnClickListener { mapView.controller.zoomOut() }
    }

    private fun setupQuickFilters() {
        findViewById<View>(R.id.chipOpenFilterDialog)?.setOnClickListener {
            val listSnapshot = synchronized(incidents) { incidents.toList() }
            val sheet = FilterIncidentsBottomSheet.newInstance(currentFilter, listSnapshot)
            sheet.onFilterAppliedListener = { newFilter ->
                applyFilter(newFilter)
            }
            sheet.show(supportFragmentManager, "FilterIncidentsBottomSheet")
        }

        val chipGroup = findViewById<com.google.android.material.chip.ChipGroup>(R.id.chipGroupQuickFilters)
        chipGroup?.setOnCheckedStateChangeListener { _, checkedIds ->
            if (checkedIds.isEmpty()) {
                applyFilter(IncidentFilter())
                return@setOnCheckedStateChangeListener
            }
            val filter = when (checkedIds.first()) {
                R.id.chipQuickFlood -> IncidentFilter(type = "FLOOD")
                R.id.chipQuickEarthquake -> IncidentFilter(type = "EARTHQUAKE")
                R.id.chipQuickFire -> IncidentFilter(type = "FIRE")
                R.id.chipQuickLandslide -> IncidentFilter(type = "LANDSLIDE")
                R.id.chipQuickRoadBlock -> IncidentFilter(type = "ROUTE_BLOCK")
                R.id.chipQuickCritical -> IncidentFilter(severity = "CRITICAL")
                R.id.chipQuickHigh -> IncidentFilter(severity = "HIGH")
                else -> IncidentFilter()
            }
            applyFilter(filter, updateChips = false)
        }

        findViewById<View>(R.id.buttonClearActiveFilter)?.setOnClickListener {
            applyFilter(IncidentFilter())
        }
    }

    private fun applyFilter(filter: IncidentFilter, updateChips: Boolean = true) {
        currentFilter = filter

        val filtered = synchronized(incidents) {
            incidents.filter { currentFilter.matches(it) }
        }
        renderIncidentMarkers(filtered)

        val pill = findViewById<View>(R.id.layoutActiveFilterStatus)
        val pillText = findViewById<TextView>(R.id.textActiveFilterDescription)
        val chipFilterBtn = findViewById<com.google.android.material.chip.Chip>(R.id.chipOpenFilterDialog)

        if (currentFilter.isActive) {
            pill?.visibility = View.VISIBLE
            pillText?.text = "Filtered: ${currentFilter.describe()} (${filtered.size})"
            chipFilterBtn?.text = "Filter • Active"
            chipFilterBtn?.setChipBackgroundColorResource(R.color.brand_primary_light)
        } else {
            pill?.visibility = View.GONE
            chipFilterBtn?.text = "Filter"
            chipFilterBtn?.setChipBackgroundColorResource(R.color.white)
        }

        if (updateChips) {
            syncQuickFilterChips(filter)
        }

        if (currentFilter.isActive && filtered.isEmpty()) {
            Toast.makeText(
                this,
                "No incidents match: ${currentFilter.describe()}",
                Toast.LENGTH_SHORT
            ).show()
        }
    }

    private fun syncQuickFilterChips(filter: IncidentFilter) {
        val chipGroup = findViewById<com.google.android.material.chip.ChipGroup>(R.id.chipGroupQuickFilters) ?: return
        if (!filter.isActive) {
            chipGroup.check(R.id.chipQuickAll)
            return
        }

        val chipIdToCheck = when {
            filter.type.equals("FLOOD", ignoreCase = true) && filter.severity == null -> R.id.chipQuickFlood
            filter.type.equals("EARTHQUAKE", ignoreCase = true) && filter.severity == null -> R.id.chipQuickEarthquake
            filter.type.equals("FIRE", ignoreCase = true) && filter.severity == null -> R.id.chipQuickFire
            filter.type.equals("LANDSLIDE", ignoreCase = true) && filter.severity == null -> R.id.chipQuickLandslide
            filter.type.equals("ROUTE_BLOCK", ignoreCase = true) && filter.severity == null -> R.id.chipQuickRoadBlock
            filter.severity.equals("CRITICAL", ignoreCase = true) && filter.type == null -> R.id.chipQuickCritical
            filter.severity.equals("HIGH", ignoreCase = true) && filter.type == null -> R.id.chipQuickHigh
            else -> View.NO_ID
        }

        if (chipIdToCheck != View.NO_ID) {
            chipGroup.check(chipIdToCheck)
        } else {
            chipGroup.clearCheck()
        }
    }

    private fun setupIncidentReporting() {
        findViewById<View>(R.id.buttonReportIncident)?.setOnClickListener {
            val mapCenter = mapView.mapCenter
            val lat = currentUserLat ?: mapCenter.latitude
            val lng = currentUserLng ?: mapCenter.longitude

            val reportSheet = ReportIncidentBottomSheet.newInstance(lat, lng)
            reportSheet.onRefreshGpsRequested = {
                (currentUserLat ?: mapView.mapCenter.latitude) to (currentUserLng ?: mapView.mapCenter.longitude)
            }
            reportSheet.onIncidentReportedListener = { newIncident ->
                // 1. Save locally for instant offline availability
                container.incidentStore.saveIncident(newIncident)

                // 2. Publish to cloud if internet available
                container.firebaseIncidentSync.publishIncident(newIncident)

                // 3. Relay through peer-to-peer BLE mesh
                try {
                    val incType = if (newIncident.type.equals("ROUTE_BLOCK", ignoreCase = true) ||
                        newIncident.type.equals("ROAD_BLOCK", ignoreCase = true)
                    ) {
                        IncidentType.ROUTE_BLOCK
                    } else {
                        IncidentType.HAZARD
                    }

                    val incSev = when (newIncident.severity.uppercase()) {
                        "CRITICAL" -> IncidentSeverity.CRITICAL
                        "HIGH" -> IncidentSeverity.HIGH
                        "MEDIUM" -> IncidentSeverity.MEDIUM
                        else -> IncidentSeverity.LOW
                    }

                    container.reportHazardUseCase.report(
                        description = "${newIncident.getTypeDisplayName()}: ${newIncident.description}",
                        latitude = newIncident.latitude,
                        longitude = newIncident.longitude,
                        type = incType,
                        severity = incSev,
                        eventId = newIncident.id,
                    )
                } catch (e: Exception) {
                    e.printStackTrace()
                }

                // 4. Update memory list and re-render pins
                synchronized(incidents) {
                    val existingIdx = incidents.indexOfFirst { it.id == newIncident.id }
                    if (existingIdx >= 0) {
                        incidents[existingIdx] = newIncident
                    } else {
                        incidents.add(0, newIncident)
                    }
                }
                applyFilter(currentFilter, updateChips = false)

                // 5. Center map onto the newly reported incident
                mapView.controller.animateTo(GeoPoint(newIncident.latitude, newIncident.longitude))
                mapView.controller.setZoom(16.5)
            }
            reportSheet.show(supportFragmentManager, "ReportIncidentBottomSheet")
        }
    }

    private fun loadAndObserveIncidents() {
        // 1. Instant local load (offline ready)
        val localIncidents = container.incidentStore.getAllIncidents()
        synchronized(incidents) {
            incidents.clear()
            incidents.addAll(localIncidents)
        }
        applyFilter(currentFilter, updateChips = false)
        renderSosMarkers()

        // 2. Real-time cloud sync
        container.firebaseIncidentSync.startObserving { updatedList ->
            runOnUiThread {
                if (isFinishing || isDestroyed) return@runOnUiThread
                synchronized(incidents) {
                    incidents.clear()
                    incidents.addAll(updatedList)
                }
                applyFilter(currentFilter, updateChips = false)
                renderSosMarkers()
            }
        }
    }

    private fun renderSosMarkers() {
        sosMarkers.forEach {
            try {
                mapView.overlayManager.remove(it)
            } catch (_: Exception) {}
        }
        sosMarkers.clear()

        val active = runCatching { container.sosRepository.getActiveEmergency() }.getOrNull()
        val historyLocated = runCatching { container.sosRepository.getHistory() }.getOrNull()
            .orEmpty()
            .filter { it.location != null }
            .take(10)

        val emergenciesToRender = buildList {
            if (active?.location != null) add(active)
            addAll(historyLocated)
        }

        val alertIcon = ContextCompat.getDrawable(this, android.R.drawable.ic_dialog_alert)

        emergenciesToRender.forEach { emergency ->
            val loc = emergency.location ?: return@forEach
            val isThisActive = emergency.isActive
            val marker = Marker(mapView).apply {
                position = GeoPoint(loc.latitude, loc.longitude)
                title = if (isThisActive) "🚨 ACTIVE EMERGENCY SOS" else "SOS Alert Record"
                snippet = "ID: ${emergency.id.take(12)}…\nTap for action & routing"
                if (alertIcon != null) icon = alertIcon
                setAnchor(Marker.ANCHOR_CENTER, Marker.ANCHOR_CENTER)

                setOnMarkerClickListener { m, _ ->
                    m.showInfoWindow()
                    showEmergencyDetailsDialog(emergency)
                    true
                }
            }
            mapView.overlays.add(marker)
            sosMarkers.add(marker)
        }
        mapView.invalidate()
    }

    private fun showEmergencyDetailsDialog(emergency: SosEmergency) {
        val loc = emergency.location ?: return
        val uLat = currentUserLat
        val uLng = currentUserLng
        val distanceStr = if (uLat != null && uLng != null && uLat != 0.0) {
            val dist = HazardCorroboration.haversineMeters(uLat, uLng, loc.latitude, loc.longitude)
            if (dist < 1000) "${dist.toInt()}m away" else String.format(Locale.US, "%.1f km away", dist / 1000.0)
        } else {
            "Distance calculating…"
        }

        val noteText = emergency.note?.takeIf { it.isNotBlank() } ?: "Emergency distress signal"
        val statusText = if (emergency.isActive) "CRITICAL • ACTIVE" else "RESOLVED / ARCHIVED"

        AlertDialog.Builder(this)
            .setTitle(if (emergency.isActive) "🚨 Active Emergency Alert" else "Incident Archive")
            .setMessage("Status: $statusText\nDistance: $distanceStr\nCoordinates: ${loc.latitude}, ${loc.longitude}\nNote: $noteText")
            .setPositiveButton("Route to Emergency") { _, _ ->
                val intent = Intent(this, RouteActivity::class.java).apply {
                    putExtra(RouteActivity.EXTRA_DEST_LAT, loc.latitude)
                    putExtra(RouteActivity.EXTRA_DEST_LNG, loc.longitude)
                    putExtra(RouteActivity.EXTRA_DEST_TITLE, "Emergency ${emergency.id.take(8)}")
                }
                startActivity(intent)
            }
            .setNegativeButton("Close", null)
            .show()
    }

    private fun renderIncidentMarkers(list: List<IncidentReport>) {
        incidentMarkers.forEach {
            try {
                mapView.overlayManager.remove(it)
            } catch (_: Exception) {}
        }
        incidentMarkers.clear()

        list.forEach { incident ->
            if (incident.latitude == 0.0 && incident.longitude == 0.0) return@forEach

            val marker = Marker(mapView).apply {
                position = GeoPoint(incident.latitude, incident.longitude)
                title = "${incident.getTypeEmoji()} ${incident.getTypeDisplayName()}"
                val snippetText = if (incident.description.length > 55) {
                    "${incident.description.take(52)}…"
                } else {
                    incident.description
                }
                snippet = "$snippetText\nTap for details"
                icon = IncidentIconFactory.getMarkerIcon(this@MapActivity, incident)
                setAnchor(Marker.ANCHOR_CENTER, Marker.ANCHOR_BOTTOM)

                setOnMarkerClickListener { m, _ ->
                    m.showInfoWindow()
                    val detailsSheet = IncidentDetailsBottomSheet.newInstance(
                        incident = incident,
                        userLat = currentUserLat,
                        userLng = currentUserLng,
                    )
                    detailsSheet.show(supportFragmentManager, "IncidentDetailsBottomSheet")
                    true
                }
            }

            mapView.overlays.add(marker)
            incidentMarkers.add(marker)
        }
        mapView.invalidate()
    }

    /**
     * Draws the active region's boundary so the user can always see the edge
     * of the installed offline package (spec §14 "regional boundaries").
     */
    private fun setupRegionBoundaryOverlay() {
        boundaryOverlay?.let {
            try { mapView.overlays.remove(it) } catch (_: Exception) {}
        }
        val boundary = Polyline().apply {
            outlinePaint.color = ContextCompat.getColor(this@MapActivity, R.color.brand_primary)
            outlinePaint.strokeWidth = 4f
        }
        val (minLat, maxLat) = activeRegion.minLatitude to activeRegion.maxLatitude
        val (minLng, maxLng) = activeRegion.minLongitude to activeRegion.maxLongitude
        boundary.setPoints(
            listOf(
                GeoPoint(minLat, minLng),
                GeoPoint(maxLat, minLng),
                GeoPoint(maxLat, maxLng),
                GeoPoint(minLat, maxLng),
                GeoPoint(minLat, minLng),
            ),
        )
        mapView.overlays.add(boundary)
        boundaryOverlay = boundary
        mapView.invalidate()
    }

    private fun startLocationAcquisition() {
        container.locationRepository.startAcquisition { fix ->
            currentUserLat = fix.latitude
            currentUserLng = fix.longitude
            runOnUiThread {
                if (isFinishing || isDestroyed) return@runOnUiThread
                showLocationMarker(GeoPoint(fix.latitude, fix.longitude))
            }
        }
    }

    private fun showLocationMarker(point: GeoPoint) {
        if (locationMarker == null) {
            locationMarker = Marker(mapView).apply {
                setAnchor(Marker.ANCHOR_CENTER, Marker.ANCHOR_BOTTOM)
                title = getString(R.string.map_my_location)
            }
            mapView.overlays.add(locationMarker)
        }
        locationMarker?.position = point
        mapView.invalidate()
    }

    private fun wireHelpAndNavigation() {
        HelpLauncher.bind(this)
        findViewById<View>(R.id.buttonOpenSafeRoute).setOnClickListener {
            startActivity(Intent(this, RouteActivity::class.java))
        }
        BottomNav.bind(this, R.id.nav_map)
    }

    private fun hasLocationPermission(): Boolean =
        ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) ==
            PackageManager.PERMISSION_GRANTED ||
            ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) ==
            PackageManager.PERMISSION_GRANTED

    override fun onResume() {
        super.onResume()
        mapView.onResume()
        ActiveSosBanner.refresh(this)
        renderSosMarkers()
        val local = container.incidentStore.getAllIncidents()
        if (local.isNotEmpty()) {
            synchronized(incidents) {
                incidents.clear()
                incidents.addAll(local)
            }
            applyFilter(currentFilter, updateChips = false)
        }
    }

    override fun onPause() {
        mapView.onPause()
        super.onPause()
    }

    override fun onDestroy() {
        container.firebaseIncidentSync.stopObserving()
        container.locationRepository.stopAcquisition()
        connectivityBadge.unbind()
        incidentMarkers.forEach { try { mapView.overlayManager.remove(it) } catch (_: Exception) {} }
        incidentMarkers.clear()
        sosMarkers.forEach { try { mapView.overlayManager.remove(it) } catch (_: Exception) {} }
        sosMarkers.clear()
        boundaryOverlay?.let { try { mapView.overlays.remove(it) } catch (_: Exception) {} }
        boundaryOverlay = null
        mapView.onDetach()
        super.onDestroy()
    }
}
