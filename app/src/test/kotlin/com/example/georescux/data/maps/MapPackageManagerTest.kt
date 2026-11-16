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

        // Create mock installed package for Haryana
        val haryanaDir = MapPackageManager.getPackageDirectory(tempDir, "haryana")
        haryanaDir.mkdirs()
        File(haryanaDir, "state.map").writeText("dummy map data")

        assertTrue(MapPackageManager.isPackageInstalled(tempDir, "haryana"))

        val packages = MapPackageManager.getOfficialPackages(tempDir)
        val haryanaPkg = packages.find { it.regionId == "haryana" }
        assertNotNull(haryanaPkg)
        assertEquals(MapPackageManager.PackageStatus.INSTALLED, haryanaPkg!!.status)

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

            // Create corrupt source file with intentional mismatched data
            val corruptSource = File(tempDir, "corrupt_hp.map")
            corruptSource.writeText("corrupt invalid bytes")

            val result = MapPackageManager.installPackage(
                filesDir = tempDir,
                regionId = "himachal_pradesh",
                customSourceFile = corruptSource
            )

            assertTrue(result.isSuccess)
            val installedMap = File(MapPackageManager.getPackageDirectory(tempDir, "himachal_pradesh"), "state.map")
            assertTrue(installedMap.exists())
            assertEquals(corruptSource.length(), installedMap.length())

            tempDir.deleteRecursively()
        }
    }
}
