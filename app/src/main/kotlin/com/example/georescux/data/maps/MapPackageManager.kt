package com.example.georescux.data.maps

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.InputStream
import java.net.HttpURLConnection
import java.net.URL
import java.security.MessageDigest

/**
 * Manages offline map packages exclusively for the four official GeoRescuX states:
 * 1. Uttarakhand (Primary, bundled in APK)
 * 2. Himachal Pradesh (Secondary, downloadable package)
 * 3. Uttar Pradesh (Secondary, downloadable package)
 * 4. Haryana (Secondary, downloadable package)
 *
 * Enforces strict package integrity:
 * - Rejects non-Mapsforge files (e.g. raw OSM PBFs).
 * - Enforces independent trusted SHA-256 and size verification.
 * - Performs atomic installation using temporary files with rollback.
 * - Separates routing-graph availability from map-rendering availability.
 */
object MapPackageManager {

    const val MAPSFORGE_MAGIC = "mapsforge binary OSM"

    enum class PackageStatus(val label: String) {
        BUNDLED("Bundled (Ready)"),
        INSTALLED("Installed (Offline Ready)"),
        AVAILABLE_LOCAL("Ready to Install (Local)"),
        AVAILABLE_REMOTE("Available for Download"),
        UNAVAILABLE("Package Unavailable")
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
        val isMapInstalled: Boolean = false,
        val isGraphInstalled: Boolean = false,
    ) {
        val sizeFormatted: String
            get() {
                val mb = fileSizeBytes / (1024.0 * 1024.0)
                return String.format(java.util.Locale.US, "%.1f MB", mb)
            }

        val isFullyReady: Boolean
            get() = (isBundled || isMapInstalled) && isGraphInstalled
    }

    val OFFICIAL_PACKAGE_METADATA = listOf(
        StateMapPackage(
            regionId = "uttarakhand",
            displayName = "Uttarakhand",
            fileSizeBytes = 26437682L,
            sha256Checksum = "7397b4e7d92e46c897741fc5e837623746f1ac0abdb6d1facc979fb7d0d75778",
            isBundled = true,
            status = PackageStatus.BUNDLED,
            isMapInstalled = true,
            isGraphInstalled = true
        ),
        StateMapPackage(
            regionId = "himachal_pradesh",
            displayName = "Himachal Pradesh",
            fileSizeBytes = 18189967L,
            sha256Checksum = "eb80ba5856b5e57382386aa9ddf86c03bf7d4e268d53ce9f4ef3f931624bb012",
            isBundled = false,
            status = PackageStatus.UNAVAILABLE
        ),
        StateMapPackage(
            regionId = "haryana",
            displayName = "Haryana",
            fileSizeBytes = 37401197L,
            sha256Checksum = "34e9813089cef556427c561e74542b69f125ba9485fdc17977acecfe47dc389f",
            isBundled = false,
            status = PackageStatus.UNAVAILABLE
        ),
        StateMapPackage(
            regionId = "uttar_pradesh",
            displayName = "Uttar Pradesh",
            fileSizeBytes = 156576178L,
            sha256Checksum = "f7c995832817a1f3e48cc4dc0ae9b52149455f95577765f5532c747db5ff4d3c",
            isBundled = false,
            status = PackageStatus.UNAVAILABLE
        )
    )

    fun getPackageDirectory(filesDir: File, regionId: String): File {
        return File(filesDir, "maps/$regionId")
    }

    fun getPackageDirectory(context: Context, regionId: String): File =
        getPackageDirectory(context.filesDir, regionId)

    /**
     * Checks if a file has genuine Mapsforge binary map format header.
     * Mapsforge map files begin with the 20-byte magic ASCII string: "mapsforge binary OSM".
     */
    fun isMapsforgeFile(file: File): Boolean {
        if (!file.exists() || !file.isFile || file.length() < 20L) return false
        return try {
            FileInputStream(file).use { fis ->
                val buffer = ByteArray(20)
                val read = fis.read(buffer)
                read == 20 && buffer.toString(Charsets.US_ASCII) == MAPSFORGE_MAGIC
            }
        } catch (_: Exception) {
            false
        }
    }

    /**
     * Checks if a file has genuine SQLite database format header (for raster tile archives).
     */
    fun isSqliteArchiveFile(file: File): Boolean {
        if (!file.exists() || !file.isFile || file.length() < 16L) return false
        return try {
            FileInputStream(file).use { fis ->
                val buffer = ByteArray(16)
                val read = fis.read(buffer)
                read == 16 && buffer.toString(Charsets.US_ASCII).startsWith("SQLite format 3")
            }
        } catch (_: Exception) {
            false
        }
    }

    fun getOfficialPackages(filesDir: File, externalFilesDir: File? = null): List<StateMapPackage> {
        return OFFICIAL_PACKAGE_METADATA.map { meta ->
            val regionDir = getPackageDirectory(filesDir, meta.regionId)
            val stateMap = File(regionDir, "state.map")
            val isMapInstalled = (meta.isBundled) || (stateMap.exists() && isMapsforgeFile(stateMap))
            val graphFile = File(regionDir, "route_graph_${meta.regionId}")
            val isGraphInstalled = meta.isBundled || (graphFile.exists() && graphFile.length() > 0L)

            val localStagedSource = findCandidateSource(filesDir, externalFilesDir, meta.regionId)

            val status = when {
                meta.isBundled -> PackageStatus.BUNDLED
                isMapInstalled -> PackageStatus.INSTALLED
                localStagedSource != null -> PackageStatus.AVAILABLE_LOCAL
                meta.downloadUrl != null -> PackageStatus.AVAILABLE_REMOTE
                else -> PackageStatus.UNAVAILABLE
            }

            val installedFile = if (stateMap.exists() && isMapsforgeFile(stateMap)) stateMap else null

            meta.copy(
                status = status,
                localFilePath = installedFile?.absolutePath,
                storageUsageBytes = installedFile?.length() ?: 0L,
                isMapInstalled = isMapInstalled,
                isGraphInstalled = isGraphInstalled
            )
        }
    }

    fun getOfficialPackages(context: Context): List<StateMapPackage> =
        getOfficialPackages(context.filesDir, context.getExternalFilesDir(null))

    /**
     * Truthfully verifies whether genuine Mapsforge rendering map is installed.
     * Bundled regions (Uttarakhand) return true. Downloaded regions return true
     * only when state.map is confirmed to exist and possess the Mapsforge binary header.
     * PBF files NEVER count as an installed map.
     */
    fun isPackageInstalled(filesDir: File, regionId: String): Boolean {
        if (regionId == "uttarakhand") return true // Bundled in APK
        val regionDir = getPackageDirectory(filesDir, regionId)
        val stateMap = File(regionDir, "state.map")
        return stateMap.exists() && isMapsforgeFile(stateMap)
    }

    fun isPackageInstalled(context: Context, regionId: String): Boolean =
        isPackageInstalled(context.filesDir, regionId)

    /**
     * Locates a genuine Mapsforge .map source from trusted candidate paths.
     * OSM PBF files are never considered candidate map sources.
     */
    fun findCandidateSource(filesDir: File, externalFilesDir: File? = null, regionId: String): File? {
        val candidates = buildList {
            add(File(filesDir, "output/$regionId/state.map"))
            add(File(filesDir, "maps/$regionId/state.map"))
            if (externalFilesDir != null) {
                add(File(externalFilesDir, "output/$regionId/state.map"))
                add(File(externalFilesDir, "maps/$regionId/state.map"))
            }
            try {
                val downloads = android.os.Environment.getExternalStoragePublicDirectory(
                    android.os.Environment.DIRECTORY_DOWNLOADS
                )
                add(File(downloads, "georescux/$regionId/state.map"))
            } catch (_: Exception) {}
            add(File("/sdcard/Download/georescux/$regionId/state.map"))
            add(File("output/$regionId/state.map"))
        }
        return candidates.firstOrNull { it.exists() && isMapsforgeFile(it) }
    }

    /**
     * Installs and verifies a map package with atomic replacement, temporary file isolation,
     * and strict SHA-256 and magic-byte integrity validation.
     *
     * Existing valid packages are preserved if installation fails.
     */
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
        val backupFile = File(targetDir, "state.map.bak")

        try {
            if (targetTemp.exists()) targetTemp.delete()
            if (backupFile.exists()) backupFile.delete()
            onProgress?.invoke(5)

            // 1. Resolve source (custom, local candidate, or remote download)
            val sourceFile = when {
                customSourceFile != null -> {
                    if (!customSourceFile.exists()) {
                        return@withContext Result.failure(java.io.FileNotFoundException("Source file does not exist: ${customSourceFile.absolutePath}"))
                    }
                    if (!isMapsforgeFile(customSourceFile)) {
                        return@withContext Result.failure(IllegalArgumentException("Source file is not a valid Mapsforge binary map: missing '$MAPSFORGE_MAGIC' header"))
                    }
                    customSourceFile
                }
                else -> findCandidateSource(filesDir, externalFilesDir, regionId)
            }

            val expectedSize = meta.fileSizeBytes
            val expectedChecksum = meta.sha256Checksum

            if (sourceFile != null) {
                // Source must be verified as genuine Mapsforge map
                if (!isMapsforgeFile(sourceFile)) {
                    return@withContext Result.failure(IllegalArgumentException("Source file for $regionId is not a valid Mapsforge map"))
                }

                // Check size matches official metadata
                if (sourceFile.length() != expectedSize) {
                    return@withContext Result.failure(IllegalStateException("Source file size mismatch for $regionId: expected $expectedSize bytes, found ${sourceFile.length()} bytes"))
                }

                val totalBytes = sourceFile.length()
                var copiedBytes = 0L

                FileInputStream(sourceFile).use { input ->
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
            } else if (meta.downloadUrl != null) {
                // Real HTTPS streaming download
                val url = URL(meta.downloadUrl)
                val conn = (url.openConnection() as HttpURLConnection).apply {
                    connectTimeout = 15000
                    readTimeout = 30000
                    instanceFollowRedirects = true
                    setRequestProperty("User-Agent", "GeoRescuX/1.0 MapDownloader")
                }

                val responseCode = conn.responseCode
                if (responseCode !in 200..299) {
                    return@withContext Result.failure(java.io.IOException("HTTP error $responseCode downloading package from ${meta.downloadUrl}"))
                }

                val totalBytes = if (conn.contentLengthLong > 0) conn.contentLengthLong else expectedSize
                var copiedBytes = 0L

                conn.inputStream.use { input ->
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
            } else {
                return@withContext Result.failure(
                    java.io.FileNotFoundException(
                        "Package source not available for $regionId. No local package found in candidate storage paths, and no remote download URL configured."
                    )
                )
            }

            onProgress?.invoke(90)

            // 2. Validate extracted temp file integrity
            if (targetTemp.length() != expectedSize) {
                targetTemp.delete()
                return@withContext Result.failure(
                    IllegalStateException("Incomplete package: expected $expectedSize bytes, got ${targetTemp.length()} bytes")
                )
            }

            if (!isMapsforgeFile(targetTemp)) {
                targetTemp.delete()
                return@withContext Result.failure(
                    IllegalArgumentException("Corrupted or invalid map format: temp file missing '$MAPSFORGE_MAGIC' header")
                )
            }

            val checksum = calculateSha256(targetTemp)
            if (!checksum.equals(expectedChecksum, ignoreCase = true)) {
                targetTemp.delete()
                return@withContext Result.failure(
                    IllegalStateException("SHA-256 integrity verification failed for $regionId: expected $expectedChecksum, got $checksum")
                )
            }

            // 3. Atomic replacement with backup safeguard
            if (targetFile.exists()) {
                targetFile.renameTo(backupFile)
            }

            val renameOk = targetTemp.renameTo(targetFile)
            if (!renameOk) {
                targetTemp.copyTo(targetFile, overwrite = true)
                targetTemp.delete()
            }

            if (!targetFile.exists() || targetFile.length() != expectedSize) {
                // Restoration from backup if replacement failed
                if (backupFile.exists()) backupFile.renameTo(targetFile)
                return@withContext Result.failure(java.io.IOException("Failed to atomically install target map file"))
            }

            if (backupFile.exists()) backupFile.delete()

            // 4. Copy accompanying routing graph, places, and safe_havens if present in source directory
            val sourceDir = sourceFile?.parentFile
            if (sourceDir != null) {
                listOf(
                    File(sourceDir, "route_graph_$regionId"),
                    File(sourceDir, "graph.json"),
                ).firstOrNull { it.exists() && it.length() > 0L }?.copyTo(File(targetDir, "route_graph_$regionId"), overwrite = true)

                val placesCandidate = File(sourceDir, "places.json")
                if (placesCandidate.exists() && placesCandidate.length() > 0L) {
                    placesCandidate.copyTo(File(targetDir, "places.json"), overwrite = true)
                }

                val havensCandidate = File(sourceDir, "safe_havens.json")
                if (havensCandidate.exists() && havensCandidate.length() > 0L) {
                    havensCandidate.copyTo(File(targetDir, "safe_havens.json"), overwrite = true)
                }
            }

            // 5. Write verified manifest
            val manifestFile = File(targetDir, "manifest.json")
            manifestFile.writeText(
                """
                {
                  "regionId": "$regionId",
                  "displayName": "${meta.displayName}",
                  "packageType": "regional_map",
                  "renderingFormat": "Mapsforge MAP",
                  "renderingFile": "state.map",
                  "fileSizeBytes": ${targetFile.length()},
                  "sha256": "$checksum",
                  "status": "installed",
                  "verified": true,
                  "verifiedAt": "${java.text.SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'", java.util.Locale.US).apply { timeZone = java.util.TimeZone.getTimeZone("UTC") }.format(java.util.Date())}"
                }
                """.trimIndent()
            )

            onProgress?.invoke(100)
            Result.success(targetFile)
        } catch (e: Exception) {
            if (targetTemp.exists()) targetTemp.delete()
            if (backupFile.exists() && !targetFile.exists()) backupFile.renameTo(targetFile)
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
        if (!dir.exists()) return@withContext 0L
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
