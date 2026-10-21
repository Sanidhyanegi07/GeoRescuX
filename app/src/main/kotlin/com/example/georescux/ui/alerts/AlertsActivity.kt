package com.example.georescux.ui.alerts

import android.os.Bundle
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.ViewModelProvider
import com.example.georescux.GeoRescuXApplication
import com.example.georescux.R
import com.example.georescux.data.alerts.AdminAlertObserver
import com.example.georescux.domain.admin.DistrictCatalog
import com.example.georescux.domain.alerts.AlertEntry
import com.example.georescux.domain.alerts.AlertSource
import com.example.georescux.ui.common.ActiveSosBanner
import com.example.georescux.ui.common.BottomNav
import com.example.georescux.ui.common.HelpLauncher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * The unified emergency information centre (spec §20): the user's own SOS
 * events, SOS alerts received over the local BLE mesh, and official
 * administrative alerts for the active region — all source-labelled, newest
 * first, and readable offline once stored locally.
 *
 * This screen never creates, modifies, or deletes records.
 */
class AlertsActivity : AppCompatActivity() {

    private val uiScope = CoroutineScope(Dispatchers.Main.immediate + SupervisorJob())
    private val container by lazy { (application as GeoRescuXApplication).appContainer }
    private lateinit var adminAlertObserver: AdminAlertObserver

    private val dateFormat = SimpleDateFormat("dd MMM yyyy, HH:mm", Locale.getDefault())

    override fun onResume() {
        super.onResume()
        ActiveSosBanner.refresh(this)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_alerts)

        HelpLauncher.bind(this)
        findViewById<TextView>(R.id.topBarTitle).setText(R.string.alerts_title)

        findViewById<View>(R.id.buttonAlertsBack)?.setOnClickListener { finish() }
        findViewById<View>(R.id.buttonAlertsRefresh)?.setOnClickListener {
            adminAlertObserver.stop()
            adminAlertObserver.start()
            android.widget.Toast.makeText(this, "Alerts refreshed", android.widget.Toast.LENGTH_SHORT).show()
        }

        val chipAll = findViewById<TextView>(R.id.chipFilterAll)
        val chipDisaster = findViewById<TextView>(R.id.chipFilterDisaster)
        val chipWeather = findViewById<TextView>(R.id.chipFilterWeather)
        val chipRoad = findViewById<TextView>(R.id.chipFilterRoad)

        val chips = listOf(chipAll, chipDisaster, chipWeather, chipRoad)

        fun selectChip(selected: TextView?, category: String) {
            activeCategory = category
            chips.forEach { chip ->
                if (chip == selected) {
                    chip?.setBackgroundResource(R.drawable.bg_tab_active)
                    chip?.setTextColor(androidx.core.content.ContextCompat.getColor(this, R.color.white))
                } else {
                    chip?.setBackgroundResource(R.drawable.bg_tab_container)
                    chip?.setTextColor(androidx.core.content.ContextCompat.getColor(this, R.color.text_muted))
                }
            }
            renderAlertsList()
        }

        chipAll?.setOnClickListener { selectChip(chipAll, "ALL") }
        chipDisaster?.setOnClickListener { selectChip(chipDisaster, "DISASTER") }
        chipWeather?.setOnClickListener { selectChip(chipWeather, "WEATHER") }
        chipRoad?.setOnClickListener { selectChip(chipRoad, "ROAD") }

        adminAlertObserver = AdminAlertObserver(this, { container.activeRegionId }) {
            val located = container.sosRepository.getActiveEmergency()?.location
                ?: container.sosRepository.getHistory()
                    .firstOrNull { it.location != null }?.location
            located?.let { DistrictCatalog.districtForLocation(it.latitude, it.longitude)?.displayName }
        }
        val viewModel = ViewModelProvider(
            this,
            AlertsViewModel.Factory(container.sosRepository, adminAlertObserver.alerts.value),
        ).get(AlertsViewModel::class.java)

        adminAlertObserver.start()

        uiScope.launch {
            adminAlertObserver.alerts.collect { alerts -> viewModel.setAdminAlerts(alerts) }
        }

        uiScope.launch {
            viewModel.allAlerts.collect { alerts ->
                allAlertsCache = alerts
                renderAlertsList()
            }
        }

        BottomNav.bind(this, R.id.nav_alerts)
    }

    private var activeCategory: String = "ALL"
    private var allAlertsCache: List<AlertEntry> = emptyList()

    private fun renderAlertsList() {
        val alertsListContainer = findViewById<LinearLayout>(R.id.alertsListContainer) ?: return
        val emptyText = findViewById<View>(R.id.textViewEmptyAlerts)

        val filtered = allAlertsCache.filter { alert ->
            val text = (alert.title + " " + alert.body + " " + (alert.note ?: "")).lowercase()
            when (activeCategory) {
                "DISASTER" -> alert.source == AlertSource.OWN_SOS || alert.source == AlertSource.BLE_SOS ||
                        text.contains("disaster") || text.contains("landslide") || text.contains("flood") || text.contains("quake")
                "WEATHER" -> text.contains("weather") || text.contains("rain") || text.contains("storm") || text.contains("cyclone")
                "ROAD" -> text.contains("road") || text.contains("block") || text.contains("route") || text.contains("traffic")
                else -> true
            }
        }

        emptyText?.visibility = if (filtered.isEmpty()) View.VISIBLE else View.GONE
        alertsListContainer.removeAllViews()
        filtered.forEach { alert ->
            alertsListContainer.addView(alertRow(alert))
        }
    }

    private fun alertRow(alert: AlertEntry): View {
        val row = layoutInflater.inflate(R.layout.item_alert, findViewById<LinearLayout>(R.id.alertsListContainer), false)

        val sourceBadge = row.findViewById<TextView>(R.id.textAlertSource)
        val iconView = row.findViewById<android.widget.ImageView>(R.id.imageAlertIcon)

        val titleText = alert.title.ifBlank {
            when (alert.source) {
                AlertSource.OWN_SOS -> "Active Emergency SOS"
                AlertSource.BLE_SOS -> "Nearby Mesh Emergency SOS"
                AlertSource.ADMIN -> "Official Disaster Advisory"
            }
        }

        val textLower = (alert.title + " " + alert.body).lowercase()
        val (iconRes, iconTint, pillBg) = when {
            textLower.contains("weather") || textLower.contains("rain") ->
                Triple(R.drawable.ic_weather_alert, R.color.amber_alert, R.drawable.bg_pill_pending)
            textLower.contains("road") || textLower.contains("block") ->
                Triple(R.drawable.ic_safe_route, R.color.brand_primary, R.drawable.bg_pill_low_risk)
            else ->
                Triple(R.drawable.ic_alerts_warning, R.color.sos_red, R.drawable.bg_pill_high_priority)
        }

        iconView?.setImageResource(iconRes)
        iconView?.imageTintList = android.content.res.ColorStateList.valueOf(
            androidx.core.content.ContextCompat.getColor(this, iconTint)
        )
        iconView?.setBackgroundResource(pillBg)

        sourceBadge?.text = alert.source.label
        sourceBadge?.setBackgroundResource(pillBg)
        sourceBadge?.setTextColor(androidx.core.content.ContextCompat.getColor(this, iconTint))

        row.findViewById<TextView>(R.id.textAlertDate)?.text =
            dateFormat.format(Date(alert.timestampMs))

        val titleView = row.findViewById<TextView>(R.id.textAlertTitle)
        titleView?.text = titleText

        val noteView = row.findViewById<TextView>(R.id.textAlertNote)
        val noteText = alert.note?.takeIf { it.isNotBlank() }
            ?: alert.body.takeIf { it.isNotBlank() }
        if (noteText != null) {
            noteView?.text = noteText
            noteView?.visibility = View.VISIBLE
        } else {
            noteView?.visibility = View.GONE
        }

        row.findViewById<TextView>(R.id.textAlertDuration)?.text =
            if (alert.source == AlertSource.ADMIN) {
                alert.locationLabel?.let { "Scope: $it" }.orEmpty()
            } else {
                "Duration: ${alert.durationLabel}"
            }

        row.findViewById<TextView>(R.id.textAlertLocation)?.text = alert.locationLabel ?: "Location: Active Region"

        row.setOnClickListener {
            showAlertDetailsDialog(alert, titleText, noteText)
        }

        return row
    }

    private fun showAlertDetailsDialog(alert: AlertEntry, title: String, note: String?) {
        val message = StringBuilder()
            .append("Source: ").append(alert.source.label).append("\n")
            .append("Time: ").append(dateFormat.format(Date(alert.timestampMs))).append("\n")
            .append("Location: ").append(alert.locationLabel ?: "Active Region").append("\n")
        if (alert.durationLabel != null) {
            message.append("Status/Duration: ").append(alert.durationLabel).append("\n")
        }
        if (!note.isNullOrBlank()) {
            message.append("\nDetails:\n").append(note)
        }

        com.google.android.material.dialog.MaterialAlertDialogBuilder(this)
            .setTitle(title)
            .setMessage(message.toString())
            .setPositiveButton("Close", null)
            .setNeutralButton("View on Map") { _, _ ->
                val intent = android.content.Intent(this, com.example.georescux.ui.map.MapActivity::class.java)
                startActivity(intent)
            }
            .show()
    }

    private fun sourceColor(source: AlertSource): Int = androidx.core.content.ContextCompat.getColor(
        this,
        when (source) {
            AlertSource.OWN_SOS -> R.color.sos_red
            AlertSource.BLE_SOS -> R.color.hazard_orange
            AlertSource.ADMIN -> R.color.brand_primary
        },
    )

    override fun onDestroy() {
        adminAlertObserver.stop()
        uiScope.cancel()
        super.onDestroy()
    }
}

