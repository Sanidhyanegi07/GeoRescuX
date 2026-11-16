package com.example.georescux.data.maps

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class SafeHavenIndexTest {

    @Test
    fun `parse valid json with all facility categories`() {
        val json = """
            {
              "havens": [
                {
                  "nodeId": "sh_1",
                  "name": "District Hospital Dehradun",
                  "kind": "hospital",
                  "latitude": 30.3165,
                  "longitude": 78.0322,
                  "verified": true,
                  "source": "OpenStreetMap contributors (ODbL)",
                  "osmType": "node",
                  "osmId": "123456",
                  "snapDistanceMeters": 15
                },
                {
                  "nodeId": "sh_2",
                  "name": "Kotwali Police Station",
                  "kind": "police",
                  "latitude": 30.3200,
                  "longitude": 78.0350,
                  "verified": true,
                  "source": "OpenStreetMap contributors (ODbL)"
                },
                {
                  "nodeId": "sh_3",
                  "name": "City Fire Station",
                  "kind": "fire_station",
                  "latitude": 30.3250,
                  "longitude": 78.0400,
                  "verified": true
                },
                {
                  "nodeId": "sh_4",
                  "name": "District Disaster HQ",
                  "kind": "district_hq",
                  "latitude": 30.3300,
                  "longitude": 78.0450,
                  "verified": true
                }
              ]
            }
        """.trimIndent()

        val facilities = SafeHavenIndex.parse(json)
        assertEquals(4, facilities.size)

        val hospital = facilities[0]
        assertEquals("sh_1", hospital.nodeId)
        assertEquals("District Hospital Dehradun", hospital.name)
        assertEquals("hospital", hospital.kind)
        assertEquals("Emergency Hospital", hospital.categoryDisplayName)
        assertEquals("🏥", hospital.categoryEmoji)
        assertEquals(30.3165, hospital.latitude, 0.0001)
        assertEquals(78.0322, hospital.longitude, 0.0001)
        assertTrue(hospital.isVerified)
        assertEquals("node", hospital.osmType)
        assertEquals("123456", hospital.osmId)
        assertEquals(15, hospital.snapDistanceMeters)

        val police = facilities[1]
        assertEquals("Police Station", police.categoryDisplayName)
        assertEquals("👮", police.categoryEmoji)

        val fire = facilities[2]
        assertEquals("Fire & Rescue Station", fire.categoryDisplayName)
        assertEquals("🚒", fire.categoryEmoji)

        val hq = facilities[3]
        assertEquals("District Disaster HQ", hq.categoryDisplayName)
        assertEquals("🏛️", hq.categoryEmoji)
    }

    @Test
    fun `category display names and emojis cover default fallback`() {
        val facility = SafeHavenFacility(
            nodeId = "generic_1",
            name = "Community Shelter",
            kind = "shelter",
            latitude = 30.0,
            longitude = 78.0,
            isVerified = false
        )
        assertEquals("Emergency Resource Center", facility.categoryDisplayName)
        assertEquals("🛡️", facility.categoryEmoji)
        assertFalse(facility.isVerified)
    }

    @Test
    fun `findByNodeId finds existing facility and handles missing id`() {
        val list = listOf(
            SafeHavenFacility("h1", "Hospital A", "hospital", 30.0, 78.0, true),
            SafeHavenFacility("p1", "Police B", "police", 30.1, 78.1, true)
        )

        val found = SafeHavenIndex.findByNodeId(list, "p1")
        assertNotNull(found)
        assertEquals("Police B", found?.name)

        val missing = SafeHavenIndex.findByNodeId(list, "nonexistent")
        assertNull(missing)
    }

    @Test
    fun `findNearby filters by distance and sorts ascending`() {
        val refLat = 30.3165
        val refLon = 78.0322

        val close = SafeHavenFacility("c1", "Near Hospital", "hospital", 30.3200, 78.0350, true)
        val medium = SafeHavenFacility("m1", "Medium Police", "police", 30.4000, 78.1000, true)
        val far = SafeHavenFacility("f1", "Faraway Center", "shelter", 31.5000, 79.5000, true)

        val list = listOf(far, close, medium)

        val results = SafeHavenIndex.findNearby(list, refLat, refLon, maxDistanceMeters = 50_000.0)
        assertEquals(2, results.size)
        assertEquals("c1", results[0].first.nodeId)
        assertEquals("m1", results[1].first.nodeId)
        assertTrue(results[0].second < results[1].second)
    }

    @Test
    fun `empty or malformed json returns empty list`() {
        assertTrue(SafeHavenIndex.parse("").isEmpty())
        assertTrue(SafeHavenIndex.parse("{invalid json").isEmpty())
        assertTrue(SafeHavenIndex.parse("{}").isEmpty())
        assertTrue(SafeHavenIndex.parse("""{"havens": "not a list"}""").isEmpty())
    }

    @Test
    fun `authentic safe haven files for all four states load cleanly with real coordinates`() {
        val stateIds = listOf("uttarakhand", "himachal_pradesh", "haryana", "uttar_pradesh")

        for (stateId in stateIds) {
            val candidates = listOf(
                File("app/src/main/assets/maps/$stateId-safe_havens.json"),
                File("src/main/assets/maps/$stateId-safe_havens.json"),
                File("../app/src/main/assets/maps/$stateId-safe_havens.json"),
                File("output/$stateId/safe_havens.json"),
                File("../output/$stateId/safe_havens.json")
            )
            val file = candidates.firstOrNull { it.exists() && it.length() > 0L }
            assertNotNull("Safe havens file for $stateId should exist on disk", file)

            val facilities = SafeHavenIndex.loadFromFile(file!!)
            assertTrue("State $stateId must contain at least 5 safe havens, found ${facilities.size}", facilities.size >= 5)

            for (f in facilities) {
                assertTrue("Facility ${f.nodeId} must have non-blank name", f.name.isNotBlank())
                assertTrue("Facility ${f.name} latitude should be within India bounds", f.latitude in 23.5..34.0)
                assertTrue("Facility ${f.name} longitude should be within India bounds", f.longitude in 74.0..85.0)
            }
        }
    }
}
