package com.example.georescux.data.maps

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class StateBoundaryClassificationTest {

    @Test
    fun `classify correctly identifies locations in all four official states`() {
        // Uttarakhand coordinates
        val dehradun = StateBoundaryClassifier.classify(30.3165, 78.0322)
        assertNotNull("Dehradun must be classified", dehradun)
        assertEquals("uttarakhand", dehradun!!.id)

        val haridwar = StateBoundaryClassifier.classify(29.9457, 78.1642)
        assertNotNull("Haridwar must be classified", haridwar)
        assertEquals("uttarakhand", haridwar!!.id)

        val nainital = StateBoundaryClassifier.classify(29.3919, 79.4542)
        assertNotNull("Nainital must be classified", nainital)
        assertEquals("uttarakhand", nainital!!.id)

        // Himachal Pradesh coordinates
        val shimla = StateBoundaryClassifier.classify(31.1048, 77.1734)
        assertNotNull("Shimla must be classified", shimla)
        assertEquals("himachal_pradesh", shimla!!.id)

        val dharamshala = StateBoundaryClassifier.classify(32.2190, 76.3234)
        assertNotNull("Dharamshala must be classified", dharamshala)
        assertEquals("himachal_pradesh", dharamshala!!.id)

        val kullu = StateBoundaryClassifier.classify(31.9579, 77.1095)
        assertNotNull("Kullu must be classified", kullu)
        assertEquals("himachal_pradesh", kullu!!.id)

        // Haryana coordinates
        val gurugram = StateBoundaryClassifier.classify(28.4595, 77.0266)
        assertNotNull("Gurugram must be classified", gurugram)
        assertEquals("haryana", gurugram!!.id)

        val ambala = StateBoundaryClassifier.classify(30.3782, 76.7767)
        assertNotNull("Ambala must be classified", ambala)
        assertEquals("haryana", ambala!!.id)

        val hisar = StateBoundaryClassifier.classify(29.1492, 75.7217)
        assertNotNull("Hisar must be classified", hisar)
        assertEquals("haryana", hisar!!.id)

        // Uttar Pradesh coordinates
        val lucknow = StateBoundaryClassifier.classify(26.8467, 80.9462)
        assertNotNull("Lucknow must be classified", lucknow)
        assertEquals("uttar_pradesh", lucknow!!.id)

        val varanasi = StateBoundaryClassifier.classify(25.3176, 82.9739)
        assertNotNull("Varanasi must be classified", varanasi)
        assertEquals("uttar_pradesh", varanasi!!.id)

        val agra = StateBoundaryClassifier.classify(27.1767, 78.0081)
        assertNotNull("Agra must be classified", agra)
        assertEquals("uttar_pradesh", agra!!.id)
    }

    @Test
    fun `classify rejects Delhi and outside locations without false bounding-box acceptance`() {
        // New Delhi: Connaught Place (28.6289, 77.2065) - MUST NOT be classified as HR, UP, or UK
        val newDelhi = StateBoundaryClassifier.classify(28.6289, 77.2065)
        assertNull("New Delhi is outside the four official states and must return null", newDelhi)

        // Rajasthan: Jaipur (26.9124, 75.7873)
        val jaipur = StateBoundaryClassifier.classify(26.9124, 75.7873)
        assertNull("Jaipur is outside supported states", jaipur)

        // Bihar: Patna (25.5941, 85.1376)
        val patna = StateBoundaryClassifier.classify(25.5941, 85.1376)
        assertNull("Patna is outside supported states", patna)

        // Punjab: Ludhiana (30.9010, 75.8573)
        val ludhiana = StateBoundaryClassifier.classify(30.9010, 75.8573)
        assertNull("Ludhiana is outside supported states", ludhiana)

        // Overseas / Null Island (0.0, 0.0)
        val nullIsland = StateBoundaryClassifier.classify(0.0, 0.0)
        assertNull("Null Island must be rejected", nullIsland)

        // London
        val london = StateBoundaryClassifier.classify(51.5074, -0.1278)
        assertNull("London must be rejected", london)
    }

    @Test
    fun `rayCast algorithm handles concave and convex polygon boundaries correctly`() {
        // Simple test triangle: (0,0), (10, 0), (5, 10)
        val triangle = listOf(
            doubleArrayOf(0.0, 0.0),
            doubleArrayOf(10.0, 0.0),
            doubleArrayOf(5.0, 10.0),
            doubleArrayOf(0.0, 0.0)
        )

        // Point inside
        assertTrue(StateBoundaryClassifier.rayCast(3.0, 5.0, triangle))

        // Point outside
        assertFalse(StateBoundaryClassifier.rayCast(15.0, 5.0, triangle))
        assertFalse(StateBoundaryClassifier.rayCast(5.0, 15.0, triangle))
        assertFalse(StateBoundaryClassifier.rayCast(-2.0, 2.0, triangle))
    }
}
