package com.example.georescux.ui.help

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.LayoutInflater
import android.view.View
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.example.georescux.GeoRescuXApplication
import com.example.georescux.R
import com.example.georescux.data.maps.MapRegionCatalog
import com.example.georescux.ui.common.ActiveSosBanner
import com.example.georescux.ui.common.BottomNav
import com.google.android.material.dialog.MaterialAlertDialogBuilder

/**
 * Global Help & Support Center (reference 1, Screen 9 & Screen 10):
 * - Quick Help Guide modal
 * - Interactive searchable FAQs
 * - Real Support actions (dialer & email)
 * - Safe Route Pro explanation modal (Screen 10)
 * - Dynamic emergency readiness check
 */
class HelpActivity : AppCompatActivity() {

    private val categoryFilter = mutableListOf<String>()
    private var searchQuery: String = ""

    override fun onResume() {
        super.onResume()
        ActiveSosBanner.refresh(this)
        checkEmergencyReadiness()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_help)

        findViewById<View>(R.id.buttonHelpBack)?.setOnClickListener { finish() }

        // Action 1: Quick Help Guide
        findViewById<View>(R.id.cardQuickGuide)?.setOnClickListener {
            showQuickHelpGuideDialog()
        }

        // Action 2: FAQs scroll
        findViewById<View>(R.id.cardFaqs)?.setOnClickListener {
            val searchInput = findViewById<android.widget.EditText>(R.id.editHelpSearch)
            searchInput?.requestFocus()
        }

        // Action 3: Contact Support
        findViewById<View>(R.id.cardContactSupport)?.setOnClickListener {
            showContactSupportDialog()
        }

        // Action 4: Safe Route Pro Info (Screen 10 & Screen 15)
        val proClickListener = View.OnClickListener { showSafeRouteProInfoDialog() }
        findViewById<View>(R.id.cardSafeRouteProBanner)?.setOnClickListener(proClickListener)
        findViewById<View>(R.id.buttonLearnSafeRoutePro)?.setOnClickListener(proClickListener)

        setupFilters()
        renderFaqs()
        checkEmergencyReadiness()

        BottomNav.bind(this, R.id.nav_more)
    }

    private fun checkEmergencyReadiness() {
        val container = (application as GeoRescuXApplication).appContainer
        val activeRegion = MapRegionCatalog.byId(container.activeRegionId)
        val mapReady = activeRegion != null && MapRegionCatalog.isGraphInstalled(assets, activeRegion)
        val contactsCount = container.contactsRepository.getContacts().size

        val textStatus = findViewById<TextView>(R.id.textReadinessStatus)
        val iconStatus = findViewById<TextView>(R.id.iconReadiness)

        if (mapReady && contactsCount > 0) {
            textStatus?.text = "You're fully ready! Regional maps active · $contactsCount emergency contacts set."
            iconStatus?.text = "✓"
            iconStatus?.setTextColor(androidx.core.content.ContextCompat.getColor(this, R.color.safe_green))
        } else if (mapReady) {
            textStatus?.text = "Offline maps ready. Add emergency contacts to notify family during SOS."
            iconStatus?.text = "!"
            iconStatus?.setTextColor(androidx.core.content.ContextCompat.getColor(this, R.color.amber_alert))
        } else {
            textStatus?.text = "Action recommended: download your regional map package for offline routing."
            iconStatus?.text = "!"
            iconStatus?.setTextColor(androidx.core.content.ContextCompat.getColor(this, R.color.sos_red))
        }
    }

    private fun showQuickHelpGuideDialog() {
        MaterialAlertDialogBuilder(this)
            .setTitle("Quick Help Guide")
            .setMessage(
                "1. Emergency SOS:\nTap or shake to activate SOS. Broadcasts locally over BLE mesh and queues emergency backup.\n\n" +
                "2. Safe Route:\nComputes safe navigation corridors away from active hazards using native C++ offline algorithms.\n\n" +
                "3. Offline Maps:\nDownload your state map once. All routing and map rendering work 100% without internet.\n\n" +
                "4. Mesh Relay:\nNearby GeoRescuX devices forward emergency signals hop-by-hop until reaching network coverage."
            )
            .setPositiveButton("Got It", null)
            .show()
    }

    private fun showContactSupportDialog() {
        val options = arrayOf("Call National Emergency Helpline (112)", "Email GeoRescuX Support (support@georescux.org)")
        MaterialAlertDialogBuilder(this)
            .setTitle("Contact Support")
            .setItems(options) { _, which ->
                when (which) {
                    0 -> {
                        try {
                            val intent = Intent(Intent.ACTION_DIAL, Uri.parse("tel:112"))
                            startActivity(intent)
                        } catch (e: Exception) {
                            Toast.makeText(this, "Could not open dialer", Toast.LENGTH_SHORT).show()
                        }
                    }
                    1 -> {
                        try {
                            val emailIntent = Intent(Intent.ACTION_SENDTO).apply {
                                data = Uri.parse("mailto:support@georescux.org")
                                putExtra(Intent.EXTRA_SUBJECT, "GeoRescuX App Support Request")
                            }
                            startActivity(emailIntent)
                        } catch (e: Exception) {
                            Toast.makeText(this, "No email app found", Toast.LENGTH_SHORT).show()
                        }
                    }
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun showSafeRouteProInfoDialog() {
        val dialogView = LayoutInflater.from(this).inflate(R.layout.dialog_safe_route_pro_info, null)
        val dialog = MaterialAlertDialogBuilder(this)
            .setView(dialogView)
            .create()

        dialogView.findViewById<Button>(R.id.buttonDismissProInfo)?.setOnClickListener {
            dialog.dismiss()
        }
        dialog.show()
    }

    private fun setupFilters() {
        val categories = listOf(ALL_CATEGORIES) + HelpContent.Category.values().map { it.title }
        categoryFilter.clear()
        categoryFilter.addAll(categories)

        val spinner = findViewById<android.widget.Spinner>(R.id.spinnerHelpCategory)
        spinner?.adapter = ArrayAdapter(
            this,
            android.R.layout.simple_spinner_dropdown_item,
            categories,
        )
        spinner?.onItemSelectedListener = object : android.widget.AdapterView.OnItemSelectedListener {
            override fun onItemSelected(
                parent: android.widget.AdapterView<*>?,
                view: View?,
                position: Int,
                id: Long,
            ) {
                renderFaqs()
            }

            override fun onNothingSelected(parent: android.widget.AdapterView<*>?) {}
        }

        findViewById<android.widget.EditText>(R.id.editHelpSearch)?.addTextChangedListener(
            object : TextWatcher {
                override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
                override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
                override fun afterTextChanged(s: Editable?) {
                    searchQuery = s?.toString()?.trim()?.lowercase().orEmpty()
                    renderFaqs()
                }
            },
        )
    }

    private fun selectedCategoryTitle(): String? {
        val spinner = findViewById<android.widget.Spinner>(R.id.spinnerHelpCategory) ?: return null
        val position = spinner.selectedItemPosition
        if (position == 0 || position !in categoryFilter.indices) return null
        return categoryFilter[position]
    }

    private fun renderFaqs() {
        val container = findViewById<android.widget.LinearLayout>(R.id.helpListContainer) ?: return
        val empty = findViewById<TextView>(R.id.textHelpEmpty)
        container.removeAllViews()

        val categoryTitle = selectedCategoryTitle()
        val visible = HelpContent.items.filter { faq ->
            val matchesCategory = categoryTitle == null || faq.category.title == categoryTitle
            val matchesSearch = searchQuery.isEmpty() ||
                faq.question.lowercase().contains(searchQuery) ||
                faq.answer.lowercase().contains(searchQuery)
            matchesCategory && matchesSearch
        }

        empty?.visibility = if (visible.isEmpty()) View.VISIBLE else View.GONE

        visible.forEach { faq ->
            val item = LayoutInflater.from(this)
                .inflate(R.layout.item_help, container, false)

            item.findViewById<TextView>(R.id.textFaqCategory)?.text = faq.category.title
            item.findViewById<TextView>(R.id.textFaqQuestion)?.text = faq.question
            item.findViewById<TextView>(R.id.textFaqAnswer)?.text = faq.answer

            item.setOnClickListener {
                val answer = item.findViewById<TextView>(R.id.textFaqAnswer)
                val toggle = item.findViewById<TextView>(R.id.textFaqToggle)
                val expanded = answer?.visibility == View.VISIBLE
                answer?.visibility = if (expanded) View.GONE else View.VISIBLE
                toggle?.text = if (expanded) "▾" else "▴"
            }

            container.addView(item)
        }
    }

    private companion object {
        const val ALL_CATEGORIES = "All categories"
    }
}
