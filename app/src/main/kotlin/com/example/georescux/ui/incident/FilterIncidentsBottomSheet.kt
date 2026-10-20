package com.example.georescux.ui.incident

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.ImageButton
import android.widget.TextView
import com.example.georescux.R
import com.example.georescux.data.incident.IncidentFilter
import com.example.georescux.data.incident.IncidentReport
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import com.google.android.material.chip.Chip
import com.google.android.material.chip.ChipGroup

class FilterIncidentsBottomSheet : BottomSheetDialogFragment() {

    private var currentFilter: IncidentFilter = IncidentFilter()
    private var allIncidents: List<IncidentReport> = emptyList()

    private lateinit var chipGroupType: ChipGroup
    private lateinit var chipGroupSeverity: ChipGroup
    private lateinit var textMatchCount: TextView
    private lateinit var buttonApply: Button

    var onFilterAppliedListener: ((IncidentFilter) -> Unit)? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View? {
        return inflater.inflate(R.layout.bottom_sheet_filter_incidents, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        view.findViewById<ImageButton>(R.id.buttonCloseFilterSheet).setOnClickListener {
            dismiss()
        }

        chipGroupType = view.findViewById(R.id.chipGroupFilterType)
        chipGroupSeverity = view.findViewById(R.id.chipGroupFilterSeverity)
        textMatchCount = view.findViewById(R.id.textFilterMatchCount)
        buttonApply = view.findViewById(R.id.buttonApplyFilter)

        // Select initial chips based on currentFilter
        initSelectedChips(view)

        // Set listeners to update match count live
        chipGroupType.setOnCheckedStateChangeListener { _, _ -> updateCount() }
        chipGroupSeverity.setOnCheckedStateChangeListener { _, _ -> updateCount() }

        // Reset button
        view.findViewById<Button>(R.id.buttonResetFilters).setOnClickListener {
            view.findViewById<Chip>(R.id.chipFilterTypeAll)?.isChecked = true
            view.findViewById<Chip>(R.id.chipFilterSevAll)?.isChecked = true
            updateCount()
        }

        // Apply button
        buttonApply.setOnClickListener {
            val filter = buildFilterFromChips()
            onFilterAppliedListener?.invoke(filter)
            dismiss()
        }

        updateCount()
    }

    private fun initSelectedChips(root: View) {
        val selectedType = currentFilter.type?.uppercase()
        when (selectedType) {
            "FLOOD" -> root.findViewById<Chip>(R.id.chipFilterTypeFlood)?.isChecked = true
            "EARTHQUAKE" -> root.findViewById<Chip>(R.id.chipFilterTypeEarthquake)?.isChecked = true
            "LANDSLIDE" -> root.findViewById<Chip>(R.id.chipFilterTypeLandslide)?.isChecked = true
            "FIRE" -> root.findViewById<Chip>(R.id.chipFilterTypeFire)?.isChecked = true
            "ROUTE_BLOCK", "ROAD_BLOCK" -> root.findViewById<Chip>(R.id.chipFilterTypeRoadBlock)?.isChecked = true
            "BUILDING_COLLAPSE", "COLLAPSE" -> root.findViewById<Chip>(R.id.chipFilterTypeCollapse)?.isChecked = true
            "MEDICAL" -> root.findViewById<Chip>(R.id.chipFilterTypeMedical)?.isChecked = true
            "OTHER" -> root.findViewById<Chip>(R.id.chipFilterTypeOther)?.isChecked = true
            else -> root.findViewById<Chip>(R.id.chipFilterTypeAll)?.isChecked = true
        }

        val selectedSev = currentFilter.severity?.uppercase()
        when (selectedSev) {
            "CRITICAL" -> root.findViewById<Chip>(R.id.chipFilterSevCritical)?.isChecked = true
            "HIGH" -> root.findViewById<Chip>(R.id.chipFilterSevHigh)?.isChecked = true
            "MEDIUM" -> root.findViewById<Chip>(R.id.chipFilterSevMedium)?.isChecked = true
            "LOW" -> root.findViewById<Chip>(R.id.chipFilterSevLow)?.isChecked = true
            else -> root.findViewById<Chip>(R.id.chipFilterSevAll)?.isChecked = true
        }
    }

    private fun buildFilterFromChips(): IncidentFilter {
        val typeId = chipGroupType.checkedChipId
        val type = when (typeId) {
            R.id.chipFilterTypeFlood -> "FLOOD"
            R.id.chipFilterTypeEarthquake -> "EARTHQUAKE"
            R.id.chipFilterTypeLandslide -> "LANDSLIDE"
            R.id.chipFilterTypeFire -> "FIRE"
            R.id.chipFilterTypeRoadBlock -> "ROUTE_BLOCK"
            R.id.chipFilterTypeCollapse -> "BUILDING_COLLAPSE"
            R.id.chipFilterTypeMedical -> "MEDICAL"
            R.id.chipFilterTypeOther -> "OTHER"
            else -> null // ALL
        }

        val sevId = chipGroupSeverity.checkedChipId
        val severity = when (sevId) {
            R.id.chipFilterSevCritical -> "CRITICAL"
            R.id.chipFilterSevHigh -> "HIGH"
            R.id.chipFilterSevMedium -> "MEDIUM"
            R.id.chipFilterSevLow -> "LOW"
            else -> null // ALL
        }

        return IncidentFilter(type = type, severity = severity)
    }

    private fun updateCount() {
        val filter = buildFilterFromChips()
        val total = allIncidents.size
        val matched = allIncidents.count { filter.matches(it) }

        if (!filter.isActive) {
            textMatchCount.text = "Showing all $total incidents"
            buttonApply.text = "Show All ($total)"
        } else {
            textMatchCount.text = "Filters match $matched of $total incidents"
            buttonApply.text = "Show $matched Incidents"
        }
    }

    companion object {
        fun newInstance(
            currentFilter: IncidentFilter,
            allIncidents: List<IncidentReport>,
        ): FilterIncidentsBottomSheet {
            return FilterIncidentsBottomSheet().apply {
                this.currentFilter = currentFilter
                this.allIncidents = allIncidents
            }
        }
    }
}
