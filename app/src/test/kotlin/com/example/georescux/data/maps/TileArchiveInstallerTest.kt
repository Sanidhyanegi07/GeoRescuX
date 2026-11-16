package com.example.georescux.data.maps

import com.example.georescux.domain.routing.MapRegion
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import java.io.ByteArrayInputStream
import java.io.File

class TileArchiveInstallerTest {

    private lateinit var tempDir: File

    @Before
    fun setUp() {
        tempDir = File(System.getProperty("java.io.tmpdir"), "tile_installer_test_${System.currentTimeMillis()}")
        tempDir.mkdirs()
    }

    @After
    fun tearDown() {
        tempDir.deleteRecursively()
    }

    private val testRegion = MapRegion(
        id = "test_state",
        displayName = "Test State",
        minLatitude = 29.0,
        maxLatitude = 31.0,
        minLongitude = 77.0,
        maxLongitude = 79.0,
        version = 1,
        graphAssetPath = "route_graph_test",
        tileAssetPath = "maps/test_state-tiles.map"
    )

    private fun validMapsforgeBytes(): ByteArray {
        val magic = "mapsforge binary OSM\u0000\u0000\u0000".toByteArray(Charsets.US_ASCII)
        val dummyData = ByteArray(100) { 0x42.toByte() }
        return magic + dummyData
    }

    @Test
    fun `blank tileAssetPath returns null and creates nothing`() {
        val blankRegion = testRegion.copy(tileAssetPath = "")
        val result = TileArchiveInstaller.ensureExtracted(tempDir, { ByteArrayInputStream(byteArrayOf()) }, blankRegion)
        assertNull(result)
        val osmdroidDir = File(tempDir, "osmdroid")
        assertTrue(!osmdroidDir.exists() || osmdroidDir.listFiles()?.isEmpty() == true)
    }

    @Test
    fun `valid mapsforge asset extracts cleanly to target file`() {
        val assetBytes = validMapsforgeBytes()
        val result = TileArchiveInstaller.ensureExtracted(
            tempDir,
            { ByteArrayInputStream(assetBytes) },
            testRegion
        )

        assertNotNull(result)
        assertTrue(result!!.exists())
        assertEquals("test_state-tiles.map", result.name)
        assertTrue(MapPackageManager.isMapsforgeFile(result))
    }

    @Test
    fun `existing valid target is reused without re-extracting`() {
        val assetBytes = validMapsforgeBytes()
        val firstResult = TileArchiveInstaller.ensureExtracted(
            tempDir,
            { ByteArrayInputStream(assetBytes) },
            testRegion
        )
        assertNotNull(firstResult)

        var providerCalled = false
        val secondResult = TileArchiveInstaller.ensureExtracted(
            tempDir,
            {
                providerCalled = true
                ByteArrayInputStream(assetBytes)
            },
            testRegion
        )

        assertNotNull(secondResult)
        assertEquals(firstResult!!.absolutePath, secondResult!!.absolutePath)
        assertTrue("Provider should not be called when valid file already exists", !providerCalled)
    }

    @Test
    fun `corrupted target file triggers self-healing re-extraction`() {
        // Pre-create a corrupt target file
        val osmdroidDir = File(tempDir, "osmdroid")
        osmdroidDir.mkdirs()
        val corruptTarget = File(osmdroidDir, "test_state-tiles.map")
        corruptTarget.writeBytes(byteArrayOf(0x01, 0x02, 0x03, 0x04))

        val assetBytes = validMapsforgeBytes()
        val healed = TileArchiveInstaller.ensureExtracted(
            tempDir,
            { ByteArrayInputStream(assetBytes) },
            testRegion
        )

        assertNotNull(healed)
        assertTrue(healed!!.exists())
        assertTrue(MapPackageManager.isMapsforgeFile(healed))
    }

    @Test
    fun `corrupted asset stream is rejected and does not leave invalid target`() {
        val corruptBytes = byteArrayOf(0x10, 0x20, 0x30, 0x40, 0x50)
        val result = TileArchiveInstaller.ensureExtracted(
            tempDir,
            { ByteArrayInputStream(corruptBytes) },
            testRegion
        )

        assertNull(result)
        val target = File(File(tempDir, "osmdroid"), "test_state-tiles.map")
        assertTrue("Corrupted target should not exist", !target.exists())
    }
}
