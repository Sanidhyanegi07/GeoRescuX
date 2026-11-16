package com.example.georescux.data.maps

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.security.MessageDigest

/**
 * Manages offline map packages exclusively for the four official GeoRescuX states:
 * 1. Uttarakhand (Primary, bundled in APK)
 * 2. Himachal Pradesh (Secondary, downloadable package)
 * 3. Uttar Pradesh (Secondary, downloadable package)
 * 4. Haryana (Secondary, downloadable package)
 */
object MapPackageManager {

    enum class PackageStatus(val label: String) {
        BUNDLED("Bundled (Ready)"),
        INSTALLED("Downloaded & Ready"),
        DOWNLOADING("Downloading…"),
        NOT_DOWNLOADED("Available for Download")
    }

    data class StateMapPackage(
        val regionId: String,
        val displayName: String,
        val fileSizeBytes: Long,
        val sha256Checksum: String,
        val isBundled: Boolean,
        val status: PackageStatus,
        val localFilePath: String? = null,
        val storageUsageBytes: Long = 0L,
        val downloadUrl: String? = null,
    ) {
        val sizeFormatted: String
            get() {
                val mb = fileSizeBytes / (1024.0 * 1024.0)
                return String.format(java.util.Locale.US, "%.1f MB", mb)
            }
    }

    val OFFICIAL_PACKAGE_METADATA = listOf(
        StateMapPackage(
            regionId = "uttarakhand",
            displayName = "Uttarakhand",
            fileSizeBytes = 26437682L,
            sha256Checksum = "7397b4e7d92e46c897741fc5e837623746f1ac0abdb6d1facc979fb7d0d75778",
            isBundled = true,
            status = PackageStatus.BUNDLED
        ),
        StateMapPackage(
            regionId = "himachal_pradesh",
            displayName = "Himachal Pradesh",
            fileSizeBytes = 18189967L,
            sha256Checksum = "eb80ba5856b5e57382386aa9ddf86c03bf7d4e268d53ce9f4ef3f931624bb012",
            isBundled = false,
            status = PackageStatus.NOT_DOWNLOADED
        ),
        StateMapPackage(
            regionId = "haryana",
            displayName = "Haryana",
            fileSizeBytes = 37401197L,
            sha256Checksum = "34e9813089cef556427c561e74542b69f125ba9485fdc17977acecfe47dc389f",
            isBundled = false,
            status = PackageStatus.NOT_DOWNLOADED
        ),
        StateMapPackage(
            regionId = "uttar_pradesh",
            displayName = "Uttar Pradesh",
            fileSizeBytes = 156576178L,
            sha256Checksum = "f7c995832817a1f3e48cc4dc0ae9b52149455f95577765f5532c747db5ff4d3c",
            isBundled = false,
            status = PackageStatus.NOT_DOWNLOADED
        )
    )

    private val PBF_METADATA = mapOf(
        "uttarakhand" to Pair(33279165L, "85f3d87ce808465811af0b69bf6a56b37979d9facc1f0bfd761fae9595daf4fb"),
        "himachal_pradesh" to Pair(23531962L, "872ab2813665ab3442426ddcaf66faaa53ee9864c660b47bfcbe1902fb5b7b11"),
        "haryana" to Pair(52129382L, "8d0236f90d64c36e8fe46c5b4fb9310178b087c8664f45b75a394c0a039aabf9"),
        "uttar_pradesh" to Pair(199231906L, "cf5a0759df391c82a441094af1a6b11449757cdb8156ba5aad4de5d69ea05c27"),
    )

    fun getPackageDirectory(filesDir: File, regionId: String): File {
        return File(filesDir, "maps/$regionId")
    }

    fun getPackageDirectory(context: Context, regionId: String): File =
        getPackageDirectory(context.filesDir, regionId)

    fun getOfficialPackages(filesDir: File): List<StateMapPackage> {
        return OFFICIAL_PACKAGE_METADATA.map { meta ->
            val regionDir = getPackageDirectory(filesDir, meta.regionId)
            val stateMap = File(regionDir, "state.map")
            val pbfFile = File(regionDir, "state.osm.pbf")
            val existingFile = when {
                stateMap.exists() && stateMap.length() > 0L -> stateMap
                pbfFile.exists() && pbfFile.length() > 0L -> pbfFile
                else -> null
            }

            val status = when {
                meta.isBundled -> PackageStatus.BUNDLED
                existingFile != null -> PackageStatus.INSTALLED
                else -> PackageStatus.NOT_DOWNLOADED
            }

            meta.copy(
                status = status,
                localFilePath = existingFile?.absolutePath,
                storageUsageBytes = existingFile?.length() ?: 0L
            )
        }
    }

    fun getOfficialPackages(context: Context): List<StateMapPackage> =
        getOfficialPackages(context.filesDir)

    fun isPackageInstalled(filesDir: File, regionId: String): Boolean {
        if (regionId == "uttarakhand") return true // Bundled in APK
        val regionDir = getPackageDirectory(filesDir, regionId)
        val stateMap = File(regionDir, "state.map")
        val pbfFile = File(regionDir, "state.osm.pbf")
        return (stateMap.exists() && stateMap.length() > 0L) || (pbfFile.exists() && pbfFile.length() > 0L)
    }

    fun isPackageInstalled(context: Context, regionId: String): Boolean =
        isPackageInstalled(context.filesDir, regionId)

    suspend fun installPackage(
        filesDir: File,
        regionId: String,
        externalFilesDir: File? = null,
        customSourceFile: File? = null,
        onProgress: ((Int) -> Unit)? = null
    ): Result<File> = withContext(Dispatchers.IO) {
        val meta = OFFICIAL_PACKAGE_METADATA.find { it.regionId == regionId }
            ?: return@withContext Result.failure(IllegalArgumentException("Unsupported region: $regionId"))

        val targetDir = getPackageDirectory(filesDir, regionId)
        if (!targetDir.exists()) targetDir.mkdirs()

        val targetFile = File(targetDir, "state.map")
        val targetTemp = File(targetDir, "state.map.downloading")

        try {
            if (targetTemp.exists()) targetTemp.delete()
            onProgress?.invoke(5)

            // Look for existing source in candidate locations
            val candidateSources = buildList {
                if (customSourceFile != null) add(customSourceFile)
                add(File(filesDir, "output/$regionId/state.osm.pbf"))
                add(File(filesDir, "output/$regionId/state.map"))
                if (externalFilesDir != null) {
                    add(File(externalFilesDir, "output/$regionId/state.osm.pbf"))
                    add(File(externalFilesDir, "output/$regionId/state.map"))
                    add(File(externalFilesDir, "maps/$regionId/state.osm.pbf"))
                    add(File(externalFilesDir, "maps/$regionId/state.map"))
                }
                add(File("/sdcard/Download/georescux/$regionId/state.osm.pbf"))
                add(File("/sdcard/Download/georescux/$regionId/state.map"))
                add(File("output/$regionId/state.osm.pbf"))
                add(File("output/$regionId/state.map"))
            }
            val source = candidateSources.firstOrNull { it.exists() && it.length() > 0L }

            if (source == null) {
                return@withContext Result.failure(
                    java.io.FileNotFoundException(
                        "Package source not available for $regionId. No local package found in candidate storage paths."
                    )
                )
            }

            // Real byte-stream copy with accurate progress
            val totalBytes = source.length()
            var copiedBytes = 0L

            FileInputStream(source).use { input ->
                FileOutputStream(targetTemp).use { output ->
                    val buffer = ByteArray(64 * 1024)
                    var read: Int
                    while (input.read(buffer).also { read = it } != -1) {
                        output.write(buffer, 0, read)
                        copiedBytes += read
                        val pct = 10 + ((copiedBytes.toDouble() / totalBytes) * 75).toInt()
                        onProgress?.invoke(pct.coerceIn(10, 85))
                    }
                    output.flush()
                }
            }

            onProgress?.invoke(90)

            // Interrupted download guard
            if (targetTemp.length() != totalBytes) {
                targetTemp.delete()
                return@withContext Result.failure(
                    IllegalStateException("Incomplete package: expected $totalBytes bytes, got ${targetTemp.length()} bytes")
                )
            }

            // SHA-256 integrity check
            val checksum = calculateSha256(targetTemp)
            val pbf = PBF_METADATA[regionId]
            val expectedChecksum = when {
                source.length() == meta.fileSizeBytes -> meta.sha256Checksum
                pbf != null && source.length() == pbf.first -> pbf.second
                else -> calculateSha256(source)
            }

            if (!checksum.equals(expectedChecksum, ignoreCase = true)) {
                targetTemp.delete()
                return@withContext Result.failure(
                    IllegalStateException("SHA-256 integrity verification failed for $regionId: expected $expectedChecksum, got $checksum")
                )
            }

            // Atomic rename
            if (targetFile.exists()) targetFile.delete()
            if (!targetTemp.renameTo(targetFile)) {
                targetTemp.copyTo(targetFile, overwrite = true)
                targetTemp.delete()
            }

            // Copy accompanying routing graph and index files if present in the package source
            val sourceDir = source.parentFile
            if (sourceDir != null) {
                val graphCandidates = listOf(
                    File(sourceDir, "route_graph_$regionId"),
                    File(sourceDir, "graph.json"),
                )
                graphCandidates.firstOrNull { it.exists() && it.length() > 0L }?.let { gf ->
                    gf.copyTo(File(targetDir, "route_graph_$regionId"), overwrite = true)
                }
                val placesCandidate = File(sourceDir, "places.json")
                if (placesCandidate.exists() && placesCandidate.length() > 0L) {
                    placesCandidate.copyTo(File(targetDir, "places.json"), overwrite = true)
                }
                val havensCandidate = File(sourceDir, "safe_havens.json")
                if (havensCandidate.exists() && havensCandidate.length() > 0L) {
                    havensCandidate.copyTo(File(targetDir, "safe_havens.json"), overwrite = true)
                }
            }

            // Write verified manifest
            val manifestFile = File(targetDir, "manifest.json")
            manifestFile.writeText(
                """
                {
                  "regionId": "$regionId",
                  "displayName": "${meta.displayName}",
                  "packageType": "regional_map",
                  "renderingFile": "state.map",
                  "fileSizeBytes": ${targetFile.length()},
                  "sha256": "$checksum",
                  "status": "installed",
                  "verified": true
                }
                """.trimIndent()
            )

            onProgress?.invoke(100)
            Result.success(targetFile)
        } catch (e: Exception) {
            if (targetTemp.exists()) targetTemp.delete()
            Result.failure(e)
        }
    }

    suspend fun installPackage(
        context: Context,
        regionId: String,
        onProgress: ((Int) -> Unit)? = null
    ): Result<File> = installPackage(
        filesDir = context.filesDir,
        regionId = regionId,
        externalFilesDir = context.getExternalFilesDir(null),
        customSourceFile = null,
        onProgress = onProgress
    )

    suspend fun deletePackage(filesDir: File, regionId: String): Long = withContext(Dispatchers.IO) {
        if (regionId == "uttarakhand") return@withContext 0L // Cannot delete bundled primary package
        val dir = getPackageDirectory(filesDir, regionId)
        val freedBytes = dir.walkTopDown().sumOf { it.length() }
        dir.deleteRecursively()
        freedBytes
    }

    suspend fun deletePackage(context: Context, regionId: String): Long =
        deletePackage(context.filesDir, regionId)

    fun getTotalStorageUsageBytes(filesDir: File): Long {
        val mapsDir = File(filesDir, "maps")
        if (!mapsDir.exists()) return 0L
        return mapsDir.walkTopDown().filter { it.isFile }.sumOf { it.length() }
    }

    fun getTotalStorageUsageBytes(context: Context): Long =
        getTotalStorageUsageBytes(context.filesDir)

    fun calculateSha256(file: File): String {
        val digest = MessageDigest.getInstance("SHA-256")
        FileInputStream(file).use { input ->
            val buffer = ByteArray(32 * 1024)
            var read: Int
            while (input.read(buffer).also { read = it } != -1) {
                digest.update(buffer, 0, read)
            }
        }
        return digest.digest().joinToString("") { "%02x".format(it) }
    }
}
