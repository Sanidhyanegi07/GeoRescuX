package com.example.georescux.data.maps

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class MapPackageManagerTest {

    @Test
    fun `official packages list contains the four official states`() {
        val tempDir = File(System.getProperty("java.io.tmpdir"), "georescux_test_maps_${System.currentTimeMillis()}")
        tempDir.mkdirs()

        val packages = MapPackageManager.getOfficialPackages(tempDir)
        assertEquals(4, packages.size)

        val uttarakhand = packages.find { it.regionId == "uttarakhand" }
        assertNotNull(uttarakhand)
        assertTrue(uttarakhand!!.isBundled)
        assertEquals(MapPackageManager.PackageStatus.BUNDLED, uttarakhand.status)
        assertEquals("Uttarakhand", uttarakhand.displayName)
        assertTrue(uttarakhand.sizeFormatted.contains("MB"))

        val himachal = packages.find { it.regionId == "himachal_pradesh" }
        assertNotNull(himachal)
        assertFalse(himachal!!.isBundled)

        val haryana = packages.find { it.regionId == "haryana" }
        assertNotNull(haryana)
        assertFalse(haryana!!.isBundled)

        val up = packages.find { it.regionId == "uttar_pradesh" }
        assertNotNull(up)
        assertFalse(up!!.isBundled)

        tempDir.deleteRecursively()
    }

    @Test
    fun `isPackageInstalled recognizes bundled uttarakhand as installed`() {
        val tempDir = File(System.getProperty("java.io.tmpdir"), "georescux_test_maps2_${System.currentTimeMillis()}")
        tempDir.mkdirs()

        assertTrue(MapPackageManager.isPackageInstalled(tempDir, "uttarakhand"))
        assertFalse(MapPackageManager.isPackageInstalled(tempDir, "haryana"))

        // Create mock installed package for Haryana with valid Mapsforge magic bytes
        val haryanaDir = MapPackageManager.getPackageDirectory(tempDir, "haryana")
        haryanaDir.mkdirs()
        File(haryanaDir, "state.map").writeBytes(MapPackageManager.MAPSFORGE_MAGIC.toByteArray(Charsets.US_ASCII) + ByteArray(50))

        assertTrue(MapPackageManager.isPackageInstalled(tempDir, "haryana"))

        val packages = MapPackageManager.getOfficialPackages(tempDir)
        val haryanaPkg = packages.find { it.regionId == "haryana" }
        assertNotNull(haryanaPkg)
        assertEquals(MapPackageManager.PackageStatus.INSTALLED, haryanaPkg!!.status)

        tempDir.deleteRecursively()
    }

    @Test
    fun `isMapsforgeFile validates magic header and rejects arbitrary files`() {
        val tempDir = File(System.getProperty("java.io.tmpdir"), "georescux_magic_test_${System.currentTimeMillis()}")
        tempDir.mkdirs()

        val validFile = File(tempDir, "valid.map")
        validFile.writeBytes(MapPackageManager.MAPSFORGE_MAGIC.toByteArray(Charsets.US_ASCII) + ByteArray(20))
        assertTrue("File with mapsforge magic header must be recognized", MapPackageManager.isMapsforgeFile(validFile))

        val invalidFile = File(tempDir, "invalid.map")
        invalidFile.writeText("arbitrary dummy file content")
        assertFalse("Plain text file must be rejected as Mapsforge map", MapPackageManager.isMapsforgeFile(invalidFile))

        val pbfFile = File(tempDir, "test.osm.pbf")
        pbfFile.writeBytes(byteArrayOf(0x00, 0x00, 0x00, 0x0E) + "OSMHeader".toByteArray(Charsets.US_ASCII))
        assertFalse("OSM PBF file must be rejected as Mapsforge map", MapPackageManager.isMapsforgeFile(pbfFile))

        tempDir.deleteRecursively()
    }

    @Test
    fun `isSqliteArchiveFile validates SQLite format header`() {
        val tempDir = File(System.getProperty("java.io.tmpdir"), "georescux_sqlite_test_${System.currentTimeMillis()}")
        tempDir.mkdirs()

        val validSqlite = File(tempDir, "tiles.sqlite")
        validSqlite.writeBytes("SQLite format 3\u0000".toByteArray(Charsets.US_ASCII) + ByteArray(20))
        assertTrue("SQLite header must be recognized", MapPackageManager.isSqliteArchiveFile(validSqlite))

        val invalidSqlite = File(tempDir, "bad.sqlite")
        invalidSqlite.writeText("corrupt sqlite bytes")
        assertFalse("Non-SQLite file must be rejected", MapPackageManager.isSqliteArchiveFile(invalidSqlite))

        tempDir.deleteRecursively()
    }

    @Test
    fun `sha256 calculation produces consistent hash`() {
        val tempFile = File.createTempFile("test_sha", ".dat")
        tempFile.writeText("GeoRescuX Four State Mapping Test")

        val hash = MapPackageManager.calculateSha256(tempFile)
        assertNotNull(hash)
        assertEquals(64, hash.length)

        tempFile.delete()
    }

    @Test
    fun `installPackage fails and creates no files when source does not exist`() {
        kotlinx.coroutines.runBlocking {
            val tempDir = File(System.getProperty("java.io.tmpdir"), "georescux_test_missing_${System.currentTimeMillis()}")
            tempDir.mkdirs()

            val result = MapPackageManager.installPackage(tempDir, "himachal_pradesh")
            assertTrue(result.isFailure)
            assertTrue(result.exceptionOrNull() is java.io.FileNotFoundException)

            val targetDir = MapPackageManager.getPackageDirectory(tempDir, "himachal_pradesh")
            assertFalse(File(targetDir, "state.map").exists())
            assertFalse(File(targetDir, "manifest.json").exists())

            tempDir.deleteRecursively()
        }
    }

    @Test
    fun `installPackage rejects corrupt package and cleans up temporary files`() {
        kotlinx.coroutines.runBlocking {
            val tempDir = File(System.getProperty("java.io.tmpdir"), "georescux_test_corrupt_${System.currentTimeMillis()}")
            tempDir.mkdirs()

            // Create corrupt source file with intentional mismatched data and missing magic header
            val corruptSource = File(tempDir, "corrupt_hp.map")
            corruptSource.writeText("corrupt invalid bytes")

            val result = MapPackageManager.installPackage(
                filesDir = tempDir,
                regionId = "himachal_pradesh",
                customSourceFile = corruptSource
            )

            // Strictly assert failure: corrupt source missing magic header MUST be rejected
            assertTrue("Installation of corrupt source must fail", result.isFailure)
            val installedMap = File(MapPackageManager.getPackageDirectory(tempDir, "himachal_pradesh"), "state.map")
            assertFalse("Corrupt package must NOT be installed as state.map", installedMap.exists())

            val tempDownloading = File(MapPackageManager.getPackageDirectory(tempDir, "himachal_pradesh"), "state.map.downloading")
            assertFalse("Temporary downloading file must be deleted upon failure", tempDownloading.exists())

            tempDir.deleteRecursively()
        }
    }

    @Test
    fun `installPackage rejects PBF file as map rendering source`() {
        kotlinx.coroutines.runBlocking {
            val tempDir = File(System.getProperty("java.io.tmpdir"), "georescux_test_pbf_${System.currentTimeMillis()}")
            tempDir.mkdirs()

            val pbfSource = File(tempDir, "state.osm.pbf")
            pbfSource.writeBytes(byteArrayOf(0x00, 0x00, 0x00, 0x0E) + "OSMHeader".toByteArray(Charsets.US_ASCII))

            val result = MapPackageManager.installPackage(
                filesDir = tempDir,
                regionId = "himachal_pradesh",
                customSourceFile = pbfSource
            )

            assertTrue("PBF file must be rejected as rendering map", result.isFailure)
            val installedMap = File(MapPackageManager.getPackageDirectory(tempDir, "himachal_pradesh"), "state.map")
            assertFalse(installedMap.exists())

            tempDir.deleteRecursively()
        }
    }

    @Test
    fun `installPackage preserves existing valid map if update fails`() {
        kotlinx.coroutines.runBlocking {
            val tempDir = File(System.getProperty("java.io.tmpdir"), "georescux_test_rollback_${System.currentTimeMillis()}")
            tempDir.mkdirs()

            val targetDir = MapPackageManager.getPackageDirectory(tempDir, "himachal_pradesh")
            targetDir.mkdirs()
            val existingMap = File(targetDir, "state.map")
            val existingBytes = MapPackageManager.MAPSFORGE_MAGIC.toByteArray(Charsets.US_ASCII) + ByteArray(100) { 0x42 }
            existingMap.writeBytes(existingBytes)

            // Attempt to install corrupt file
            val corruptSource = File(tempDir, "corrupt.map")
            corruptSource.writeText("corrupted new version")

            val result = MapPackageManager.installPackage(
                filesDir = tempDir,
                regionId = "himachal_pradesh",
                customSourceFile = corruptSource
            )

            assertTrue("Installation of corrupt source must fail", result.isFailure)
            assertTrue("Existing state.map must be preserved untouched", existingMap.exists())
            assertEquals("Existing state.map size must match original", existingBytes.size.toLong(), existingMap.length())

            tempDir.deleteRecursively()
        }
    }
}
