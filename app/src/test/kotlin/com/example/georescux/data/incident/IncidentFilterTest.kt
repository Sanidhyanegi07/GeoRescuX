package com.example.georescux.data.incident

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class IncidentFilterTest {

    private val floodCritical = IncidentReport(
        id = "1", type = "FLOOD", title = "Flood 1", description = "Water rising",
        latitude = 30.1, longitude = 78.1, severity = "CRITICAL"
    )
    private val floodHigh = IncidentReport(
        id = "2", type = "FLOOD", title = "Flood 2", description = "River bank overflow",
        latitude = 30.2, longitude = 78.2, severity = "HIGH"
    )
    private val earthquakeCritical = IncidentReport(
        id = "3", type = "EARTHQUAKE", title = "Quake 1", description = "Building cracked",
        latitude = 30.3, longitude = 78.3, severity = "CRITICAL"
    )
    private val fireMedium = IncidentReport(
        id = "4", type = "FIRE", title = "Fire 1", description = "Forest smoke",
        latitude = 30.4, longitude = 78.4, severity = "MEDIUM"
    )
    private val roadBlockHigh = IncidentReport(
        id = "5", type = "ROUTE_BLOCK", title = "Block 1", description = "Boulders on route",
        latitude = 30.5, longitude = 78.5, severity = "HIGH"
    )

    private val allSample = listOf(floodCritical, floodHigh, earthquakeCritical, fireMedium, roadBlockHigh)

    @Test
    fun `default filter matches all incidents`() {
        val filter = IncidentFilter()
        assertFalse(filter.isActive)
        assertEquals("All Incidents", filter.describe())

        val matched = allSample.filter { filter.matches(it) }
        assertEquals(5, matched.size)
    }

    @Test
    fun `type filter isolates specific incident types`() {
        val floodFilter = IncidentFilter(type = "FLOOD")
        assertTrue(floodFilter.isActive)

        val floodMatches = allSample.filter { floodFilter.matches(it) }
        assertEquals(2, floodMatches.size)
        assertTrue(floodMatches.all { it.type == "FLOOD" })

        val earthquakeFilter = IncidentFilter(type = "EARTHQUAKE")
        val earthquakeMatches = allSample.filter { earthquakeFilter.matches(it) }
        assertEquals(1, earthquakeMatches.size)
        assertEquals("3", earthquakeMatches.first().id)

        val fireFilter = IncidentFilter(type = "FIRE")
        val fireMatches = allSample.filter { fireFilter.matches(it) }
        assertEquals(1, fireMatches.size)
        assertEquals("4", fireMatches.first().id)
    }

    @Test
    fun `severity filter isolates critical and high emergencies`() {
        val criticalFilter = IncidentFilter(severity = "CRITICAL")
        assertTrue(criticalFilter.isActive)

        val criticalMatches = allSample.filter { criticalFilter.matches(it) }
        assertEquals(2, criticalMatches.size)
        assertTrue(criticalMatches.contains(floodCritical))
        assertTrue(criticalMatches.contains(earthquakeCritical))

        val highFilter = IncidentFilter(severity = "HIGH")
        val highMatches = allSample.filter { highFilter.matches(it) }
        assertEquals(2, highMatches.size)
        assertTrue(highMatches.contains(floodHigh))
        assertTrue(highMatches.contains(roadBlockHigh))
    }

    @Test
    fun `combined type and severity filter filters with AND logic`() {
        val criticalFloodFilter = IncidentFilter(type = "FLOOD", severity = "CRITICAL")
        assertTrue(criticalFloodFilter.isActive)

        val matched = allSample.filter { criticalFloodFilter.matches(it) }
        assertEquals(1, matched.size)
        assertEquals("1", matched.first().id)
    }

    @Test
    fun `filter description generates human readable string`() {
        val filter = IncidentFilter(type = "FLOOD", severity = "CRITICAL")
        assertEquals("🌊 Flood • CRITICAL Severity", filter.describe())

        val fireOnly = IncidentFilter(type = "FIRE")
        assertEquals("🔥 Fire", fireOnly.describe())

        val criticalOnly = IncidentFilter(severity = "CRITICAL")
        assertEquals("CRITICAL Severity", criticalOnly.describe())
    }
}
