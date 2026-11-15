package com.example.georescux.ui.region

import android.content.Intent
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.LayoutInflater
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import com.example.georescux.GeoRescuXApplication
import com.example.georescux.R
import com.example.georescux.data.maps.MapPackageManager
import com.example.georescux.data.maps.MapRegionCatalog
import com.example.georescux.domain.routing.MapRegion
import com.example.georescux.ui.common.BottomNav
import com.example.georescux.ui.common.HelpLauncher
import com.example.georescux.ui.home.HomeActivity
import kotlinx.coroutines.launch

/**
 * Four-State Map Package Selection & Download Management.
 * Restricts selection exclusively to the four officially supported states:
 * 1. Uttarakhand (Primary, bundled)
 * 2. Himachal Pradesh (Secondary, downloadable)
 * 3. Uttar Pradesh (Secondary, downloadable)
 * 4. Haryana (Secondary, downloadable)
 */
class RegionSelectionActivity : AppCompatActivity() {

    private val container by lazy { (application as GeoRescuXApplication).appContainer }
    private var selectedRegionId: String = ""
    private var searchQuery: String = ""

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        selectedRegionId = container.activeRegionId

        setContentView(R.layout.activity_region_selection)

        findViewById<View>(R.id.topBarTitle)?.let { (it as? TextView)?.setText(R.string.region_select_title) }
        findViewById<View>(R.id.buttonRegionBack)?.setOnClickListener { finish() }
        HelpLauncher.bind(this)
        BottomNav.bind(this, R.id.nav_map)

        val tabAvailable = findViewById<TextView>(R.id.tabAvailableMaps)
        val tabDownloaded = findViewById<TextView>(R.id.tabDownloadedMaps)

        tabAvailable?.setOnClickListener {
            showOnlyDownloaded = false
            tabAvailable.setBackgroundResource(R.drawable.bg_tab_active)
            tabAvailable.setTextColor(getTextColor(R.color.white))
            tabDownloaded?.background = null
            tabDownloaded?.setTextColor(getTextColor(R.color.text_muted))
            renderRegions()
        }

        tabDownloaded?.setOnClickListener {
            showOnlyDownloaded = true
            tabDownloaded.setBackgroundResource(R.drawable.bg_tab_active)
            tabDownloaded.setTextColor(getTextColor(R.color.white))
            tabAvailable.background = null
            tabAvailable.setTextColor(getTextColor(R.color.text_muted))
            renderRegions()
        }

        findViewById<android.widget.EditText>(R.id.editRegionSearch)?.addTextChangedListener(
            object : TextWatcher {
                override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
                override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
                override fun afterTextChanged(s: Editable?) {
                    searchQuery = s?.toString()?.trim()?.lowercase().orEmpty()
                    renderRegions()
                }
            },
        )

        updateStorageSummary()

        val continueBtn = findViewById<View>(R.id.buttonRegionContinue) ?: findViewById<View>(R.id.buttonContinue)
        continueBtn?.setOnClickListener { continueWithSelection() }

        renderRegions()
    }

    private var showOnlyDownloaded: Boolean = false

    private fun updateStorageSummary() {
        val totalBytes = MapPackageManager.getTotalStorageUsageBytes(this)
        val mb = totalBytes / (1024.0 * 1024.0)
        findViewById<TextView>(R.id.textStorageDetail)?.text =
            String.format(java.util.Locale.US, "4 Official States · Local Storage: %.1f MB", mb)
    }

    private fun renderRegions() {
        val listContainer = findViewById<LinearLayout>(R.id.regionListContainer) ?: return
        listContainer.removeAllViews()

        MapRegionCatalog.availableRegions
            .filter { it.displayName.lowercase().contains(searchQuery) }
            .filter { region ->
                if (!showOnlyDownloaded) true
                else MapRegionCatalog.isGraphInstalled(this, region) || MapPackageManager.isPackageInstalled(this, region.id)
            }
            .forEach { region -> listContainer.addView(buildRegionRow(region)) }
    }

    private fun buildRegionRow(region: MapRegion): View {
        val row = LayoutInflater.from(this)
            .inflate(R.layout.item_region, null, false)

        val graphInstalled = MapRegionCatalog.isGraphInstalled(this, region)
        val mapInstalled = MapRegionCatalog.isOfflineMapInstalled(this, region) || MapPackageManager.isPackageInstalled(this, region.id)
        val isReady = graphInstalled || mapInstalled

        val name = row.findViewById<TextView>(R.id.regionName)
        val status = row.findViewById<TextView>(R.id.regionStatus)
        val dot = row.findViewById<View>(R.id.regionSelectionDot)
        val btnDownload = row.findViewById<android.widget.Button>(R.id.buttonRegionDownload)
        val card = row.findViewById<com.google.android.material.card.MaterialCardView>(R.id.regionCard)

        name?.text = region.displayName
        status?.text = when {
            graphInstalled && mapInstalled -> getString(R.string.region_status_installed)
            mapInstalled -> "Offline Map Ready (Routing online)"
            graphInstalled -> getString(R.string.region_status_map_missing)
            else -> getString(R.string.region_status_not_installed)
        }
        status?.setTextColor(
            getTextColor(
                if (isReady) R.color.safe_green else R.color.text_muted,
            ),
        )

        btnDownload?.backgroundTintList = null
        if (isReady) {
            btnDownload?.text = if (region.id == selectedRegionId) "Active" else "Select"
            btnDownload?.setBackgroundResource(if (region.id == selectedRegionId) R.drawable.bg_button_primary else R.drawable.bg_button_white)
            btnDownload?.setTextColor(getTextColor(if (region.id == selectedRegionId) R.color.white else R.color.brand_primary))
        } else {
            btnDownload?.text = "Download"
            btnDownload?.setBackgroundResource(R.drawable.bg_button_white)
            btnDownload?.setTextColor(getTextColor(R.color.brand_primary))
        }

        if (isReady && region.id == selectedRegionId) {
            dot?.visibility = View.VISIBLE
            card?.strokeColor = getTextColor(R.color.brand_primary)
            card?.strokeWidth = 2
        } else {
            dot?.visibility = View.GONE
            card?.strokeColor = getTextColor(R.color.border_subtle)
            card?.strokeWidth = 1
        }

        btnDownload?.setOnClickListener {
            if (!isReady) {
                btnDownload.isEnabled = false
                btnDownload.text = "0%"
                lifecycleScope.launch {
                    val result = MapPackageManager.installPackage(this@RegionSelectionActivity, region.id) { pct ->
                        runOnUiThread { btnDownload.text = "$pct%" }
                    }
                    if (result.isSuccess) {
                        Toast.makeText(this@RegionSelectionActivity, "${region.displayName} offline package verified & installed.", Toast.LENGTH_SHORT).show()
                        selectedRegionId = region.id
                        updateStorageSummary()
                    } else {
                        Toast.makeText(this@RegionSelectionActivity, "Download failed: ${result.exceptionOrNull()?.message}", Toast.LENGTH_SHORT).show()
                    }
                    renderRegions()
                }
            } else {
                selectedRegionId = region.id
                renderRegions()
            }
        }

        row.setOnClickListener {
            if (isReady) {
                selectedRegionId = region.id
                renderRegions()
            } else {
                btnDownload?.performClick()
            }
        }

        return row
    }

    private fun continueWithSelection() {
        val region = MapRegionCatalog.byId(selectedRegionId)
        if (region == null) {
            Toast.makeText(this, "Please select an official region.", Toast.LENGTH_SHORT).show()
            return
        }
        container.setActiveRegion(region.id)
        startActivity(
            Intent(this, HomeActivity::class.java)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK),
        )
        finish()
    }

    private fun getTextColor(resId: Int): Int = androidx.core.content.ContextCompat.getColor(this, resId)
}
