package com.example.georescux.domain.admin

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * District drill-down reference data (spec §11.2). The boxes are coarse, so
 * these tests check the classification contract, not survey-grade borders.
 */
class DistrictCatalogTest {

    @Test
    fun `Nainital is listed under Uttarakhand`() {
        val nainital = DistrictCatalog.allDistricts.firstOrNull { it.id == "nainital" }
        assertNotNull(nainital)
        assertEquals("uttarakhand", nainital!!.stateId)
        assertEquals("Nainital", nainital.displayName)
    }

    @Test
    fun `districts for Uttarakhand are non-empty and all belong to the state`() {
        val districts = DistrictCatalog.districtsForState("uttarakhand")

        assertTrue(districts.isNotEmpty())
        assertEquals(13, districts.size)
        assertTrue(districts.all { it.stateId == "uttarakhand" })
    }

    @Test
    fun `districts for Himachal Pradesh are non-empty and all belong to the state`() {
        val districts = DistrictCatalog.districtsForState("himachal_pradesh")

        assertTrue(districts.isNotEmpty())
        assertEquals(12, districts.size)
        assertTrue(districts.all { it.stateId == "himachal_pradesh" })
        assertNotNull(districts.find { it.displayName == "Shimla" })
    }

    @Test
    fun `districts for Haryana are non-empty and all belong to the state`() {
        val districts = DistrictCatalog.districtsForState("haryana")

        assertTrue(districts.isNotEmpty())
        assertTrue(districts.all { it.stateId == "haryana" })
        assertNotNull(districts.find { it.displayName == "Gurugram" })
    }

    @Test
    fun `districts for Uttar Pradesh are non-empty and all belong to the state`() {
        val districts = DistrictCatalog.districtsForState("uttar_pradesh")

        assertTrue(districts.isNotEmpty())
        assertTrue(districts.all { it.stateId == "uttar_pradesh" })
        assertNotNull(districts.find { it.displayName == "Lucknow" })
    }

    @Test
    fun `a position inside Nainital resolves to Nainital`() {
        // Roughly Nainital town.
        val district = DistrictCatalog.districtForLocation(29.39, 79.45)

        assertNotNull(district)
        assertEquals("nainital", district!!.id)
    }

    @Test
    fun `a position outside every shipped district resolves to null`() {
        // Mid-Indian-Ocean coordinates — no district covers this.
        assertNull(DistrictCatalog.districtForLocation(-10.0, 80.0))
    }

    @Test
    fun `an unknown state yields no districts`() {
        assertEquals(emptyList<District>(), DistrictCatalog.districtsForState("does_not_exist"))
    }

    @Test
    fun `contains is true inside the box and false outside it`() {
        val nainital = DistrictCatalog.districtsForState("uttarakhand").first { it.id == "nainital" }

        assertTrue(nainital.contains(29.3, 79.9))
        // Just outside the northern edge.
        assertTrue(!nainital.contains(29.9, 79.9))
    }
}
